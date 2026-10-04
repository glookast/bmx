/*
 * Copyright (C) 2010, British Broadcasting Corporation
 * All Rights Reserved.
 *
 * Author: Philip de Nier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the British Broadcasting Corporation nor the names
 *       of its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstring>

#include <bmx/mxf_helper/VC3MXFDescriptorHelper.h>
#include <bmx/Utils.h>
#include <bmx/BMXException.h>
#include <bmx/Logging.h>

#include <mxf/mxf_avid_labels_and_keys.h>

using namespace std;
using namespace bmx;
using namespace mxfpp;



typedef struct
{
    mxfUL pc_label;
    EssenceType essence_type;
    int32_t resolution_id;
    uint32_t component_depth;
    uint32_t horiz_subsampling;
    uint32_t frame_size;
    uint32_t stored_width;
    uint32_t stored_height;
    uint32_t display_width;
    uint32_t display_height;
    mxfVideoLineMap video_line_map;
    uint8_t frame_layout;
    uint8_t signal_standard;
    mxfUL avid_pc_label;
    mxfUL avid_ec_label;
} SupportedEssence;

static const SupportedEssence SUPPORTED_ESSENCE[] =
{
    {MXF_CMDEF_L(VC3_1080P_1235),  VC3_1080P_1235, 1235,   10,  2,  917504,  1920,   1080,   1920,   1080,   {42, 0},    MXF_FULL_FRAME,       MXF_SIGNAL_STANDARD_SMPTE274M,    MXF_CMDEF_L(DNxHD), MXF_EC_L(DNxHD1080p1235ClipWrapped)},
    {MXF_CMDEF_L(VC3_1080P_1237),  VC3_1080P_1237, 1237,   8,   2,  606208,  1920,   1080,   1920,   1080,   {42, 0},    MXF_FULL_FRAME,       MXF_SIGNAL_STANDARD_SMPTE274M,    MXF_CMDEF_L(DNxHD), MXF_EC_L(DNxHD1080p1237ClipWrapped)},
    {MXF_CMDEF_L(VC3_1080P_1238),  VC3_1080P_1238, 1238,   8,   2,  917504,  1920,   1080,   1920,   1080,   {42, 0},    MXF_FULL_FRAME,       MXF_SIGNAL_STANDARD_SMPTE274M,    MXF_CMDEF_L(DNxHD), MXF_EC_L(DNxHD1080p1238ClipWrapped)},
    {MXF_CMDEF_L(VC3_1080I_1241),  VC3_1080I_1241, 1241,   10,  2,  917504,  1920,   540,    1920,   540,    {21, 584},  MXF_SEPARATE_FIELDS,  MXF_SIGNAL_STANDARD_SMPTE274M,    MXF_CMDEF_L(DNxHD), MXF_EC_L(DNxHD1080i1241ClipWrapped)},
    {MXF_CMDEF_L(VC3_1080I_1242),  VC3_1080I_1242, 1242,   8,   2,  606208,  1920,   540,    1920,   540,    {21, 584},  MXF_SEPARATE_FIELDS,  MXF_SIGNAL_STANDARD_SMPTE274M,    MXF_CMDEF_L(DNxHD), MXF_EC_L(DNxHD1080i1242ClipWrapped)},
    {MXF_CMDEF_L(VC3_1080I_1243),  VC3_1080I_1243, 1243,   8,   2,  917504,  1920,   540,    1920,   540,    {21, 584},  MXF_SEPARATE_FIELDS,  MXF_SIGNAL_STANDARD_SMPTE274M,    MXF_CMDEF_L(DNxHD), MXF_EC_L(DNxHD1080i1243ClipWrapped)},
    {MXF_CMDEF_L(VC3_1080I_1244),  VC3_1080I_1244, 1244,   8,   2,  606208,  1440,   540,    1920,   540,    {21, 584},  MXF_SEPARATE_FIELDS,  MXF_SIGNAL_STANDARD_SMPTE274M,    MXF_CMDEF_L(DNxHD), MXF_EC_L(DNxHD1080i1244ClipWrapped)},
    {MXF_CMDEF_L(VC3_720P_1250),   VC3_720P_1250,  1250,   10,  2,  458752,  1280,   720,    1280,   720,    {26, 0},    MXF_FULL_FRAME,       MXF_SIGNAL_STANDARD_SMPTE296M,    MXF_CMDEF_L(DNxHD), MXF_EC_L(DNxHD720p1250ClipWrapped)},
    {MXF_CMDEF_L(VC3_720P_1251),   VC3_720P_1251,  1251,   8,   2,  458752,  1280,   720,    1280,   720,    {26, 0},    MXF_FULL_FRAME,       MXF_SIGNAL_STANDARD_SMPTE296M,    MXF_CMDEF_L(DNxHD), MXF_EC_L(DNxHD720p1251ClipWrapped)},
    {MXF_CMDEF_L(VC3_720P_1252),   VC3_720P_1252,  1252,   8,   2,  303104,  1280,   720,    1280,   720,    {26, 0},    MXF_FULL_FRAME,       MXF_SIGNAL_STANDARD_SMPTE296M,    MXF_CMDEF_L(DNxHD), MXF_EC_L(DNxHD720p1252ClipWrapped)},
    {MXF_CMDEF_L(VC3_1080P_1253),  VC3_1080P_1253, 1253,   8,   2,  188416,  1920,   1080,   1920,   1080,   {42, 0},    MXF_FULL_FRAME,       MXF_SIGNAL_STANDARD_SMPTE274M,    MXF_CMDEF_L(DNxHD), MXF_EC_L(DNxHD1080p1253ClipWrapped)},
    {MXF_CMDEF_L(VC3_720P_1258),   VC3_720P_1258,  1258,   8,   2,  212992,  960,    720,    1280,   720,    {26, 0},    MXF_FULL_FRAME,       MXF_SIGNAL_STANDARD_SMPTE296M,    MXF_CMDEF_L(DNxHD), MXF_EC_L(DNxHD720p1258ClipWrapped)},
    {MXF_CMDEF_L(VC3_1080P_1259),  VC3_1080P_1259, 1259,   8,   2,  417792,  1440,   1080,   1920,   1080,   {42, 0},    MXF_FULL_FRAME,       MXF_SIGNAL_STANDARD_SMPTE274M,    MXF_CMDEF_L(DNxHD), MXF_EC_L(DNxHD1080p1259ClipWrapped)},
    {MXF_CMDEF_L(VC3_1080I_1260),  VC3_1080I_1260, 1260,   8,   2,  417792,  1440,   540,    1920,   540,    {21, 584},  MXF_SEPARATE_FIELDS,  MXF_SIGNAL_STANDARD_SMPTE274M,    MXF_CMDEF_L(DNxHD), MXF_EC_L(DNxHD1080i1260ClipWrapped)},
    // DNxHD 444: RGB 4:4:4, so an RGBA descriptor (see Is444). Avid defines no OP-Atom labels for it.
    {MXF_CMDEF_L(VC3_1080P_1256),  VC3_1080P_1256, 1256,   10,  1,  1835008, 1920,   1080,   1920,   1080,   {42, 0},    MXF_FULL_FRAME,       MXF_SIGNAL_STANDARD_SMPTE274M,    g_Null_UL,          g_Null_UL},
};


// GKX (GKX-122): VC-3 / DNxHR resolution-independent essence.
//
// DNxHR is resolution-INDEPENDENT: the same compression ID is used at any raster,
// so the descriptor geometry (stored/display width+height, frame_layout, component
// depth, video_line_map) comes from the SOURCE raster supplied by the caller, and
// only the PictureEssenceCoding UL byte + the constant frame size are keyed by the
// profile. These labels/sizes are a faithful port of the proven mxflib legacy
// analyzer_vc3::set_info_dnxhr (the reason this fork work exists -- BMX 1.6 has only
// the fixed-raster DNxHD IDs 1235-1260 and cannot wrap 1270-1274).
//
// avid_resolution_id column: the Avid compression id (1270-1274). horiz_subsampling
// column: 444 -> 1 (4:4:4), all others -> 2 (4:2:2). component_depth column is the
// profile-nominal depth (444/HQX = 10-bit, HQ/SQ/LB = 8-bit) but the actual written
// depth honours the source raster's real bit depth (see UpdateFileDescriptorRI).
// packet_scale column: the profile's bytes per macroblock, times 255 (see GetRIFrameSize).
typedef struct
{
    mxfUL pc_label;
    EssenceType essence_type;
    int32_t resolution_id;
    uint32_t nominal_component_depth;
    uint32_t horiz_subsampling;
    uint32_t packet_scale;
} SupportedRIEssence;

static const SupportedRIEssence SUPPORTED_RI_ESSENCE[] =
{
    {MXF_CMDEF_L(VC3_DNXHR_444),  VC3_DNXHR_444,  1270,  10,  1,  57344},
    {MXF_CMDEF_L(VC3_DNXHR_HQX),  VC3_DNXHR_HQX,  1271,  10,  2,  28672},
    {MXF_CMDEF_L(VC3_DNXHR_HQ),   VC3_DNXHR_HQ,   1272,  8,   2,  28672},
    {MXF_CMDEF_L(VC3_DNXHR_SQ),   VC3_DNXHR_SQ,   1273,  8,   2,  18944},
    {MXF_CMDEF_L(VC3_DNXHR_LB),   VC3_DNXHR_LB,   1274,  8,   2,  5888},
};


// The size of a resolution-independent VC-3 frame at a raster, as the codec defines it (FFmpeg's
// avpriv_dnxhd_get_hr_frame_size): the macroblock count times the profile's packet scale, rounded to
// 4096 bytes and at least 8192. It replaces a table of four widths at their usual heights (GKX-122),
// which refused any other width and was short of the real frame at 2048 wide (MPS-1135).
static uint32_t get_ri_frame_size(uint32_t packet_scale, uint32_t width, uint32_t height)
{
    uint64_t size = (uint64_t)((height + 15) / 16) * ((width + 15) / 16) * packet_scale / 255;
    size = (size + 2048) / 4096 * 4096;
    return (uint32_t)(size < 8192 ? 8192 : size);
}



EssenceType VC3MXFDescriptorHelper::IsSupported(FileDescriptor *file_descriptor, mxfUL alternative_ec_label)
{
    mxfUL ec_label = file_descriptor->getEssenceContainer();
    if (!CompareECULs(ec_label, alternative_ec_label, MXF_EC_L(VC3FrameWrapped)) &&
        !CompareECULs(ec_label, alternative_ec_label, MXF_EC_L(VC3ClipWrapped)))
    {
        size_t index;
        if (IsAvidDNxHD(file_descriptor, alternative_ec_label, &index))
            return SUPPORTED_ESSENCE[index].essence_type;

        return UNKNOWN_ESSENCE_TYPE;
    }

    GenericPictureEssenceDescriptor *pic_descriptor = dynamic_cast<GenericPictureEssenceDescriptor*>(file_descriptor);
    if (!pic_descriptor || !pic_descriptor->havePictureEssenceCoding())
        return UNKNOWN_ESSENCE_TYPE;

    mxfUL pc_label = pic_descriptor->getPictureEssenceCoding();
    size_t i;
    for (i = 0; i < BMX_ARRAY_SIZE(SUPPORTED_ESSENCE); i++) {
        if (mxf_equals_ul_mod_regver(&pc_label, &SUPPORTED_ESSENCE[i].pc_label))
            return SUPPORTED_ESSENCE[i].essence_type;
    }

    // GKX (GKX-122): resolution-independent DNxHR (1270-1274)
    for (i = 0; i < BMX_ARRAY_SIZE(SUPPORTED_RI_ESSENCE); i++) {
        if (mxf_equals_ul_mod_regver(&pc_label, &SUPPORTED_RI_ESSENCE[i].pc_label))
            return SUPPORTED_RI_ESSENCE[i].essence_type;
    }

    return UNKNOWN_ESSENCE_TYPE;
}

bool VC3MXFDescriptorHelper::IsSupported(EssenceType essence_type)
{
    size_t i;
    for (i = 0; i < BMX_ARRAY_SIZE(SUPPORTED_ESSENCE); i++) {
        if (essence_type == SUPPORTED_ESSENCE[i].essence_type)
            return true;
    }

    return IsDNxHR(essence_type);
}

EssenceType VC3MXFDescriptorHelper::GetEssenceType(int32_t resolution_id)
{
    size_t i;
    for (i = 0; i < BMX_ARRAY_SIZE(SUPPORTED_ESSENCE); i++) {
        if (SUPPORTED_ESSENCE[i].resolution_id == resolution_id)
            return SUPPORTED_ESSENCE[i].essence_type;
    }
    for (i = 0; i < BMX_ARRAY_SIZE(SUPPORTED_RI_ESSENCE); i++) {
        if (SUPPORTED_RI_ESSENCE[i].resolution_id == resolution_id)
            return SUPPORTED_RI_ESSENCE[i].essence_type;
    }

    return UNKNOWN_ESSENCE_TYPE;
}

bool VC3MXFDescriptorHelper::IsDNxHR(EssenceType essence_type)
{
    size_t i;
    for (i = 0; i < BMX_ARRAY_SIZE(SUPPORTED_RI_ESSENCE); i++) {
        if (essence_type == SUPPORTED_RI_ESSENCE[i].essence_type)
            return true;
    }

    return false;
}

bool VC3MXFDescriptorHelper::IsAvidDNxHD(FileDescriptor *file_descriptor, mxfUL alternative_ec_label, size_t *index)
{
    size_t i;
    for (i = 0; i < BMX_ARRAY_SIZE(SUPPORTED_ESSENCE); i++) {
        // an ID with no Avid label (1256) must not match a file that has no alternative label either
        if (!mxf_equals_ul(&SUPPORTED_ESSENCE[i].avid_ec_label, &g_Null_UL) &&
            mxf_equals_ul_mod_regver(&alternative_ec_label, &SUPPORTED_ESSENCE[i].avid_ec_label))
        {
            break;
        }
    }
    if (i >= BMX_ARRAY_SIZE(SUPPORTED_ESSENCE))
        return false;

    GenericPictureEssenceDescriptor *pic_descriptor = dynamic_cast<GenericPictureEssenceDescriptor*>(file_descriptor);
    if (!pic_descriptor || !pic_descriptor->havePictureEssenceCoding())
        return false;
    mxfUL pc_label = pic_descriptor->getPictureEssenceCoding();

    bool result = mxf_equals_ul_mod_regver(&pc_label, &SUPPORTED_ESSENCE[i].avid_pc_label);
    if (result)
        *index = i;

    return result;
}

VC3MXFDescriptorHelper::VC3MXFDescriptorHelper()
: PictureMXFDescriptorHelper()
{
    mEssenceIndex = 0;
    mEssenceType = SUPPORTED_ESSENCE[0].essence_type;
    mIsRI = false;
    mRIStoredWidth = 0;
    mRIStoredHeight = 0;
    mRIComponentDepth = 0;
    mRIInterlaced = false;
    mRIRasterSet = false;
}

VC3MXFDescriptorHelper::~VC3MXFDescriptorHelper()
{
}

void VC3MXFDescriptorHelper::Initialize(FileDescriptor *file_descriptor, uint16_t mxf_version,
                                        mxfUL alternative_ec_label)
{
    BMX_ASSERT(IsSupported(file_descriptor, alternative_ec_label));

    PictureMXFDescriptorHelper::Initialize(file_descriptor, mxf_version, alternative_ec_label);

    if (IsAvidDNxHD(file_descriptor, alternative_ec_label, &mEssenceIndex)) {
        mFrameWrapped = false;
        mEssenceType = SUPPORTED_ESSENCE[mEssenceIndex].essence_type;
        mAvidResolutionId = SUPPORTED_ESSENCE[mEssenceIndex].resolution_id;
    } else {
        mxfUL ec_label = file_descriptor->getEssenceContainer();
        mFrameWrapped = CompareECULs(ec_label, alternative_ec_label, MXF_EC_L(VC3FrameWrapped));

        GenericPictureEssenceDescriptor *pic_descriptor = dynamic_cast<GenericPictureEssenceDescriptor*>(file_descriptor);
        mxfUL pc_label = pic_descriptor->getPictureEssenceCoding();
        size_t i;
        bool found = false;
        for (i = 0; i < BMX_ARRAY_SIZE(SUPPORTED_ESSENCE); i++) {
            if (mxf_equals_ul_mod_regver(&pc_label, &SUPPORTED_ESSENCE[i].pc_label)) {
                mEssenceIndex = i;
                mEssenceType = SUPPORTED_ESSENCE[i].essence_type;
                mAvidResolutionId = SUPPORTED_ESSENCE[i].resolution_id;
                found = true;
                break;
            }
        }

        // GKX (GKX-122): resolution-independent DNxHR (1270-1274). Geometry is read
        // back from the descriptor itself (not a table), since the raster is not
        // encoded in the compression ID.
        if (!found) {
            for (i = 0; i < BMX_ARRAY_SIZE(SUPPORTED_RI_ESSENCE); i++) {
                if (mxf_equals_ul_mod_regver(&pc_label, &SUPPORTED_RI_ESSENCE[i].pc_label)) {
                    mIsRI = true;
                    mEssenceIndex = i;
                    mEssenceType = SUPPORTED_RI_ESSENCE[i].essence_type;
                    mAvidResolutionId = SUPPORTED_RI_ESSENCE[i].resolution_id;

                    GenericPictureEssenceDescriptor *gp =
                        dynamic_cast<GenericPictureEssenceDescriptor*>(file_descriptor);
                    if (gp) {
                        if (gp->haveStoredWidth())
                            mRIStoredWidth = gp->getStoredWidth();
                        if (gp->haveStoredHeight())
                            mRIStoredHeight = gp->getStoredHeight();
                        if (gp->haveFrameLayout())
                            mRIInterlaced = (gp->getFrameLayout() == MXF_SEPARATE_FIELDS ||
                                             gp->getFrameLayout() == MXF_MIXED_FIELDS ||
                                             gp->getFrameLayout() == MXF_SEGMENTED_FRAME);
                    }
                    // GKX (GKX-122): bit depth is recovered per descriptor type -- the
                    // 4:2:2 profiles are CDCI (ComponentDepth item); 444 is RGBA, which
                    // has no ComponentDepth in bmx's typed model, so the depth is read
                    // back from the PixelLayout component depths (the RGBA-idiomatic home
                    // it was written to in UpdateFileDescriptorRI).
                    CDCIEssenceDescriptor *cdci = dynamic_cast<CDCIEssenceDescriptor*>(file_descriptor);
                    RGBAEssenceDescriptor *rgba = dynamic_cast<RGBAEssenceDescriptor*>(file_descriptor);
                    if (cdci && cdci->haveComponentDepth()) {
                        mRIComponentDepth = cdci->getComponentDepth();
                    } else if (rgba && rgba->havePixelLayout()) {
                        mxfRGBALayout pixel_layout = rgba->getPixelLayout();
                        mRIComponentDepth = pixel_layout.components[0].depth;
                    }
                    mRIRasterSet = (mRIStoredWidth != 0 && mRIStoredHeight != 0);
                    break;
                }
            }
        }
    }
}

void VC3MXFDescriptorHelper::SetEssenceType(EssenceType essence_type)
{
    BMX_ASSERT(!mFileDescriptor);

    // GKX (GKX-122): resolution-independent DNxHR (1270-1274).
    if (IsDNxHR(essence_type)) {
        size_t i;
        for (i = 0; i < BMX_ARRAY_SIZE(SUPPORTED_RI_ESSENCE); i++) {
            if (SUPPORTED_RI_ESSENCE[i].essence_type == essence_type) {
                mIsRI = true;
                mEssenceIndex = i;
                mAvidResolutionId = SUPPORTED_RI_ESSENCE[i].resolution_id;
                break;
            }
        }
        BMX_CHECK(i < BMX_ARRAY_SIZE(SUPPORTED_RI_ESSENCE));

        PictureMXFDescriptorHelper::SetEssenceType(essence_type);
        return;
    }

    size_t i;
    for (i = 0; i < BMX_ARRAY_SIZE(SUPPORTED_ESSENCE); i++) {
        if (SUPPORTED_ESSENCE[i].essence_type == essence_type) {
            mEssenceIndex = i;
            mAvidResolutionId = SUPPORTED_ESSENCE[i].resolution_id;
            break;
        }
    }
    BMX_CHECK(i < BMX_ARRAY_SIZE(SUPPORTED_ESSENCE));

    PictureMXFDescriptorHelper::SetEssenceType(essence_type);
}

void VC3MXFDescriptorHelper::SetRIRaster(uint32_t stored_width, uint32_t stored_height,
                                         uint32_t component_depth, bool is_interlaced)
{
    mRIStoredWidth = stored_width;
    mRIStoredHeight = stored_height;
    mRIComponentDepth = component_depth;
    mRIInterlaced = is_interlaced;
    mRIRasterSet = true;
}

bool VC3MXFDescriptorHelper::Is444() const
{
    if (mIsRI)
        return SUPPORTED_RI_ESSENCE[mEssenceIndex].horiz_subsampling == 1;
    return SUPPORTED_ESSENCE[mEssenceIndex].horiz_subsampling == 1;
}

FileDescriptor* VC3MXFDescriptorHelper::CreateFileDescriptor(mxfpp::HeaderMetadata *header_metadata)
{
    // GKX (GKX-122): DNxHR 444 is an RGB-space 4:4:4 codec and MUST be written as an
    // RGBAEssenceDescriptor (a faithful port of the mxflib legacy analyzer_vc3, which
    // creates RGBAEssenceDescriptor_UL for the 444 case and CDCIEssenceDescriptor_UL
    // for everything else). DNxHD 444 (1256) is RGB 4:4:4 too. All the other profiles
    // (DNxHD 1235-1260 and the four DNxHR 4:2:2 profiles HQX/HQ/SQ/LB) stay CDCI.
    if (Is444())
        mFileDescriptor = new RGBAEssenceDescriptor(header_metadata);
    else
        mFileDescriptor = new CDCIEssenceDescriptor(header_metadata);
    UpdateFileDescriptor();
    return mFileDescriptor;
}

void VC3MXFDescriptorHelper::UpdateFileDescriptor()
{
    // GKX (GKX-122): resolution-independent DNxHR takes a separate path -- its
    // geometry comes from the caller-supplied raster, not the SUPPORTED_ESSENCE table.
    if (mIsRI) {
        UpdateFileDescriptorRI();
        return;
    }

    PictureMXFDescriptorHelper::UpdateFileDescriptor();

    GenericPictureEssenceDescriptor *pic_descriptor = dynamic_cast<GenericPictureEssenceDescriptor*>(mFileDescriptor);
    BMX_ASSERT(pic_descriptor);
    CDCIEssenceDescriptor *cdci_descriptor = dynamic_cast<CDCIEssenceDescriptor*>(mFileDescriptor);
    BMX_ASSERT(cdci_descriptor || Is444());

    if ((mFlavour & MXFDESC_AVID_FLAVOUR) && !mxf_equals_ul(&SUPPORTED_ESSENCE[mEssenceIndex].avid_pc_label, &g_Null_UL))
        pic_descriptor->setPictureEssenceCoding(SUPPORTED_ESSENCE[mEssenceIndex].avid_pc_label);
    else
        pic_descriptor->setPictureEssenceCoding(SUPPORTED_ESSENCE[mEssenceIndex].pc_label);
    pic_descriptor->setSignalStandard(SUPPORTED_ESSENCE[mEssenceIndex].signal_standard);
    pic_descriptor->setFrameLayout(SUPPORTED_ESSENCE[mEssenceIndex].frame_layout);
    pic_descriptor->setStoredWidth(SUPPORTED_ESSENCE[mEssenceIndex].stored_width);
    pic_descriptor->setStoredHeight(SUPPORTED_ESSENCE[mEssenceIndex].stored_height);
    pic_descriptor->setDisplayWidth(SUPPORTED_ESSENCE[mEssenceIndex].display_width);
    pic_descriptor->setDisplayHeight(SUPPORTED_ESSENCE[mEssenceIndex].display_height);
    pic_descriptor->setSampledWidth(pic_descriptor->getDisplayWidth());
    pic_descriptor->setSampledHeight(pic_descriptor->getDisplayHeight());
    if ((mFlavour & MXFDESC_AVID_FLAVOUR)) {
        pic_descriptor->setSampledXOffset(0);
        pic_descriptor->setSampledYOffset(0);
        pic_descriptor->setDisplayXOffset(0);
        pic_descriptor->setDisplayYOffset(0);
    }
    pic_descriptor->setVideoLineMap(SUPPORTED_ESSENCE[mEssenceIndex].video_line_map);
    if ((mFlavour & MXFDESC_AVID_FLAVOUR))
        pic_descriptor->setImageAlignmentOffset(8192);

    if (cdci_descriptor) {
        UpdateCDCIDefaults(SUPPORTED_ESSENCE[mEssenceIndex].component_depth);
        cdci_descriptor->setHorizontalSubsampling(SUPPORTED_ESSENCE[mEssenceIndex].horiz_subsampling);
        cdci_descriptor->setVerticalSubsampling(1);
    } else {
        UpdateRGBADescriptor(SUPPORTED_ESSENCE[mEssenceIndex].component_depth);
    }
}

// MPS-1135: the profile's colour defaults, which a value the caller set wins over. The base class
// has already written the caller's values; writing BT.709 coding equations over them labelled a
// BT.2020 picture BT.709.
void VC3MXFDescriptorHelper::UpdateCDCIDefaults(uint32_t component_depth)
{
    CDCIEssenceDescriptor *cdci_descriptor = dynamic_cast<CDCIEssenceDescriptor*>(mFileDescriptor);
    BMX_ASSERT(cdci_descriptor);

    if (!BMX_OPT_PROP_IS_SET(mColorSiting))
        SetColorSitingMod(MXF_COLOR_SITING_REC601);
    cdci_descriptor->setComponentDepth(component_depth);
    // Video range at the picture's depth: 16, 235 and 225 levels scaled by 2^(depth - 8), as 64 / 940 /
    // 897 at 10 bits (a 12-bit picture was given the 8-bit levels).
    const uint32_t shift = component_depth > 8 ? component_depth - 8 : 0;
    if (!BMX_OPT_PROP_IS_SET(mBlackRefLevel))
        cdci_descriptor->setBlackRefLevel(16u << shift);
    if (!BMX_OPT_PROP_IS_SET(mWhiteRefLevel))
        cdci_descriptor->setWhiteReflevel(235u << shift);
    if (!BMX_OPT_PROP_IS_SET(mColorRange))
        cdci_descriptor->setColorRange((224u << shift) + 1);
    if (!BMX_OPT_PROP_IS_SET(mCodingEquations))
        SetCodingEquationsMod(ITUR_BT709_CODING_EQ);
}

// GKX (GKX-122): DNx 4:4:4 is RGB. The legacy mxflib analyzer's 444 case is a MINIMAL
// RGBAEssenceDescriptor -- the common picture fields plus the per-component bit depth,
// with no ColorSiting, reference levels or subsampling (all CDCI-only).
//
// NOTE (bmx API divergence from the mxflib reference): ComponentDepth is a CDCI-only
// property in bmx's typed model (RGBAEssenceDescriptor has no ComponentDepth item), so
// the bit depth is carried in the PixelLayout component depths, as bmx's own RGBA
// helpers (UncRGBA/JPEG2000/JPEGXS) do.
void VC3MXFDescriptorHelper::UpdateRGBADescriptor(uint32_t component_depth)
{
    RGBAEssenceDescriptor *rgba_descriptor = dynamic_cast<RGBAEssenceDescriptor*>(mFileDescriptor);
    BMX_ASSERT(rgba_descriptor);

    mxfRGBALayout pixel_layout;
    for (int i = 0; i < 8; i++) {
        pixel_layout.components[i].code = 0;
        pixel_layout.components[i].depth = 0;
    }
    pixel_layout.components[0].code = 'R';
    pixel_layout.components[0].depth = (uint8_t)component_depth;
    pixel_layout.components[1].code = 'G';
    pixel_layout.components[1].depth = (uint8_t)component_depth;
    pixel_layout.components[2].code = 'B';
    pixel_layout.components[2].depth = (uint8_t)component_depth;
    rgba_descriptor->setPixelLayout(pixel_layout);
}

// GKX (GKX-122): resolution-independent DNxHR descriptor. Faithful port of
// analyzer_vc3::set_info_dnxhr: geometry (stored/display width+height, frame_layout,
// video_line_map, component_depth) comes from the caller-supplied source raster;
// only the PictureEssenceCoding UL byte and the constant frame size are keyed by the
// profile. horiz_subsampling: 444 -> 1 (4:4:4), all others -> 2 (4:2:2).
void VC3MXFDescriptorHelper::UpdateFileDescriptorRI()
{
    PictureMXFDescriptorHelper::UpdateFileDescriptor();

    // GKX (GKX-122): the 444 profile is RGBA, all other RI profiles are CDCI (see
    // CreateFileDescriptor). Recover the concrete picture descriptor accordingly.
    GenericPictureEssenceDescriptor *pic_descriptor =
        dynamic_cast<GenericPictureEssenceDescriptor*>(mFileDescriptor);
    BMX_ASSERT(pic_descriptor);
    CDCIEssenceDescriptor *cdci_descriptor = dynamic_cast<CDCIEssenceDescriptor*>(mFileDescriptor);
    RGBAEssenceDescriptor *rgba_descriptor = dynamic_cast<RGBAEssenceDescriptor*>(mFileDescriptor);

    BMX_CHECK_M(mRIRasterSet && mRIStoredWidth != 0 && mRIStoredHeight != 0,
                ("DNxHR resolution-independent essence requires the source raster to be set "
                 "(SetRIRaster) before creating the descriptor"));

    const SupportedRIEssence &ri = SUPPORTED_RI_ESSENCE[mEssenceIndex];

    // Component depth: honour the source's actual bit depth when supplied, else fall
    // back to the profile-nominal depth (444/HQX = 10-bit, HQ/SQ/LB = 8-bit).
    uint32_t component_depth = (mRIComponentDepth != 0) ? mRIComponentDepth : ri.nominal_component_depth;

    // Fields common to both descriptor types. Faithful port of the mxflib legacy
    // analyzer_vc3::build_descriptor, which writes SampleRate/FrameLayout/geometry/
    // VideoLineMap/ComponentDepth/PictureEssenceCoding on BOTH the CDCI and the RGBA
    // (444) descriptor.
    pic_descriptor->setPictureEssenceCoding(ri.pc_label);
    pic_descriptor->setSignalStandard(MXF_SIGNAL_STANDARD_NONE);

    mxfVideoLineMap video_line_map;
    if (mRIInterlaced) {
        pic_descriptor->setFrameLayout(MXF_SEPARATE_FIELDS);
        video_line_map.first = 21;
        video_line_map.second = 584;
    } else {
        pic_descriptor->setFrameLayout(MXF_FULL_FRAME);
        video_line_map.first = 42;
        video_line_map.second = 0;
    }
    pic_descriptor->setVideoLineMap(video_line_map);

    // The frame height, interlaced or not, as GKX-122 and the mxflib analyzer_vc3 wrote it: files exist
    // with it, and nothing produces an interlaced resolution-independent picture (the Avid SDK refuses one).
    pic_descriptor->setStoredWidth(mRIStoredWidth);
    pic_descriptor->setStoredHeight(mRIStoredHeight);
    pic_descriptor->setDisplayWidth(mRIStoredWidth);
    pic_descriptor->setDisplayHeight(mRIStoredHeight);
    pic_descriptor->setSampledWidth(mRIStoredWidth);
    pic_descriptor->setSampledHeight(mRIStoredHeight);
    if ((mFlavour & MXFDESC_AVID_FLAVOUR)) {
        pic_descriptor->setSampledXOffset(0);
        pic_descriptor->setSampledYOffset(0);
        pic_descriptor->setDisplayXOffset(0);
        pic_descriptor->setDisplayYOffset(0);
    }
    if ((mFlavour & MXFDESC_AVID_FLAVOUR))
        pic_descriptor->setImageAlignmentOffset(8192);

    if (rgba_descriptor) {
        UpdateRGBADescriptor(component_depth);
    } else {
        BMX_ASSERT(cdci_descriptor);

        UpdateCDCIDefaults(component_depth);
        cdci_descriptor->setHorizontalSubsampling(ri.horiz_subsampling);
        cdci_descriptor->setVerticalSubsampling(1);
    }
}

uint32_t VC3MXFDescriptorHelper::GetRIFrameSize() const
{
    BMX_CHECK_M(mRIStoredWidth != 0 && mRIStoredHeight != 0,
                ("DNxHR resolution-independent frame size requested before the source raster was set"));
    return get_ri_frame_size(SUPPORTED_RI_ESSENCE[mEssenceIndex].packet_scale, mRIStoredWidth, mRIStoredHeight);
}

uint32_t VC3MXFDescriptorHelper::GetSampleSize()
{
    if (mIsRI)
        return GetRIFrameSize();

    return SUPPORTED_ESSENCE[mEssenceIndex].frame_size;
}

mxfUL VC3MXFDescriptorHelper::ChooseEssenceContainerUL() const
{
    // GKX (GKX-122): DNxHR RI is only wrapped as plain VC3 frame/clip (no Avid
    // OP-Atom EC labels are defined for the RI IDs), so ignore the Avid flavour.
    if (!mIsRI && (mFlavour & MXFDESC_AVID_FLAVOUR)) {
        BMX_ASSERT(!mFrameWrapped);
        BMX_CHECK_M(!mxf_equals_ul(&SUPPORTED_ESSENCE[mEssenceIndex].avid_ec_label, &g_Null_UL),
                    ("Avid defines no essence container for %s", essence_type_to_string(mEssenceType)));
        return SUPPORTED_ESSENCE[mEssenceIndex].avid_ec_label;
    } else {
        if (mFrameWrapped)
            return MXF_EC_L(VC3FrameWrapped);
        else
            return MXF_EC_L(VC3ClipWrapped);
    }
}

