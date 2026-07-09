/*
 * Copyright (C) 2026, Glookast LLC
 * All Rights Reserved.
 *
 * SMPTE ST 381-5:2023 HEVC MXF Descriptor Helper
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <bmx/mxf_helper/HEVCMXFDescriptorHelper.h>
#include <bmx/essence_parser/HEVCEssenceParser.h>
#include <bmx/Utils.h>
#include <bmx/BMXException.h>
#include <bmx/Logging.h>

#include <mxf/mxf_labels_and_keys.h>

using namespace std;
using namespace bmx;
using namespace mxfpp;


typedef struct
{
    mxfUL pc_label;
    EssenceType essence_type;
} SupportedEssence;

static const SupportedEssence SUPPORTED_ESSENCE[] =
{
    {MXF_CMDEF_L(HEVC_MAIN_8),             HEVC_MAIN},
    {MXF_CMDEF_L(HEVC_MAIN_10),            HEVC_MAIN_10},
    {MXF_CMDEF_L(HEVC_MAIN_12),            HEVC_MAIN_12},
    {MXF_CMDEF_L(HEVC_MAIN_422_10),        HEVC_MAIN_422_10},
    {MXF_CMDEF_L(HEVC_MAIN_422_12),        HEVC_MAIN_422_12},
    {MXF_CMDEF_L(HEVC_MAIN_444),           HEVC_MAIN_444},
    {MXF_CMDEF_L(HEVC_MAIN_444_10),        HEVC_MAIN_444_10},
    {MXF_CMDEF_L(HEVC_MAIN_444_12),        HEVC_MAIN_444_12},
    {MXF_CMDEF_L(HEVC_MAIN_INTRA_8),       HEVC_MAIN_INTRA},
    {MXF_CMDEF_L(HEVC_MAIN_INTRA_10),      HEVC_MAIN_10_INTRA},
    {MXF_CMDEF_L(HEVC_MAIN_INTRA_12),      HEVC_MAIN_12_INTRA},
    {MXF_CMDEF_L(HEVC_MAIN_422_INTRA_10),  HEVC_MAIN_422_10_INTRA},
    {MXF_CMDEF_L(HEVC_MAIN_422_INTRA_12),  HEVC_MAIN_422_12_INTRA},
    {MXF_CMDEF_L(HEVC_MAIN_444_INTRA),     HEVC_MAIN_444_INTRA},
    {MXF_CMDEF_L(HEVC_MAIN_444_INTRA_10),  HEVC_MAIN_444_10_INTRA},
    {MXF_CMDEF_L(HEVC_MAIN_444_INTRA_12),  HEVC_MAIN_444_12_INTRA},
    {MXF_CMDEF_L(HEVC_MAIN_444_INTRA_16),  HEVC_MAIN_444_16_INTRA},
};


static bool is_hevc_ec(const mxfUL *label)
{
    // HEVC NAL unit stream: bytes 13-14 = 0x1f 0x60 (or any VideoStream SID)
    // HEVC byte stream:     bytes 13-14 = 0x20 0x60
    return mxf_is_generic_container_label(label) &&
           (label->octet13 == 0x1f || label->octet13 == 0x20);
}



EssenceType HEVCMXFDescriptorHelper::IsSupported(FileDescriptor *file_descriptor, mxfUL alternative_ec_label)
{
    mxfUL ec_label = file_descriptor->getEssenceContainer();
    if (!is_hevc_ec(&ec_label) && !is_hevc_ec(&alternative_ec_label))
        return UNKNOWN_ESSENCE_TYPE;

    GenericPictureEssenceDescriptor *pic_descriptor = dynamic_cast<GenericPictureEssenceDescriptor*>(file_descriptor);
    if (!pic_descriptor || !pic_descriptor->havePictureEssenceCoding())
        return UNKNOWN_ESSENCE_TYPE;

    mxfUL pc_label = pic_descriptor->getPictureEssenceCoding();
    size_t i;
    for (i = 0; i < BMX_ARRAY_SIZE(SUPPORTED_ESSENCE); i++) {
        if (mxf_equals_ul_mod_regver(&pc_label, &SUPPORTED_ESSENCE[i].pc_label))
            return SUPPORTED_ESSENCE[i].essence_type;
    }

    return UNKNOWN_ESSENCE_TYPE;
}

bool HEVCMXFDescriptorHelper::IsSupported(EssenceType essence_type)
{
    size_t i;
    for (i = 0; i < BMX_ARRAY_SIZE(SUPPORTED_ESSENCE); i++) {
        if (essence_type == SUPPORTED_ESSENCE[i].essence_type)
            return true;
    }

    return false;
}

HEVCMXFDescriptorHelper::HEVCMXFDescriptorHelper()
: PictureMXFDescriptorHelper()
{
    mEssenceIndex = 0;
    mEssenceType = SUPPORTED_ESSENCE[0].essence_type;
    mHEVCSubDescriptor = 0;
}

HEVCMXFDescriptorHelper::~HEVCMXFDescriptorHelper()
{
}

void HEVCMXFDescriptorHelper::Initialize(FileDescriptor *file_descriptor, uint16_t mxf_version,
                                          mxfUL alternative_ec_label)
{
    BMX_ASSERT(IsSupported(file_descriptor, alternative_ec_label));

    PictureMXFDescriptorHelper::Initialize(file_descriptor, mxf_version, alternative_ec_label);

    mxfUL ec_label = file_descriptor->getEssenceContainer();
    mFrameWrapped = true; // default; refine from EC label byte 16 if needed

    GenericPictureEssenceDescriptor *pic_descriptor = dynamic_cast<GenericPictureEssenceDescriptor*>(file_descriptor);
    mxfUL pc_label = pic_descriptor->getPictureEssenceCoding();
    size_t i;
    for (i = 0; i < BMX_ARRAY_SIZE(SUPPORTED_ESSENCE); i++) {
        if (mxf_equals_ul_mod_regver(&pc_label, &SUPPORTED_ESSENCE[i].pc_label))
        {
            mEssenceIndex = i;
            mEssenceType = SUPPORTED_ESSENCE[i].essence_type;
            break;
        }
    }

    if (file_descriptor->haveSubDescriptors()) {
        vector<SubDescriptor*> sub_descriptors = file_descriptor->getSubDescriptors();
        for (i = 0; i < sub_descriptors.size(); i++) {
            mHEVCSubDescriptor = dynamic_cast<HEVCSubDescriptor*>(sub_descriptors[i]);
            if (mHEVCSubDescriptor)
                break;
        }
    }
}

void HEVCMXFDescriptorHelper::SetEssenceType(EssenceType essence_type)
{
    BMX_ASSERT(!mFileDescriptor);

    PictureMXFDescriptorHelper::SetEssenceType(essence_type);

    UpdateEssenceIndex();
}

FileDescriptor* HEVCMXFDescriptorHelper::CreateFileDescriptor(mxfpp::HeaderMetadata *header_metadata)
{
    UpdateEssenceIndex();

    mFileDescriptor = new CDCIEssenceDescriptor(header_metadata);
    mHEVCSubDescriptor = new HEVCSubDescriptor(header_metadata);
    mFileDescriptor->appendSubDescriptors(mHEVCSubDescriptor);

    UpdateFileDescriptor();

    return mFileDescriptor;
}

void HEVCMXFDescriptorHelper::UpdateFileDescriptor()
{
    PictureMXFDescriptorHelper::UpdateFileDescriptor();

    CDCIEssenceDescriptor *cdci_descriptor = dynamic_cast<CDCIEssenceDescriptor*>(mFileDescriptor);
    BMX_ASSERT(cdci_descriptor);

    cdci_descriptor->setPictureEssenceCoding(SUPPORTED_ESSENCE[mEssenceIndex].pc_label);
}

void HEVCMXFDescriptorHelper::UpdateFileDescriptor(FileDescriptor *file_desc_in)
{
    CDCIEssenceDescriptor *cdci_descriptor = dynamic_cast<CDCIEssenceDescriptor*>(mFileDescriptor);
    BMX_ASSERT(cdci_descriptor);

    CDCIEssenceDescriptor *cdci_desc_in = dynamic_cast<CDCIEssenceDescriptor*>(file_desc_in);
    BMX_CHECK(cdci_desc_in);

#define SET_PROPERTY(name)                                                \
    if (cdci_desc_in->have##name() && !cdci_descriptor->have##name())     \
        cdci_descriptor->set##name(cdci_desc_in->get##name());

    SET_PROPERTY(SignalStandard)
    SET_PROPERTY(FrameLayout)
    SET_PROPERTY(StoredWidth)
    SET_PROPERTY(StoredHeight)
    SET_PROPERTY(DisplayWidth)
    SET_PROPERTY(DisplayHeight)
    SET_PROPERTY(AspectRatio)
    SET_PROPERTY(ActiveFormatDescriptor)
    SET_PROPERTY(VideoLineMap)
    SET_PROPERTY(FieldDominance)
    SET_PROPERTY(CaptureGamma)
    SET_PROPERTY(CodingEquations)
    SET_PROPERTY(ColorPrimaries)
    if (!cdci_descriptor->haveColorSiting() && cdci_desc_in->haveColorSiting())
        SetColorSitingMod(cdci_desc_in->getColorSiting());
    SET_PROPERTY(ComponentDepth)
    SET_PROPERTY(HorizontalSubsampling)
    SET_PROPERTY(VerticalSubsampling)
    SET_PROPERTY(BlackRefLevel)
    SET_PROPERTY(WhiteReflevel)
    SET_PROPERTY(ColorRange)
}

void HEVCMXFDescriptorHelper::UpdateFileDescriptor(HEVCEssenceParser *essence_parser)
{
    UpdateFileDescriptor();

    CDCIEssenceDescriptor *cdci_descriptor = dynamic_cast<CDCIEssenceDescriptor*>(mFileDescriptor);
    BMX_ASSERT(cdci_descriptor);

    uint32_t stored_width   = essence_parser->GetStoredWidth();
    uint32_t stored_height  = essence_parser->GetStoredHeight();
    uint32_t display_width  = essence_parser->GetDisplayWidth();
    uint32_t display_height = essence_parser->GetDisplayHeight();
    if (display_width == 0)
        display_width = stored_width;
    if (display_height == 0)
        display_height = stored_height;

    cdci_descriptor->setStoredWidth(stored_width);
    cdci_descriptor->setStoredHeight(stored_height);
    cdci_descriptor->setDisplayWidth(display_width);
    cdci_descriptor->setDisplayHeight(display_height);
    cdci_descriptor->setDisplayXOffset(0);
    cdci_descriptor->setDisplayYOffset(0);
    cdci_descriptor->setSampledWidth(stored_width);
    cdci_descriptor->setSampledHeight(stored_height);
    cdci_descriptor->setSampledXOffset(0);
    cdci_descriptor->setSampledYOffset(0);
    cdci_descriptor->setImageStartOffset(0);
    cdci_descriptor->setPaddingBits(0);

    if (essence_parser->GetComponentDepth() > 0)
        cdci_descriptor->setComponentDepth(essence_parser->GetComponentDepth());

    // Chroma subsampling from chroma_format_idc (ITU-T H.265 / SMPTE ST 381-5)
    switch (essence_parser->GetChromaFormat())
    {
        case 1: // 4:2:0
            cdci_descriptor->setHorizontalSubsampling(2);
            cdci_descriptor->setVerticalSubsampling(2);
            if (!cdci_descriptor->haveColorSiting())
                SetColorSitingMod(MXF_COLOR_SITING_VERT_MIDPOINT);
            break;
        case 2: // 4:2:2
            cdci_descriptor->setHorizontalSubsampling(2);
            cdci_descriptor->setVerticalSubsampling(1);
            if (!cdci_descriptor->haveColorSiting())
                SetColorSitingMod(MXF_COLOR_SITING_COSITING);
            break;
        case 3: // 4:4:4
            cdci_descriptor->setHorizontalSubsampling(1);
            cdci_descriptor->setVerticalSubsampling(1);
            if (!cdci_descriptor->haveColorSiting())
                SetColorSitingMod(MXF_COLOR_SITING_COSITING);
            break;
        case 0: // Monochrome
        default:
            break;
    }
}

mxfUL HEVCMXFDescriptorHelper::ChooseEssenceContainerUL() const
{
    if (mFrameWrapped)
        return MXF_EC_L(HEVCFrameWrapped);
    else
        return MXF_EC_L(HEVCClipWrapped);
}

void HEVCMXFDescriptorHelper::UpdateEssenceIndex()
{
    size_t i;
    for (i = 0; i < BMX_ARRAY_SIZE(SUPPORTED_ESSENCE); i++) {
        if (SUPPORTED_ESSENCE[i].essence_type == mEssenceType) {
            mEssenceIndex = i;
            break;
        }
    }
}
