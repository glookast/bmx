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

#ifndef BMX_VC3_MXF_DESCRIPTOR_HELPER_H_
#define BMX_VC3_MXF_DESCRIPTOR_HELPER_H_


#include <bmx/mxf_helper/PictureMXFDescriptorHelper.h>



namespace bmx
{


class VC3MXFDescriptorHelper : public PictureMXFDescriptorHelper
{
public:
    static EssenceType IsSupported(mxfpp::FileDescriptor *file_descriptor, mxfUL alternative_ec_label);
    static bool IsSupported(EssenceType essence_type);

private:
    static bool IsAvidDNxHD(mxfpp::FileDescriptor *file_descriptor, mxfUL alternative_ec_label, size_t *index);

public:
    VC3MXFDescriptorHelper();
    virtual ~VC3MXFDescriptorHelper();

public:
    // initialize from existing descriptor
    virtual void Initialize(mxfpp::FileDescriptor *file_descriptor, uint16_t mxf_version, mxfUL alternative_ec_label);

public:
    // configure and create new descriptor
    virtual void SetEssenceType(EssenceType essence_type);

    // GKX (GKX-122): DNxHR is resolution-independent -- the compression ID does not
    // encode the raster. The caller supplies the source raster/depth/scan so the
    // descriptor geometry + constant frame size can be resolved. Must be called
    // before CreateFileDescriptor() for the DNxHR RI essence types; ignored for the
    // fixed-raster DNxHD types (which take geometry from the SUPPORTED_ESSENCE table).
    void SetRIRaster(uint32_t stored_width, uint32_t stored_height, uint32_t component_depth,
                     bool is_interlaced);

    virtual mxfpp::FileDescriptor* CreateFileDescriptor(mxfpp::HeaderMetadata *header_metadata);
    virtual void UpdateFileDescriptor();

public:
    virtual uint32_t GetSampleSize();

    // GKX (GKX-122): true for the DNxHR resolution-independent essence types (1270-1274).
    static bool IsDNxHR(EssenceType essence_type);

protected:
    virtual mxfUL ChooseEssenceContainerUL() const;

private:
    // GKX (GKX-122): resolution-independent DNxHR support.
    static bool IsSupportedRI(mxfpp::FileDescriptor *file_descriptor, mxfUL alternative_ec_label,
                              EssenceType *essence_type);
    void UpdateFileDescriptorRI();
    uint32_t GetRIFrameSize() const;  // (profile, stored_width) -> constant frame size; throws on unsupported raster

private:
    size_t mEssenceIndex;

    // GKX (GKX-122): RI (DNxHR) descriptor state -- raster supplied by the caller.
    bool mIsRI;
    uint32_t mRIStoredWidth;
    uint32_t mRIStoredHeight;
    uint32_t mRIComponentDepth;
    bool mRIInterlaced;
    bool mRIRasterSet;
};


};



#endif

