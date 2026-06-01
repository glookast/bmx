/*
 * Copyright (C) 2026, Glookast LLC
 * All Rights Reserved.
 *
 * SMPTE ST 381-5:2023 HEVC Sub Descriptor
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.
 */

#ifndef MXFPP_HEVCSUBDESCRIPTOR_BASE_H_
#define MXFPP_HEVCSUBDESCRIPTOR_BASE_H_



#include <libMXF++/metadata/SubDescriptor.h>


namespace mxfpp
{


class HEVCSubDescriptorBase : public SubDescriptor
{
public:
    friend class MetadataSetFactory<HEVCSubDescriptorBase>;
    static const mxfKey setKey;

public:
    HEVCSubDescriptorBase(HeaderMetadata *headerMetadata);
    virtual ~HEVCSubDescriptorBase();


   // getters

   uint8_t getHEVCDecodingDelay() const;
   bool haveHEVCConstantBPictureFlag() const;
   bool getHEVCConstantBPictureFlag() const;
   bool haveHEVCCodedContentKind() const;
   uint8_t getHEVCCodedContentKind() const;
   bool haveHEVCClosedGOPIndicator() const;
   bool getHEVCClosedGOPIndicator() const;
   bool haveHEVCIdenticalGOPIndicator() const;
   bool getHEVCIdenticalGOPIndicator() const;
   bool haveHEVCMaximumGOPSize() const;
   uint16_t getHEVCMaximumGOPSize() const;
   bool haveHEVCMaximumBPictureCount() const;
   uint16_t getHEVCMaximumBPictureCount() const;
   bool haveHEVCMaximumBitrate() const;
   uint32_t getHEVCMaximumBitrate() const;
   bool haveHEVCAverageBitrate() const;
   uint32_t getHEVCAverageBitrate() const;
   bool haveHEVCProfile() const;
   uint8_t getHEVCProfile() const;
   bool haveHEVCProfileConstraint() const;
   uint16_t getHEVCProfileConstraint() const;
   bool haveHEVCLevel() const;
   uint8_t getHEVCLevel() const;
   bool haveHEVCTier() const;
   uint8_t getHEVCTier() const;
   bool haveHEVCMaximumRefFrames() const;
   uint8_t getHEVCMaximumRefFrames() const;
   bool haveHEVCSequenceParameterSetFlag() const;
   uint8_t getHEVCSequenceParameterSetFlag() const;
   bool haveHEVCPictureParameterSetFlag() const;
   uint8_t getHEVCPictureParameterSetFlag() const;
   bool haveHEVCVideoParameterSetFlag() const;
   uint8_t getHEVCVideoParameterSetFlag() const;


   // setters

   void setHEVCDecodingDelay(uint8_t value);
   void setHEVCConstantBPictureFlag(bool value);
   void setHEVCCodedContentKind(uint8_t value);
   void setHEVCClosedGOPIndicator(bool value);
   void setHEVCIdenticalGOPIndicator(bool value);
   void setHEVCMaximumGOPSize(uint16_t value);
   void setHEVCMaximumBPictureCount(uint16_t value);
   void setHEVCMaximumBitrate(uint32_t value);
   void setHEVCAverageBitrate(uint32_t value);
   void setHEVCProfile(uint8_t value);
   void setHEVCProfileConstraint(uint16_t value);
   void setHEVCLevel(uint8_t value);
   void setHEVCTier(uint8_t value);
   void setHEVCMaximumRefFrames(uint8_t value);
   void setHEVCSequenceParameterSetFlag(uint8_t value);
   void setHEVCPictureParameterSetFlag(uint8_t value);
   void setHEVCVideoParameterSetFlag(uint8_t value);


protected:
    HEVCSubDescriptorBase(HeaderMetadata *headerMetadata, ::MXFMetadataSet *cMetadataSet);
};


};


#endif
