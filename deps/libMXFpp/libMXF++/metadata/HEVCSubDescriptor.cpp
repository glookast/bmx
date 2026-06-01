/*
 * Copyright (C) 2026, Glookast LLC
 * All Rights Reserved.
 *
 * SMPTE ST 381-5:2023 HEVC Sub Descriptor
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <libMXF++/MXF.h>


using namespace std;
using namespace mxfpp;



HEVCSubDescriptor::HEVCSubDescriptor(HeaderMetadata *headerMetadata)
: HEVCSubDescriptorBase(headerMetadata)
{}

HEVCSubDescriptor::HEVCSubDescriptor(HeaderMetadata *headerMetadata, ::MXFMetadataSet *cMetadataSet)
: HEVCSubDescriptorBase(headerMetadata, cMetadataSet)
{}

HEVCSubDescriptor::~HEVCSubDescriptor()
{}
