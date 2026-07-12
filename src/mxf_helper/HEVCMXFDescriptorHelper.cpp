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

void HEVCMXFDescriptorHelper::MapColorPrimaries(uint8_t hevc_value)
{
    CDCIEssenceDescriptor *cdci_descriptor = dynamic_cast<CDCIEssenceDescriptor*>(mFileDescriptor);
    BMX_ASSERT(cdci_descriptor);

    switch (hevc_value) {
        case 1:  cdci_descriptor->setColorPrimaries(ITU709_COLOR_PRIM);     break;
        case 4:
        case 5:  cdci_descriptor->setColorPrimaries(ITU470_PAL_COLOR_PRIM); break;
        case 6:
        case 7:  cdci_descriptor->setColorPrimaries(SMPTE170M_COLOR_PRIM);  break;
        case 9:  cdci_descriptor->setColorPrimaries(ITU2020_COLOR_PRIM);    break;
        case 10: cdci_descriptor->setColorPrimaries(SMPTE_DCDM_COLOR_PRIM); break;
        default: break;
    }
}

void HEVCMXFDescriptorHelper::MapTransferCharacteristic(uint8_t hevc_value)
{
    CDCIEssenceDescriptor *cdci_descriptor = dynamic_cast<CDCIEssenceDescriptor*>(mFileDescriptor);
    BMX_ASSERT(cdci_descriptor);

    switch (hevc_value) {
        case 1:
        case 6:  cdci_descriptor->setCaptureGamma(ITUR_BT709_TRANSFER_CH);       break;
        case 4:
        case 5:  cdci_descriptor->setCaptureGamma(ITUR_BT470_TRANSFER_CH);       break;
        case 7:  cdci_descriptor->setCaptureGamma(SMPTE240M_TRANSFER_CH);        break;
        case 8:  cdci_descriptor->setCaptureGamma(LINEAR_TRANSFER_CH);           break;
        case 11: cdci_descriptor->setCaptureGamma(IEC6196624_XVYCC_TRANSFER_CH); break;
        case 12: cdci_descriptor->setCaptureGamma(ITU1361_TRANSFER_CH);          break;
        case 14:
        case 15: cdci_descriptor->setCaptureGamma(ITU2020_TRANSFER_CH);          break;
        case 16: cdci_descriptor->setCaptureGamma(SMPTE_ST2084_TRANSFER_CH);     break;
        case 17: cdci_descriptor->setCaptureGamma(SMPTE_DCDM_TRANSFER_CH);       break;
        case 18: cdci_descriptor->setCaptureGamma(HLG_OETF_TRANSFER_CH);         break;
        default: break;
    }
}

void HEVCMXFDescriptorHelper::MapMatrixCoefficients(uint8_t hevc_value)
{
    switch (hevc_value) {
        case 0: SetCodingEquationsMod(GBR_CODING_EQ);         break;
        case 1: SetCodingEquationsMod(ITUR_BT709_CODING_EQ);  break;
        case 5:
        case 6: SetCodingEquationsMod(ITUR_BT601_CODING_EQ);  break;
        case 7: SetCodingEquationsMod(SMPTE_240M_CODING_EQ);  break;
        case 8: SetCodingEquationsMod(Y_CG_CO_CODING_EQ);     break;
        case 9: SetCodingEquationsMod(ITU2020_NCL_CODING_EQ); break;
        default: break;
    }
}

void HEVCMXFDescriptorHelper::UpdateFileDescriptor(HEVCEssenceParser *essence_parser)
{
    // Single source of truth: re-derive the essence profile from the SPS's reliably-parsed bit
    // depth and chroma format so the PictureEssenceCoding UL matches the real bitstream, overriding
    // any profile the caller guessed from a (possibly stale or unpopulated) source descriptor.
    // We deliberately do NOT use HEVCEssenceParser::GetEssenceType(): it maps every Range-Extensions
    // stream (general_profile_idc == 4) to an Intra profile because the parser does not read the
    // RExt general_intra_constraint_flag, which would mislabel a non-intra 4:2:2/4:4:4 stream as
    // Intra (an Intra-profile decoder rejects inter frames). Choosing the non-intra variant is
    // correct for inter streams and still decodable for intra-only ones.
    if (essence_parser->HaveSequenceParameterSet()) {
        uint32_t depth = essence_parser->GetComponentDepth();
        EssenceType derived = mEssenceType;
        switch (essence_parser->GetChromaFormat()) {
            case 3: // 4:4:4
                derived = (depth >= 12) ? HEVC_MAIN_444_12 : (depth >= 10 ? HEVC_MAIN_444_10 : HEVC_MAIN_444);
                break;
            case 2: // 4:2:2 (no 8-bit 4:2:2 profile exists; Main 4:2:2 10 is the minimum)
                derived = (depth >= 12) ? HEVC_MAIN_422_12 : HEVC_MAIN_422_10;
                break;
            default: // 4:2:0 and monochrome
                derived = (depth >= 12) ? HEVC_MAIN_12 : (depth >= 10 ? HEVC_MAIN_10 : HEVC_MAIN);
                break;
        }
        if (IsSupported(derived)) {
            mEssenceType = derived;
            UpdateEssenceIndex();
        }
    }

    UpdateFileDescriptor();  // sets PictureEssenceCoding from the (now SPS-derived) essence index

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

    // Only write geometry when the SPS actually yielded dimensions, so a parameter-set-less
    // first frame never stamps StoredWidth/Height == 0 -- an unopenable file that nonetheless
    // looks populated.
    if (stored_width > 0 && stored_height > 0) {
        cdci_descriptor->setStoredWidth(stored_width);
        cdci_descriptor->setStoredHeight(stored_height);
        cdci_descriptor->setDisplayWidth(display_width);
        cdci_descriptor->setDisplayHeight(display_height);
        cdci_descriptor->setDisplayXOffset(essence_parser->GetDisplayXOffset());
        cdci_descriptor->setDisplayYOffset(essence_parser->GetDisplayYOffset());
        cdci_descriptor->setSampledWidth(stored_width);
        cdci_descriptor->setSampledHeight(stored_height);
        cdci_descriptor->setSampledXOffset(0);
        cdci_descriptor->setSampledYOffset(0);
        cdci_descriptor->setImageStartOffset(0);
        cdci_descriptor->setPaddingBits(0);
    }

    if (essence_parser->GetComponentDepth() > 0)
        cdci_descriptor->setComponentDepth(essence_parser->GetComponentDepth());

    // Chroma subsampling from chroma_format_idc (ITU-T H.265 / SMPTE ST 381-5). A CDCI descriptor
    // must carry subsampling items, so monochrome (idc 0) and any unexpected value fall back to a
    // valid setting rather than being left unset.
    switch (essence_parser->GetChromaFormat())
    {
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
        case 0: // Monochrome -- no chroma planes; keep the descriptor conformant with 1:1.
            cdci_descriptor->setHorizontalSubsampling(1);
            cdci_descriptor->setVerticalSubsampling(1);
            break;
        case 1: // 4:2:0
        default:
            cdci_descriptor->setHorizontalSubsampling(2);
            cdci_descriptor->setVerticalSubsampling(2);
            if (!cdci_descriptor->haveColorSiting())
                SetColorSitingMod(MXF_COLOR_SITING_VERT_MIDPOINT);
            break;
    }

    // Colour metadata from the SPS VUI. Without this the descriptor carries no colour primaries /
    // transfer / matrix and HDR (BT.2020/PQ/HLG) and wide-gamut tagging is silently lost.
    MapColorPrimaries(essence_parser->GetColorPrimaries());
    MapTransferCharacteristic(essence_parser->GetTransferCharacteristics());
    MapMatrixCoefficients(essence_parser->GetMatrixCoefficients());

    // Colour range from the SPS video_full_range_flag. Without explicit black/white reference
    // levels a conformant reader assumes a default range and mis-scales luma for full-range HEVC.
    if (essence_parser->GetComponentDepth() > 0) {
        uint32_t depth = essence_parser->GetComponentDepth();
        uint32_t shift = depth - 8;
        if (essence_parser->GetVideoFullRange()) {
            cdci_descriptor->setBlackRefLevel(0);
            cdci_descriptor->setWhiteReflevel((1u << depth) - 1);
            cdci_descriptor->setColorRange(1u << depth);
        } else {
            cdci_descriptor->setBlackRefLevel(16u << shift);
            cdci_descriptor->setWhiteReflevel(235u << shift);
            // ColorRange is a level count, not a single code point, so it does not scale
            // linearly: 8-bit=225, 10-bit=897, 12-bit=3585 (matches UncCDCIMXFDescriptorHelper).
            cdci_descriptor->setColorRange((224u << shift) + 1);
        }
    }

    // Display aspect ratio from the sample aspect ratio and display dimensions.
    if (!cdci_descriptor->haveAspectRatio() &&
        essence_parser->GetSampleAspectRatio().numerator > 0 &&
        display_width > 0 && display_height > 0)
    {
        Rational sar = essence_parser->GetSampleAspectRatio();
        Rational calc_aspect_ratio;
        calc_aspect_ratio.numerator   = (int32_t)(sar.numerator   * display_width);
        calc_aspect_ratio.denominator = (int32_t)(sar.denominator * display_height);
        cdci_descriptor->setAspectRatio(reduce_rational(calc_aspect_ratio));
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
