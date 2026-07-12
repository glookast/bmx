/*
 * Copyright (C) 2026, Glookast LLC
 * All Rights Reserved.
 *
 * SMPTE ST 381-5:2023 HEVC OP1a Track
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <bmx/mxf_op1a/OP1AHEVCTrack.h>
#include <bmx/mxf_op1a/OP1AFile.h>
#include <bmx/BMXException.h>
#include <bmx/Logging.h>

using namespace std;
using namespace bmx;
using namespace mxfpp;


static const mxfKey VIDEO_ELEMENT_KEY = MXF_MPEG_PICT_EE_K(0x01, MXF_MPEG_PICT_FRAME_WRAPPED_EE_TYPE, 0x00);



OP1AHEVCTrack::OP1AHEVCTrack(OP1AFile *file, uint32_t track_index, uint32_t track_id, uint8_t track_type_number,
                               mxfRational frame_rate, EssenceType essence_type)
: OP1APictureTrack(file, track_index, track_id, track_type_number, frame_rate, essence_type)
{
    mTrackNumber = MXF_MPEG_PICT_TRACK_NUM(0x01, MXF_MPEG_PICT_FRAME_WRAPPED_EE_TYPE, 0x00);
    mEssenceElementKey = VIDEO_ELEMENT_KEY;
    mHEVCDescriptorHelper = dynamic_cast<HEVCMXFDescriptorHelper*>(mDescriptorHelper);
    mWriterHelper.SetDescriptorHelper(mHEVCDescriptorHelper);
    mWrittenDuration = 0;
    mFirstFrame = true;
}

OP1AHEVCTrack::~OP1AHEVCTrack()
{
}

void OP1AHEVCTrack::SetDecodingDelay(uint8_t delay)
{
    BMX_ASSERT(mHEVCDescriptorHelper);
    mHEVCDescriptorHelper->GetHEVCSubDescriptor()->setHEVCDecodingDelay(delay);
}

void OP1AHEVCTrack::SetClosedGOP(bool closed)
{
    BMX_ASSERT(mHEVCDescriptorHelper);
    mHEVCDescriptorHelper->GetHEVCSubDescriptor()->setHEVCClosedGOPIndicator(closed);
}

void OP1AHEVCTrack::SetIdenticalGOP(bool identical)
{
    BMX_ASSERT(mHEVCDescriptorHelper);
    mHEVCDescriptorHelper->GetHEVCSubDescriptor()->setHEVCIdenticalGOPIndicator(identical);
}

void OP1AHEVCTrack::SetMaxGOP(uint16_t max_gop)
{
    BMX_ASSERT(mHEVCDescriptorHelper);
    mHEVCDescriptorHelper->GetHEVCSubDescriptor()->setHEVCMaximumGOPSize(max_gop);
}

void OP1AHEVCTrack::SetMaxBPictureCount(uint16_t max_b)
{
    BMX_ASSERT(mHEVCDescriptorHelper);
    mHEVCDescriptorHelper->GetHEVCSubDescriptor()->setHEVCMaximumBPictureCount(max_b);
}

void OP1AHEVCTrack::SetCodedContentKind(uint8_t kind)
{
    BMX_ASSERT(mHEVCDescriptorHelper);
    mHEVCDescriptorHelper->GetHEVCSubDescriptor()->setHEVCCodedContentKind(kind);
}

void OP1AHEVCTrack::SetProfile(uint8_t profile)
{
    BMX_ASSERT(mHEVCDescriptorHelper);
    mHEVCDescriptorHelper->GetHEVCSubDescriptor()->setHEVCProfile(profile);
}

void OP1AHEVCTrack::SetProfileConstraint(uint16_t constraint)
{
    BMX_ASSERT(mHEVCDescriptorHelper);
    mHEVCDescriptorHelper->GetHEVCSubDescriptor()->setHEVCProfileConstraint(constraint);
}

void OP1AHEVCTrack::SetLevel(uint8_t level)
{
    BMX_ASSERT(mHEVCDescriptorHelper);
    mHEVCDescriptorHelper->GetHEVCSubDescriptor()->setHEVCLevel(level);
}

void OP1AHEVCTrack::SetTier(uint8_t tier)
{
    BMX_ASSERT(mHEVCDescriptorHelper);
    mHEVCDescriptorHelper->GetHEVCSubDescriptor()->setHEVCTier(tier);
}

void OP1AHEVCTrack::PrepareWrite(uint8_t track_count)
{
    CompleteEssenceKeyAndTrackNum(track_count);

    mCPManager->RegisterPictureTrackElement(mTrackIndex, mEssenceElementKey, false);
    // enable index reordering (temporal/key-frame offsets) for long-GOP HEVC
    mIndexTable->RegisterPictureTrackElement(mTrackIndex, false, true);
}

void OP1AHEVCTrack::WriteSamplesInt(const unsigned char *data, uint32_t size, uint32_t num_samples)
{
    BMX_CHECK(num_samples == 1);
    BMX_CHECK(data && size);

    // First-frame descriptor population (geometry, colour, profile/level/tier) from the SPS.
    mEssenceParser.ParseFrameInfo(data, size);

    if (mFirstFrame && mEssenceParser.HaveSequenceParameterSet()) {
        mFirstFrame = false;
        BMX_ASSERT(mHEVCDescriptorHelper);

        // Populate the CDCI picture geometry (dimensions, component depth, subsampling) from the
        // parsed SPS. Without this the file descriptor has StoredWidth/StoredHeight == 0 and no
        // NLE (Avid, Premiere) can open the output.
        mHEVCDescriptorHelper->UpdateFileDescriptor(&mEssenceParser);

        mxfpp::HEVCSubDescriptor *sub = mHEVCDescriptorHelper->GetHEVCSubDescriptor();
        sub->setHEVCProfile(mEssenceParser.GetProfile());
        sub->setHEVCLevel(mEssenceParser.GetLevel());
        sub->setHEVCTier(mEssenceParser.GetTier());
    }

    // Reorder index bookkeeping: derive temporal/key-frame offsets from picture order count so
    // long-GOP (B-frame) HEVC gets a correct index for index-driven readers such as Avid.
    mWriterHelper.ProcessFrame(data, size);

    bool require_update = true;
    int64_t position = -1;
    int8_t temporal_offset = 0;
    int8_t key_frame_offset = 0;
    uint8_t flags = 0;
    MPEGFrameType frame_type = UNKNOWN_FRAME_TYPE;
    while (mWriterHelper.TakeCompleteIndexEntry(&position, &temporal_offset, &key_frame_offset, &flags, &frame_type)) {
        if (position == mWriterHelper.GetFramePosition()) {
            require_update = false;
            break;
        }
        mIndexTable->UpdateIndexEntry(mTrackIndex, position, temporal_offset, key_frame_offset, flags);
    }
    if (require_update)
        mWriterHelper.GetIncompleteIndexEntry(&position, &temporal_offset, &key_frame_offset, &flags, &frame_type);

    mCPManager->WriteSamples(mTrackIndex, data, size, num_samples);
    mIndexTable->AddIndexEntry(mTrackIndex, position, temporal_offset, key_frame_offset, flags,
                               frame_type == I_FRAME, require_update);
    mWrittenDuration++;
}

void OP1AHEVCTrack::CompleteWrite()
{
    mWriterHelper.CompleteProcess();

    int64_t position;
    int8_t temporal_offset;
    int8_t key_frame_offset;
    uint8_t flags;
    MPEGFrameType frame_type;
    while (mWriterHelper.TakeCompleteIndexEntry(&position, &temporal_offset, &key_frame_offset, &flags, &frame_type))
        mIndexTable->UpdateIndexEntry(mTrackIndex, position, temporal_offset, key_frame_offset, flags);
}
