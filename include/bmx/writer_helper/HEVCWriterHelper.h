/*
 * Copyright (C) 2026, Glookast LLC
 * All Rights Reserved.
 *
 * HEVC OP1a writer helper: computes the MXF reorder index table (temporal and
 * key-frame offsets) for long-GOP HEVC from per-frame Picture Order Count.
 * Modelled on AVCWriterHelper; HEVC is frame-only (no PAFF/MBAFF).
 */

#ifndef BMX_HEVC_WRITER_HELPER_H_
#define BMX_HEVC_WRITER_HELPER_H_

#include <map>
#include <queue>
#include <vector>

#include <bmx/essence_parser/HEVCEssenceParser.h>
#include <bmx/mxf_helper/HEVCMXFDescriptorHelper.h>


namespace bmx
{


class HEVCWriterHelper
{
public:
    HEVCWriterHelper();
    ~HEVCWriterHelper();

    void SetDescriptorHelper(HEVCMXFDescriptorHelper *descriptor_helper);

    void ProcessFrame(const unsigned char *data, uint32_t size);
    void CompleteProcess();

public:
    int64_t GetFramePosition() const { return mPosition - 1; }

    bool TakeCompleteIndexEntry(int64_t *position, int8_t *temporal_offset, int8_t *key_frame_offset,
                                uint8_t *flags, MPEGFrameType *frame_type);
    void GetIncompleteIndexEntry(int64_t *position, int8_t *temporal_offset, int8_t *key_frame_offset,
                                 uint8_t *flags, MPEGFrameType *frame_type);

private:
    class IndexedFrame
    {
    public:
        IndexedFrame();

    public:
        bool is_complete;
        bool is_decoded;
        int64_t position;
        MPEGFrameType frame_type;
        int32_t pic_order_cnt;
        int64_t decoded_frame_offset;
        int64_t key_frame_offset;
        int64_t temporal_offset;
        uint8_t flags;
    };

private:
    int32_t DecodePOC();
    void SetIndexResult(const IndexedFrame &indexed_frame, int64_t *position, int8_t *temporal_offset,
                        int8_t *key_frame_offset, uint8_t *flags, MPEGFrameType *frame_type);
    void PopAllDecodedFrames();
    void PopDecodedFrame();

private:
    HEVCMXFDescriptorHelper *mDescriptorHelper;
    HEVCEssenceParser mEssenceParser;
    int64_t mPosition;

    // POC decode state (H.265 8.3.1)
    int32_t mPrevPicOrderCntLsb;
    int32_t mPrevPicOrderCntMsb;
    bool mHavePrevPOC;

    std::map<int64_t, IndexedFrame> mIndexedCodedFrames;
    std::map<int32_t, int64_t> mDecodedFrames;   // pic_order_cnt -> coded position
    std::map<int64_t, IndexedFrame> mIndexedDecodedFrames;
    std::map<int64_t, IndexedFrame> mIncompleteIndexedFrames;
    std::queue<IndexedFrame> mCompleteIndexedFrames;
    int64_t mNextIndexedDecodedPos;
    int64_t mNextIndexedPos;
    int64_t mKeyFramePosition;
    int64_t mIDRKeyFramePosition;

    uint8_t mDecodingDelay;
    uint16_t mBPictureCount;
    uint16_t mMaxBPictureCount;
    bool mClosedGOP;
    int64_t mGOPStartPosition;
    bool mUnlimitedGOPSize;
    uint16_t mMaxGOP;
    bool mStartsWithIFrame;
    bool mIdenticalGOP;
    bool mFirstGOP;
    std::vector<int> mGOPStructure;
};


};


#endif
