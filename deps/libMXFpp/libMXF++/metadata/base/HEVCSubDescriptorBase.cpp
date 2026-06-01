/*
 * Copyright (C) 2026, Glookast LLC
 * All Rights Reserved.
 *
 * SMPTE ST 381-5:2023 HEVC Sub Descriptor
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <memory>

#include <libMXF++/MXF.h>


using namespace std;
using namespace mxfpp;


const mxfKey HEVCSubDescriptorBase::setKey = MXF_SET_K(HEVCSubDescriptor);


HEVCSubDescriptorBase::HEVCSubDescriptorBase(HeaderMetadata *headerMetadata)
: SubDescriptor(headerMetadata, headerMetadata->createCSet(&setKey))
{
    headerMetadata->add(this);
}

HEVCSubDescriptorBase::HEVCSubDescriptorBase(HeaderMetadata *headerMetadata, ::MXFMetadataSet *cMetadataSet)
: SubDescriptor(headerMetadata, cMetadataSet)
{}

HEVCSubDescriptorBase::~HEVCSubDescriptorBase()
{}


uint8_t HEVCSubDescriptorBase::getHEVCDecodingDelay() const
{
    return getUInt8Item(&MXF_ITEM_K(HEVCSubDescriptor, HEVCDecodingDelay));
}

bool HEVCSubDescriptorBase::haveHEVCConstantBPictureFlag() const
{
    return haveItem(&MXF_ITEM_K(HEVCSubDescriptor, HEVCConstantBPictureFlag));
}

bool HEVCSubDescriptorBase::getHEVCConstantBPictureFlag() const
{
    return getBooleanItem(&MXF_ITEM_K(HEVCSubDescriptor, HEVCConstantBPictureFlag));
}

bool HEVCSubDescriptorBase::haveHEVCCodedContentKind() const
{
    return haveItem(&MXF_ITEM_K(HEVCSubDescriptor, HEVCCodedContentKind));
}

uint8_t HEVCSubDescriptorBase::getHEVCCodedContentKind() const
{
    return getUInt8Item(&MXF_ITEM_K(HEVCSubDescriptor, HEVCCodedContentKind));
}

bool HEVCSubDescriptorBase::haveHEVCClosedGOPIndicator() const
{
    return haveItem(&MXF_ITEM_K(HEVCSubDescriptor, HEVCClosedGOPIndicator));
}

bool HEVCSubDescriptorBase::getHEVCClosedGOPIndicator() const
{
    return getBooleanItem(&MXF_ITEM_K(HEVCSubDescriptor, HEVCClosedGOPIndicator));
}

bool HEVCSubDescriptorBase::haveHEVCIdenticalGOPIndicator() const
{
    return haveItem(&MXF_ITEM_K(HEVCSubDescriptor, HEVCIdenticalGOPIndicator));
}

bool HEVCSubDescriptorBase::getHEVCIdenticalGOPIndicator() const
{
    return getBooleanItem(&MXF_ITEM_K(HEVCSubDescriptor, HEVCIdenticalGOPIndicator));
}

bool HEVCSubDescriptorBase::haveHEVCMaximumGOPSize() const
{
    return haveItem(&MXF_ITEM_K(HEVCSubDescriptor, HEVCMaximumGOPSize));
}

uint16_t HEVCSubDescriptorBase::getHEVCMaximumGOPSize() const
{
    return getUInt16Item(&MXF_ITEM_K(HEVCSubDescriptor, HEVCMaximumGOPSize));
}

bool HEVCSubDescriptorBase::haveHEVCMaximumBPictureCount() const
{
    return haveItem(&MXF_ITEM_K(HEVCSubDescriptor, HEVCMaximumBPictureCount));
}

uint16_t HEVCSubDescriptorBase::getHEVCMaximumBPictureCount() const
{
    return getUInt16Item(&MXF_ITEM_K(HEVCSubDescriptor, HEVCMaximumBPictureCount));
}

bool HEVCSubDescriptorBase::haveHEVCMaximumBitrate() const
{
    return haveItem(&MXF_ITEM_K(HEVCSubDescriptor, HEVCMaximumBitrate));
}

uint32_t HEVCSubDescriptorBase::getHEVCMaximumBitrate() const
{
    return getUInt32Item(&MXF_ITEM_K(HEVCSubDescriptor, HEVCMaximumBitrate));
}

bool HEVCSubDescriptorBase::haveHEVCAverageBitrate() const
{
    return haveItem(&MXF_ITEM_K(HEVCSubDescriptor, HEVCAverageBitrate));
}

uint32_t HEVCSubDescriptorBase::getHEVCAverageBitrate() const
{
    return getUInt32Item(&MXF_ITEM_K(HEVCSubDescriptor, HEVCAverageBitrate));
}

bool HEVCSubDescriptorBase::haveHEVCProfile() const
{
    return haveItem(&MXF_ITEM_K(HEVCSubDescriptor, HEVCProfile));
}

uint8_t HEVCSubDescriptorBase::getHEVCProfile() const
{
    return getUInt8Item(&MXF_ITEM_K(HEVCSubDescriptor, HEVCProfile));
}

bool HEVCSubDescriptorBase::haveHEVCProfileConstraint() const
{
    return haveItem(&MXF_ITEM_K(HEVCSubDescriptor, HEVCProfileConstraint));
}

uint16_t HEVCSubDescriptorBase::getHEVCProfileConstraint() const
{
    return getUInt16Item(&MXF_ITEM_K(HEVCSubDescriptor, HEVCProfileConstraint));
}

bool HEVCSubDescriptorBase::haveHEVCLevel() const
{
    return haveItem(&MXF_ITEM_K(HEVCSubDescriptor, HEVCLevel));
}

uint8_t HEVCSubDescriptorBase::getHEVCLevel() const
{
    return getUInt8Item(&MXF_ITEM_K(HEVCSubDescriptor, HEVCLevel));
}

bool HEVCSubDescriptorBase::haveHEVCTier() const
{
    return haveItem(&MXF_ITEM_K(HEVCSubDescriptor, HEVCTier));
}

uint8_t HEVCSubDescriptorBase::getHEVCTier() const
{
    return getUInt8Item(&MXF_ITEM_K(HEVCSubDescriptor, HEVCTier));
}

bool HEVCSubDescriptorBase::haveHEVCMaximumRefFrames() const
{
    return haveItem(&MXF_ITEM_K(HEVCSubDescriptor, HEVCMaximumRefFrames));
}

uint8_t HEVCSubDescriptorBase::getHEVCMaximumRefFrames() const
{
    return getUInt8Item(&MXF_ITEM_K(HEVCSubDescriptor, HEVCMaximumRefFrames));
}

bool HEVCSubDescriptorBase::haveHEVCSequenceParameterSetFlag() const
{
    return haveItem(&MXF_ITEM_K(HEVCSubDescriptor, HEVCSequenceParameterSetFlag));
}

uint8_t HEVCSubDescriptorBase::getHEVCSequenceParameterSetFlag() const
{
    return getUInt8Item(&MXF_ITEM_K(HEVCSubDescriptor, HEVCSequenceParameterSetFlag));
}

bool HEVCSubDescriptorBase::haveHEVCPictureParameterSetFlag() const
{
    return haveItem(&MXF_ITEM_K(HEVCSubDescriptor, HEVCPictureParameterSetFlag));
}

uint8_t HEVCSubDescriptorBase::getHEVCPictureParameterSetFlag() const
{
    return getUInt8Item(&MXF_ITEM_K(HEVCSubDescriptor, HEVCPictureParameterSetFlag));
}

bool HEVCSubDescriptorBase::haveHEVCVideoParameterSetFlag() const
{
    return haveItem(&MXF_ITEM_K(HEVCSubDescriptor, HEVCVideoParameterSetFlag));
}

uint8_t HEVCSubDescriptorBase::getHEVCVideoParameterSetFlag() const
{
    return getUInt8Item(&MXF_ITEM_K(HEVCSubDescriptor, HEVCVideoParameterSetFlag));
}


void HEVCSubDescriptorBase::setHEVCDecodingDelay(uint8_t value)
{
    setUInt8Item(&MXF_ITEM_K(HEVCSubDescriptor, HEVCDecodingDelay), value);
}

void HEVCSubDescriptorBase::setHEVCConstantBPictureFlag(bool value)
{
    setBooleanItem(&MXF_ITEM_K(HEVCSubDescriptor, HEVCConstantBPictureFlag), value);
}

void HEVCSubDescriptorBase::setHEVCCodedContentKind(uint8_t value)
{
    setUInt8Item(&MXF_ITEM_K(HEVCSubDescriptor, HEVCCodedContentKind), value);
}

void HEVCSubDescriptorBase::setHEVCClosedGOPIndicator(bool value)
{
    setBooleanItem(&MXF_ITEM_K(HEVCSubDescriptor, HEVCClosedGOPIndicator), value);
}

void HEVCSubDescriptorBase::setHEVCIdenticalGOPIndicator(bool value)
{
    setBooleanItem(&MXF_ITEM_K(HEVCSubDescriptor, HEVCIdenticalGOPIndicator), value);
}

void HEVCSubDescriptorBase::setHEVCMaximumGOPSize(uint16_t value)
{
    setUInt16Item(&MXF_ITEM_K(HEVCSubDescriptor, HEVCMaximumGOPSize), value);
}

void HEVCSubDescriptorBase::setHEVCMaximumBPictureCount(uint16_t value)
{
    setUInt16Item(&MXF_ITEM_K(HEVCSubDescriptor, HEVCMaximumBPictureCount), value);
}

void HEVCSubDescriptorBase::setHEVCMaximumBitrate(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(HEVCSubDescriptor, HEVCMaximumBitrate), value);
}

void HEVCSubDescriptorBase::setHEVCAverageBitrate(uint32_t value)
{
    setUInt32Item(&MXF_ITEM_K(HEVCSubDescriptor, HEVCAverageBitrate), value);
}

void HEVCSubDescriptorBase::setHEVCProfile(uint8_t value)
{
    setUInt8Item(&MXF_ITEM_K(HEVCSubDescriptor, HEVCProfile), value);
}

void HEVCSubDescriptorBase::setHEVCProfileConstraint(uint16_t value)
{
    setUInt16Item(&MXF_ITEM_K(HEVCSubDescriptor, HEVCProfileConstraint), value);
}

void HEVCSubDescriptorBase::setHEVCLevel(uint8_t value)
{
    setUInt8Item(&MXF_ITEM_K(HEVCSubDescriptor, HEVCLevel), value);
}

void HEVCSubDescriptorBase::setHEVCTier(uint8_t value)
{
    setUInt8Item(&MXF_ITEM_K(HEVCSubDescriptor, HEVCTier), value);
}

void HEVCSubDescriptorBase::setHEVCMaximumRefFrames(uint8_t value)
{
    setUInt8Item(&MXF_ITEM_K(HEVCSubDescriptor, HEVCMaximumRefFrames), value);
}

void HEVCSubDescriptorBase::setHEVCSequenceParameterSetFlag(uint8_t value)
{
    setUInt8Item(&MXF_ITEM_K(HEVCSubDescriptor, HEVCSequenceParameterSetFlag), value);
}

void HEVCSubDescriptorBase::setHEVCPictureParameterSetFlag(uint8_t value)
{
    setUInt8Item(&MXF_ITEM_K(HEVCSubDescriptor, HEVCPictureParameterSetFlag), value);
}

void HEVCSubDescriptorBase::setHEVCVideoParameterSetFlag(uint8_t value)
{
    setUInt8Item(&MXF_ITEM_K(HEVCSubDescriptor, HEVCVideoParameterSetFlag), value);
}
