/*
 * Copyright (C) 2026, Glookast LLC
 * All Rights Reserved.
 *
 * SMPTE ST 381-5:2023 HEVC OP1a Track
 */

#ifndef BMX_OP1A_HEVC_TRACK_H_
#define BMX_OP1A_HEVC_TRACK_H_

#include <bmx/mxf_op1a/OP1APictureTrack.h>
#include <bmx/mxf_helper/HEVCMXFDescriptorHelper.h>
#include <bmx/essence_parser/HEVCEssenceParser.h>
#include <bmx/writer_helper/HEVCWriterHelper.h>



namespace bmx
{


class OP1AHEVCTrack : public OP1APictureTrack
{
public:
    OP1AHEVCTrack(OP1AFile *file, uint32_t track_index, uint32_t track_id, uint8_t track_type_number,
                  mxfRational frame_rate, EssenceType essence_type);
    virtual ~OP1AHEVCTrack();

    void SetDecodingDelay(uint8_t delay);
    void SetClosedGOP(bool closed);
    void SetIdenticalGOP(bool identical);
    void SetMaxGOP(uint16_t max_gop);
    void SetMaxBPictureCount(uint16_t max_b);
    void SetCodedContentKind(uint8_t kind);
    void SetProfile(uint8_t profile);
    void SetProfileConstraint(uint16_t constraint);
    void SetLevel(uint8_t level);
    void SetTier(uint8_t tier);

public:
    virtual void PrepareWrite(uint8_t track_count);
    virtual void WriteSamplesInt(const unsigned char *data, uint32_t size, uint32_t num_samples);
    virtual void CompleteWrite();

private:
    HEVCMXFDescriptorHelper *mHEVCDescriptorHelper;
    HEVCEssenceParser mEssenceParser;
    HEVCWriterHelper mWriterHelper;
    int64_t mWrittenDuration;
    bool mFirstFrame;
};


};


#endif
