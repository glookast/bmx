/*
 * Copyright (C) 2026, Glookast LLC
 * All Rights Reserved.
 *
 * SMPTE ST 381-5:2023 HEVC Sub Descriptor
 */

#ifndef MXFPP_HEVCSUBDESCRIPTOR_H_
#define MXFPP_HEVCSUBDESCRIPTOR_H_



#include <libMXF++/metadata/base/HEVCSubDescriptorBase.h>


namespace mxfpp
{


class HEVCSubDescriptor : public HEVCSubDescriptorBase
{
public:
    friend class MetadataSetFactory<HEVCSubDescriptor>;

public:
    HEVCSubDescriptor(HeaderMetadata *headerMetadata);
    virtual ~HEVCSubDescriptor();




protected:
    HEVCSubDescriptor(HeaderMetadata *headerMetadata, ::MXFMetadataSet *cMetadataSet);
};


};


#endif
