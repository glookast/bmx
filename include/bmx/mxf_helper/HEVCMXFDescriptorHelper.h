/*
 * Copyright (C) 2026, Glookast LLC
 * All Rights Reserved.
 *
 * SMPTE ST 381-5:2023 HEVC MXF Descriptor Helper
 */

#ifndef BMX_HEVC_MXF_DESCRIPTOR_HELPER_H_
#define BMX_HEVC_MXF_DESCRIPTOR_HELPER_H_


#include <bmx/mxf_helper/PictureMXFDescriptorHelper.h>



namespace bmx
{


class HEVCEssenceParser;


class HEVCMXFDescriptorHelper : public PictureMXFDescriptorHelper
{
public:
    static EssenceType IsSupported(mxfpp::FileDescriptor *file_descriptor, mxfUL alternative_ec_label);
    static bool IsSupported(EssenceType essence_type);

public:
    HEVCMXFDescriptorHelper();
    virtual ~HEVCMXFDescriptorHelper();

public:
    virtual void Initialize(mxfpp::FileDescriptor *file_descriptor, uint16_t mxf_version, mxfUL alternative_ec_label);

public:
    virtual void SetEssenceType(EssenceType essence_type);

    virtual mxfpp::FileDescriptor* CreateFileDescriptor(mxfpp::HeaderMetadata *header_metadata);
    virtual void UpdateFileDescriptor();
    virtual void UpdateFileDescriptor(mxfpp::FileDescriptor *file_desc_in);

    // Populate the CDCI picture geometry (stored/display/sampled dimensions, component depth and
    // chroma subsampling) from a parsed HEVC SPS. Mirrors AVCMXFDescriptorHelper; without this the
    // output descriptor carries StoredWidth/StoredHeight == 0 and no NLE can open the file.
    void UpdateFileDescriptor(HEVCEssenceParser *essence_parser);

    mxfpp::HEVCSubDescriptor* GetHEVCSubDescriptor() const { return mHEVCSubDescriptor; }

protected:
    virtual mxfUL ChooseEssenceContainerUL() const;

private:
    void UpdateEssenceIndex();

    // Map SPS VUI colour signalling (H.265 shares the H.264 / H.273 code points) onto the
    // CDCI descriptor's ColorPrimaries / CaptureGamma / CodingEquations.
    void MapColorPrimaries(uint8_t hevc_value);
    void MapTransferCharacteristic(uint8_t hevc_value);
    void MapMatrixCoefficients(uint8_t hevc_value);

private:
    size_t mEssenceIndex;
    mxfpp::HEVCSubDescriptor *mHEVCSubDescriptor;
};


};



#endif
