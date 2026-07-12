/*
 * Copyright (C) 2026, Glookast LLC
 * All Rights Reserved.
 *
 * HEVC OP1a writer helper — see HEVCWriterHelper.h.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdio>

#include <bmx/writer_helper/HEVCWriterHelper.h>
#include <bmx/BMXException.h>
#include <bmx/Logging.h>

using namespace std;
using namespace bmx;
using namespace mxfpp;


// MaxDpbFrames is limited to a maximum of 16
#define MAX_DPB_FRAMES  16


HEVCWriterHelper::IndexedFrame::IndexedFrame()
{
    is_complete = false;
    is_decoded = false;
    position = 0;
    frame_type = UNKNOWN_FRAME_TYPE;
    pic_order_cnt = 0;
    decoded_frame_offset = 0;
    key_frame_offset = 0;
    temporal_offset = 0;
    flags = 0;
}


HEVCWriterHelper::HEVCWriterHelper()
{
    mDescriptorHelper = 0;
    mPosition = 0;
    mPrevPicOrderCntLsb = 0;
    mPrevPicOrderCntMsb = 0;
    mHavePrevPOC = false;
    mNextIndexedDecodedPos = 0;
    mNextIndexedPos = 0;
    mKeyFramePosition = -1;
    mIDRKeyFramePosition = -1;
    mDecodingDelay = 0;
    mBPictureCount = 0;
    mMaxBPictureCount = 0;
    mClosedGOP = true;
    mGOPStartPosition = 0;
    mUnlimitedGOPSize = false;
    mMaxGOP = 0;
    mStartsWithIFrame = true;
    mIdenticalGOP = true;
    mFirstGOP = true;
}

HEVCWriterHelper::~HEVCWriterHelper()
{
}

void HEVCWriterHelper::SetDescriptorHelper(HEVCMXFDescriptorHelper *descriptor_helper)
{
    mDescriptorHelper = descriptor_helper;
}

int32_t HEVCWriterHelper::DecodePOC()
{
    // H.265 8.3.1. IDR pictures have POC 0 and reset the reference POC state.
    if (mEssenceParser.IsIDRFrame()) {
        mPrevPicOrderCntLsb = 0;
        mPrevPicOrderCntMsb = 0;
        mHavePrevPOC = true;
        return 0;
    }

    uint8_t log2_max = mEssenceParser.GetLog2MaxPicOrderCntLsb();
    int32_t max_lsb = (log2_max > 0 && log2_max < 31) ? (1 << log2_max) : 0;
    int32_t poc_lsb = (int32_t)mEssenceParser.GetSlicePicOrderCntLsb();

    int32_t poc_msb;
    if (!mHavePrevPOC || max_lsb == 0) {
        poc_msb = 0;
    } else {
        int32_t prev_lsb = mPrevPicOrderCntLsb;
        int32_t prev_msb = mPrevPicOrderCntMsb;
        if (poc_lsb < prev_lsb && (prev_lsb - poc_lsb) >= (max_lsb / 2))
            poc_msb = prev_msb + max_lsb;
        else if (poc_lsb > prev_lsb && (poc_lsb - prev_lsb) > (max_lsb / 2))
            poc_msb = prev_msb - max_lsb;
        else
            poc_msb = prev_msb;
    }

    int32_t poc = poc_msb + poc_lsb;

    // Update prevTid0Pic state: base sub-layer (TemporalId 0), not RASL/RADL, not a
    // sub-layer non-reference picture (the even VCL nal types 0..14).
    uint8_t nal_type = mEssenceParser.GetNalUnitType();
    bool is_rasl = (nal_type == HEVC_RASL_N || nal_type == HEVC_RASL_R);
    bool is_radl = (nal_type == HEVC_RADL_N || nal_type == HEVC_RADL_R);
    bool is_sub_layer_non_ref = (nal_type <= HEVC_RSV_VCL_N14 && (nal_type % 2) == 0);
    if (mEssenceParser.GetTemporalId() == 0 && !is_rasl && !is_radl && !is_sub_layer_non_ref) {
        mPrevPicOrderCntLsb = poc_lsb;
        mPrevPicOrderCntMsb = poc_msb;
        mHavePrevPOC = true;
    }

    return poc;
}

void HEVCWriterHelper::ProcessFrame(const unsigned char *data, uint32_t size)
{
    mEssenceParser.ParseFrameInfo(data, size);

    int32_t pic_order_cnt = DecodePOC();

    MPEGFrameType frame_type = mEssenceParser.GetFrameType();
    if (frame_type == UNKNOWN_FRAME_TYPE)
        frame_type = I_FRAME; // conservative fallback so an unparsed slice never breaks indexing

    bool gop_start = (frame_type == I_FRAME);

    if (frame_type == B_FRAME) {
        mBPictureCount++;
        if (mBPictureCount > mMaxBPictureCount)
            mMaxBPictureCount = mBPictureCount;
    } else if (mBPictureCount > 0) {
        mBPictureCount = 0;
    }

    // Note that there is a matching calculation in CompleteProcess.
    if (gop_start && !mUnlimitedGOPSize) {
        int64_t gop_size = mPosition - mGOPStartPosition;
        if (gop_size > UINT16_MAX) {
            mUnlimitedGOPSize = true;
            mMaxGOP = 0;
        } else if (gop_size > mMaxGOP) {
            mMaxGOP = (uint16_t)gop_size;
        }
    }

    if (gop_start && !mEssenceParser.IsIDRFrame())
        mClosedGOP = false;

    if (mPosition == 0 && !gop_start)
        mStartsWithIFrame = false;
    if (!mStartsWithIFrame && gop_start && mIdenticalGOP)
        mIdenticalGOP = false;

    if (mIdenticalGOP) {
        if (mFirstGOP && gop_start && mPosition > 0)
            mFirstGOP = false;

        if (mFirstGOP) {
            mGOPStructure.push_back(frame_type);
            if (mGOPStructure.size() >= 4096)
                mIdenticalGOP = false;
        } else {
            size_t pos_in_gop = (gop_start ? 0 : (size_t)(mPosition - mGOPStartPosition));
            if (pos_in_gop >= mGOPStructure.size() || mGOPStructure[pos_in_gop] != frame_type)
                mIdenticalGOP = false;
        }
    }

    uint8_t flags = 0x00;
    if (mEssenceParser.IsIDRFrame())
        flags |= 1 << 7; // random access bit
    // prediction directions (naive; refined below via key_frame_offset for B-frames)
    if (frame_type == I_FRAME)
        flags |= 0 << 4;
    else if (frame_type == P_FRAME)
        flags |= 2 << 4;
    else
        flags |= 3 << 4;
    if (mEssenceParser.IsIDRFrame())
        flags |= 1 << 2;
    if (frame_type == I_FRAME)
        flags |= 0;
    else if (frame_type == P_FRAME)
        flags |= 2;
    else
        flags |= 3;

    if (mEssenceParser.IsIDRFrame()) {
        PopAllDecodedFrames();
    } else {
        while (mDecodedFrames.size() > MAX_DPB_FRAMES)
            PopDecodedFrame();
    }

    IndexedFrame indexed_frame;
    indexed_frame.position      = mPosition;
    indexed_frame.pic_order_cnt = pic_order_cnt;
    indexed_frame.frame_type    = frame_type;
    indexed_frame.flags         = flags;
    mIndexedCodedFrames[mPosition] = indexed_frame;

    // Map display order (POC) -> coded position. A well-formed stream has a unique POC per
    // picture within the DPB window; nudge on the rare collision rather than aborting.
    int32_t poc_key = pic_order_cnt;
    while (mDecodedFrames.count(poc_key))
        poc_key++;
    mDecodedFrames[poc_key] = mPosition;

    if (mEssenceParser.IsIDRFrame())
        mIDRKeyFramePosition = mPosition;

    if (frame_type == I_FRAME)
        mGOPStartPosition = mPosition;
    mPosition++;
}

void HEVCWriterHelper::CompleteProcess()
{
    PopAllDecodedFrames();

    if (!mUnlimitedGOPSize) {
        int64_t gop_size = mPosition - mGOPStartPosition;
        if (gop_size > UINT16_MAX) {
            mUnlimitedGOPSize = true;
            mMaxGOP = 0;
        } else if (gop_size > mMaxGOP) {
            mMaxGOP = (uint16_t)gop_size;
        }
    }

    if (!mDescriptorHelper)
        return;

    CDCIEssenceDescriptor *cdci_descriptor =
        dynamic_cast<CDCIEssenceDescriptor*>(mDescriptorHelper->GetFileDescriptor());
    if (cdci_descriptor) {
        // HEVC OP1a frame-wrapped essence is progressive full-frame.
        if (!cdci_descriptor->haveFrameLayout())
            cdci_descriptor->setFrameLayout(MXF_FULL_FRAME);
        if (!cdci_descriptor->haveVideoLineMap())
            cdci_descriptor->setVideoLineMap(1, 0);
    }

    HEVCSubDescriptor *sub = mDescriptorHelper->GetHEVCSubDescriptor();
    if (sub) {
        if (mMaxBPictureCount > 0)
            sub->setHEVCDecodingDelay(mDecodingDelay);
        else
            sub->setHEVCDecodingDelay(0);
        sub->setHEVCClosedGOPIndicator(mClosedGOP);
        sub->setHEVCIdenticalGOPIndicator(mIdenticalGOP);
        sub->setHEVCMaximumGOPSize(mMaxGOP);
        sub->setHEVCMaximumBPictureCount(mMaxBPictureCount);
    }
}

bool HEVCWriterHelper::TakeCompleteIndexEntry(int64_t *position, int8_t *temporal_offset, int8_t *key_frame_offset,
                                              uint8_t *flags, MPEGFrameType *frame_type)
{
    if (mCompleteIndexedFrames.empty())
        return false;

    IndexedFrame complete_index = mCompleteIndexedFrames.front();
    mCompleteIndexedFrames.pop();

    SetIndexResult(complete_index, position, temporal_offset, key_frame_offset, flags, frame_type);

    return true;
}

void HEVCWriterHelper::GetIncompleteIndexEntry(int64_t *position, int8_t *temporal_offset, int8_t *key_frame_offset,
                                               uint8_t *flags, MPEGFrameType *frame_type)
{
    int64_t current_pos = GetFramePosition();
    IndexedFrame incomplete_index;
    if (mIndexedCodedFrames.count(current_pos))
        incomplete_index = mIndexedCodedFrames[current_pos];
    else if (mIndexedDecodedFrames.count(current_pos))
        incomplete_index = mIndexedDecodedFrames[current_pos];
    else
        incomplete_index = mIncompleteIndexedFrames[current_pos];

    SetIndexResult(incomplete_index, position, temporal_offset, key_frame_offset, flags, frame_type);
}

void HEVCWriterHelper::SetIndexResult(const IndexedFrame &indexed_frame, int64_t *position, int8_t *temporal_offset,
                                      int8_t *key_frame_offset, uint8_t *flags, MPEGFrameType *frame_type)
{
    *position = indexed_frame.position;
    *flags = indexed_frame.flags;
    if (indexed_frame.temporal_offset < INT8_MIN || indexed_frame.temporal_offset > INT8_MAX) {
        if (indexed_frame.temporal_offset < INT8_MIN)
            *temporal_offset = INT8_MIN;
        else
            *temporal_offset = INT8_MAX;
        *flags |= 1 << 3;
    } else {
        *temporal_offset = (int8_t)indexed_frame.temporal_offset;
    }
    if (indexed_frame.key_frame_offset < INT8_MIN) {
        *key_frame_offset = INT8_MIN;
        *flags |= 1 << 3;
    } else {
        *key_frame_offset = (int8_t)indexed_frame.key_frame_offset;
    }
    *frame_type = indexed_frame.frame_type;
}

void HEVCWriterHelper::PopAllDecodedFrames()
{
    while (!mDecodedFrames.empty())
        PopDecodedFrame();
}

void HEVCWriterHelper::PopDecodedFrame()
{
    int64_t decoded_pos = mPosition - (int64_t)mDecodedFrames.size();
    int64_t coded_pos   = mDecodedFrames.begin()->second;

    int64_t decoding_delay = coded_pos - decoded_pos;
    if (decoding_delay > (int64_t)mDecodingDelay)
        mDecodingDelay = (uint8_t)decoding_delay;

    mIndexedDecodedFrames[coded_pos] = mIndexedCodedFrames[coded_pos];
    mIndexedCodedFrames.erase(coded_pos);
    IndexedFrame &indexed_dec_frame = mIndexedDecodedFrames[coded_pos];
    if (indexed_dec_frame.frame_type == I_FRAME)
    {
        indexed_dec_frame.key_frame_offset = 0;
        mKeyFramePosition = coded_pos;
    }
    else if (mKeyFramePosition >= 0)
    {
        indexed_dec_frame.key_frame_offset = mKeyFramePosition - coded_pos;
    }
    else if (mIDRKeyFramePosition >= 0 && mIDRKeyFramePosition <= coded_pos &&
             (mKeyFramePosition < 0 || mIDRKeyFramePosition >= mKeyFramePosition))
    {
        indexed_dec_frame.key_frame_offset = mIDRKeyFramePosition - coded_pos;
        if (indexed_dec_frame.frame_type == B_FRAME)
            indexed_dec_frame.flags &= 0xdf;
    }
    else
    {
        indexed_dec_frame.key_frame_offset = (int64_t)INT32_MIN - 1;
    }
    indexed_dec_frame.decoded_frame_offset = decoded_pos - coded_pos;
    if (indexed_dec_frame.decoded_frame_offset == 0)
        indexed_dec_frame.is_complete = true;
    indexed_dec_frame.is_decoded = true;

    while (!mIndexedDecodedFrames.empty() &&
            mIndexedDecodedFrames.begin()->first == mNextIndexedDecodedPos &&
            mIndexedDecodedFrames.begin()->second.is_decoded)
    {
        if (mIncompleteIndexedFrames.count(mNextIndexedDecodedPos)) {
            IndexedFrame &forward_frame = mIncompleteIndexedFrames[mNextIndexedDecodedPos];
            mIndexedDecodedFrames[mNextIndexedDecodedPos].temporal_offset = forward_frame.temporal_offset;
            mIndexedDecodedFrames[mNextIndexedDecodedPos].is_complete     = true;
        }
        mIncompleteIndexedFrames[mNextIndexedDecodedPos] = mIndexedDecodedFrames[mNextIndexedDecodedPos];
        mIndexedDecodedFrames.erase(mIndexedDecodedFrames.begin());

        IndexedFrame &indexed_frame = mIncompleteIndexedFrames[mNextIndexedDecodedPos];
        if (indexed_frame.decoded_frame_offset != 0) {
            int64_t decode_pos = mNextIndexedDecodedPos + indexed_frame.decoded_frame_offset;
            if (mIncompleteIndexedFrames.count(decode_pos)) {
                mIncompleteIndexedFrames[decode_pos].temporal_offset = - indexed_frame.decoded_frame_offset;
                mIncompleteIndexedFrames[decode_pos].is_complete     = true;
            } else {
                IndexedFrame forward_frame;
                forward_frame.temporal_offset = - indexed_frame.decoded_frame_offset;
                mIncompleteIndexedFrames[decode_pos] = forward_frame;
            }
        }

        mNextIndexedDecodedPos++;
    }

    while (!mIncompleteIndexedFrames.empty() &&
            mIncompleteIndexedFrames.begin()->first == mNextIndexedPos &&
            mIncompleteIndexedFrames.begin()->second.is_complete)
    {
        IndexedFrame &indexed_frame = mIncompleteIndexedFrames.begin()->second;
        if (indexed_frame.frame_type == I_FRAME && indexed_frame.temporal_offset == 0)
            indexed_frame.flags |= 1 << 7; // random access bit

        mCompleteIndexedFrames.push(indexed_frame);
        mIncompleteIndexedFrames.erase(mIncompleteIndexedFrames.begin());

        mNextIndexedPos++;
    }

    mDecodedFrames.erase(mDecodedFrames.begin());
}
