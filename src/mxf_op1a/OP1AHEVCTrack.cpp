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
    mIndexTable->RegisterPictureTrackElement(mTrackIndex, false, false);
}

void OP1AHEVCTrack::WriteSamplesInt(const unsigned char *data, uint32_t size, uint32_t num_samples)
{
    BMX_CHECK(num_samples == 1);
    BMX_CHECK(data && size);

    mEssenceParser.ParseFrameInfo(data, size);

    MPEGFrameType frame_type = mEssenceParser.GetFrameType();
    bool is_key_frame = mEssenceParser.IsIDRFrame() || mEssenceParser.IsCRAFrame() ||
                        frame_type == I_FRAME;

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

    uint8_t flags = 0;
    if (is_key_frame)
        flags |= 0x80;

    mCPManager->WriteSamples(mTrackIndex, data, size, num_samples);
    mIndexTable->AddIndexEntry(mTrackIndex, mWrittenDuration, 0, 0, flags, is_key_frame, false);
    mWrittenDuration++;
}

void OP1AHEVCTrack::CompleteWrite()
{
}
