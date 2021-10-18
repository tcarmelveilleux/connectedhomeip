/*
 *
 *    Copyright (c) 2020-2021 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/**
 * @file
 *   This file contains the implementation the chip message header
 *   encode/decode classes.
 */

#include "MessageHeader.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>

#include <type_traits>

#include <lib/core/CHIPError.h>
#include <lib/support/BufferReader.h>
#include <lib/support/CodeUtils.h>

/**********************************************
 * Header format (little endian):
 *
 * -------- Unencrypted header -----------------------------------------------------
 *  8 bit:  | Message Flags: VERSION: 4 bit | S: 1 bit | RESERVED: 1 bit | DSIZ: 2 bit |
 *  8 bit:  | Security Flags: P: 1 bit | C: 1 bit | MX: 1 bit | RESERVED: 3 bit | Session Type: 2 bit |
 *  16 bit: | Session ID                                                           |
 *  32 bit: | Message Counter                                                      |
 *  64 bit: | SOURCE_NODE_ID (iff source node flag is set)                         |
 *  64 bit: | DEST_NODE_ID (iff destination node flag is set)                      |
 * -------- Encrypted header -------------------------------------------------------
 *  8 bit:  | Exchange Flags: RESERVED: 3 bit | V: 1 bit | SX: 1 bit | R: 1 bit | A: 1 bit | I: 1 bit |
 *  8 bit:  | Protocol Opcode                                                      |
 * 16 bit:  | Exchange ID                                                          |
 * 16 bit:  | Protocol ID                                                          |
 * 16 bit:  | Optional Vendor ID                                                   |
 * 32 bit:  | Acknowledged Message Counter (if A flag in the Header is set)        |
 * -------- Encrypted Application Data Start ---------------------------------------
 *  <var>:  | Encrypted Data                                                       |
 * -------- Encrypted Application Data End -----------------------------------------
 *  <var>:  | (Unencrypted) Message Authentication Tag                             |
 *
 **********************************************/

namespace chip {
namespace {

using namespace chip::Encoding;

/// size of the fixed portion of the header
constexpr size_t kFixedUnencryptedHeaderSizeBytes = 8;

/// size of the encrypted portion of the header
constexpr size_t kEncryptedHeaderSizeBytes = 6;

/// size of a serialized node id inside a header
constexpr size_t kNodeIdSizeBytes = 8;

/// size of a serialized group id inside a header
constexpr size_t kGroupIdSizeBytes = 2;

/// size of a serialized vendor id inside a header
constexpr size_t kVendorIdSizeBytes = 2;

/// size of a serialized ack message counter inside a header
constexpr size_t kAckMessageCounterSizeBytes = 4;

/// Mask to extract just the version part from a 8bits header prefix.
constexpr uint8_t kVersionMask = 0xF0;

constexpr uint8_t kMsgFlagsMask = 0x07;
/// Shift to convert to/from a masked version 8bit value to a 4bit version.
constexpr int kVersionShift = 4;

// Mask to extract sessionType
constexpr uint8_t kSessionTypeMask = 0x03;

} // namespace

uint16_t PacketHeader::EncodeSizeBytes() const
{
    size_t size = kFixedUnencryptedHeaderSizeBytes;

    if (mSourceNodeId.HasValue())
    {
        size += kNodeIdSizeBytes;
    }

    if (mDestinationNodeId.HasValue())
    {
        size += kNodeIdSizeBytes;
    }
    else if (mDestinationGroupId.HasValue())
    {
        size += kGroupIdSizeBytes;
    }

    static_assert(kFixedUnencryptedHeaderSizeBytes + kNodeIdSizeBytes + kNodeIdSizeBytes <= UINT16_MAX,
                  "Header size does not fit in uint16_t");
    return static_cast<uint16_t>(size);
}

uint16_t PayloadHeader::EncodeSizeBytes() const
{
    size_t size = kEncryptedHeaderSizeBytes;

    if (HaveVendorId())
    {
        size += kVendorIdSizeBytes;
    }

    if (mAckMessageCounter.HasValue())
    {
        size += kAckMessageCounterSizeBytes;
    }

    static_assert(kEncryptedHeaderSizeBytes + kVendorIdSizeBytes + kAckMessageCounterSizeBytes <= UINT16_MAX,
                  "Header size does not fit in uint16_t");
    return static_cast<uint16_t>(size);
}

CHIP_ERROR PacketHeader::Decode(const uint8_t * const data, uint16_t size, uint16_t * decode_len)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    LittleEndian::Reader reader(data, size);
    int version;
    // TODO: De-uint16-ify everything related to this library
    uint16_t octets_read;

    uint8_t msgFlags;
    err = reader.Read8(&msgFlags).StatusCode();
    SuccessOrExit(err);
    version = ((msgFlags & kVersionMask) >> kVersionShift);
    VerifyOrExit(version == kMsgHeaderVersion, err = CHIP_ERROR_VERSION_MISMATCH);

    mMsgFlags.SetRaw(msgFlags);

    uint8_t securityFlags;
    err = reader.Read8(&securityFlags).StatusCode();
    SuccessOrExit(err);
    mSecFlags.SetRaw(securityFlags);

    mSessionType = static_cast<Header::SessionType>(securityFlags & kSessionTypeMask);

    err = reader.Read16(&mSessionId).StatusCode();
    SuccessOrExit(err);

    err = reader.Read32(&mMessageCounter).StatusCode();
    SuccessOrExit(err);

    if (mMsgFlags.Has(Header::MsgFlagValues::kSourceNodeIdPresent))
    {
        uint64_t sourceNodeId;
        err = reader.Read64(&sourceNodeId).StatusCode();
        SuccessOrExit(err);
        mSourceNodeId.SetValue(sourceNodeId);
    }
    else
    {
        mSourceNodeId.ClearValue();
    }

    if (!IsSessionTypeValid())
    {
        // Reserved.
        err = CHIP_ERROR_INTERNAL;
        SuccessOrExit(err);
    }

    if (mMsgFlags.HasAll(Header::MsgFlagValues::kDestinationNodeIdPresent, Header::MsgFlagValues::kDestinationGroupIdPresent))
    {
        // Reserved.
        err = CHIP_ERROR_INTERNAL;
        SuccessOrExit(err);
    }
    else if (mMsgFlags.Has(Header::MsgFlagValues::kDestinationNodeIdPresent))
    {
        if (mSessionType != Header::SessionType::kUnicastSession)
        {
            err = CHIP_ERROR_INTERNAL;
            SuccessOrExit(err);
        }
        uint64_t destinationNodeId;
        err = reader.Read64(&destinationNodeId).StatusCode();
        SuccessOrExit(err);
        mDestinationNodeId.SetValue(destinationNodeId);
        mDestinationGroupId.ClearValue();
    }
    else if (mMsgFlags.Has(Header::MsgFlagValues::kDestinationGroupIdPresent))
    {
        if (mSessionType != Header::SessionType::kGroupSession)
        {
            err = CHIP_ERROR_INTERNAL;
            SuccessOrExit(err);
        }
        uint16_t destinationGroupId;
        err = reader.Read16(&destinationGroupId).StatusCode();
        SuccessOrExit(err);
        mDestinationGroupId.SetValue(destinationGroupId);
        mDestinationNodeId.ClearValue();
    }
    else
    {
        mDestinationNodeId.ClearValue();
        mDestinationGroupId.ClearValue();
    }

    octets_read = static_cast<uint16_t>(reader.OctetsRead());
    VerifyOrExit(octets_read == EncodeSizeBytes(), err = CHIP_ERROR_INTERNAL);
    *decode_len = octets_read;

exit:

    return err;
}

CHIP_ERROR PacketHeader::DecodeAndConsume(const System::PacketBufferHandle & buf)
{
    uint16_t headerSize = 0;
    ReturnErrorOnFailure(Decode(buf->Start(), buf->DataLength(), &headerSize));
    buf->ConsumeHead(headerSize);
    return CHIP_NO_ERROR;
}

CHIP_ERROR PayloadHeader::Decode(const uint8_t * const data, uint16_t size, uint16_t * decode_len)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    LittleEndian::Reader reader(data, size);
    uint8_t header;
    uint16_t octets_read;

    err = reader.Read8(&header).Read8(&mMessageType).Read16(&mExchangeID).StatusCode();
    SuccessOrExit(err);

    mExchangeFlags.SetRaw(header);

    VendorId vendor_id;
    if (HaveVendorId())
    {
        uint16_t vendor_id_raw;
        err = reader.Read16(&vendor_id_raw).StatusCode();
        SuccessOrExit(err);
        vendor_id = static_cast<VendorId>(vendor_id_raw);
    }
    else
    {
        vendor_id = VendorId::Common;
    }

    uint16_t protocol_id;
    err = reader.Read16(&protocol_id).StatusCode();
    SuccessOrExit(err);

    mProtocolID = Protocols::Id(vendor_id, protocol_id);

    if (mExchangeFlags.Has(Header::ExFlagValues::kExchangeFlag_AckMsg))
    {
        uint32_t ack_message_counter;
        err = reader.Read32(&ack_message_counter).StatusCode();
        SuccessOrExit(err);
        mAckMessageCounter.SetValue(ack_message_counter);
    }
    else
    {
        mAckMessageCounter.ClearValue();
    }

    octets_read = static_cast<uint16_t>(reader.OctetsRead());
    VerifyOrExit(octets_read == EncodeSizeBytes(), err = CHIP_ERROR_INTERNAL);
    *decode_len = octets_read;

exit:

    return err;
}

CHIP_ERROR PayloadHeader::DecodeAndConsume(const System::PacketBufferHandle & buf)
{
    uint16_t headerSize = 0;
    ReturnErrorOnFailure(Decode(buf->Start(), buf->DataLength(), &headerSize));
    buf->ConsumeHead(headerSize);
    return CHIP_NO_ERROR;
}

CHIP_ERROR PacketHeader::Encode(uint8_t * data, uint16_t size, uint16_t * encode_size) const
{
    VerifyOrReturnError(size >= EncodeSizeBytes(), CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(!(mDestinationNodeId.HasValue() && mDestinationGroupId.HasValue()), CHIP_ERROR_INTERNAL);
    VerifyOrReturnError(encode_size != nullptr, CHIP_ERROR_INTERNAL);
    VerifyOrReturnError(IsSessionTypeValid(), CHIP_ERROR_INTERNAL);
    VerifyOrReturnError(!(IsGroupSession() && !mDestinationGroupId.HasValue()), CHIP_ERROR_INTERNAL);

    Header::MsgFlags messageFlags = mMsgFlags;
    messageFlags.Set(Header::MsgFlagValues::kSourceNodeIdPresent, mSourceNodeId.HasValue())
        .Set(Header::MsgFlagValues::kDestinationNodeIdPresent, mDestinationNodeId.HasValue())
        .Set(Header::MsgFlagValues::kDestinationGroupIdPresent, mDestinationGroupId.HasValue());

    uint8_t msgFlags = (kMsgHeaderVersion << kVersionShift) | (messageFlags.Raw() & kMsgFlagsMask);
    uint8_t secFlags = mSecFlags.Raw();
    secFlags |= static_cast<uint8_t>(mSessionType);

    uint8_t * p = data;
    Write8(p, msgFlags);
    Write8(p, secFlags);
    LittleEndian::Write16(p, mSessionId);
    LittleEndian::Write32(p, mMessageCounter);
    if (mSourceNodeId.HasValue())
    {
        LittleEndian::Write64(p, mSourceNodeId.Value());
    }
    if (mDestinationNodeId.HasValue())
    {
        LittleEndian::Write64(p, mDestinationNodeId.Value());
    }

    if (mDestinationGroupId.HasValue())
    {
        LittleEndian::Write16(p, mDestinationGroupId.Value());
    }

    // Written data size provided to caller on success
    VerifyOrReturnError(p - data == EncodeSizeBytes(), CHIP_ERROR_INTERNAL);
    *encode_size = static_cast<uint16_t>(p - data);

    return CHIP_NO_ERROR;
}

CHIP_ERROR PacketHeader::EncodeBeforeData(const System::PacketBufferHandle & buf) const
{
    // Note: PayloadHeader::EncodeBeforeData probably needs changes if you
    // change anything here.
    uint16_t headerSize = EncodeSizeBytes();
    VerifyOrReturnError(buf->EnsureReservedSize(headerSize), CHIP_ERROR_NO_MEMORY);
    buf->SetStart(buf->Start() - headerSize);
    uint16_t actualEncodedHeaderSize;
    ReturnErrorOnFailure(EncodeAtStart(buf, &actualEncodedHeaderSize));
    VerifyOrReturnError(actualEncodedHeaderSize == headerSize, CHIP_ERROR_INTERNAL);
    return CHIP_NO_ERROR;
}

CHIP_ERROR PayloadHeader::Encode(uint8_t * data, uint16_t size, uint16_t * encode_size) const
{
    VerifyOrReturnError(size >= EncodeSizeBytes(), CHIP_ERROR_INVALID_ARGUMENT);

    uint8_t * p          = data;
    const uint8_t header = mExchangeFlags.Raw();

    Write8(p, header);
    Write8(p, mMessageType);
    LittleEndian::Write16(p, mExchangeID);
    if (HaveVendorId())
    {
        LittleEndian::Write16(p, to_underlying(mProtocolID.GetVendorId()));
    }
    LittleEndian::Write16(p, mProtocolID.GetProtocolId());
    if (mAckMessageCounter.HasValue())
    {
        LittleEndian::Write32(p, mAckMessageCounter.Value());
    }

    // Written data size provided to caller on success
    VerifyOrReturnError(p - data == EncodeSizeBytes(), CHIP_ERROR_INTERNAL);
    *encode_size = static_cast<uint16_t>(p - data);

    return CHIP_NO_ERROR;
}

CHIP_ERROR PayloadHeader::EncodeBeforeData(const System::PacketBufferHandle & buf) const
{
    // Note: PacketHeader::EncodeBeforeData probably needs changes if you change
    // anything here.
    uint16_t headerSize = EncodeSizeBytes();
    VerifyOrReturnError(buf->EnsureReservedSize(headerSize), CHIP_ERROR_NO_MEMORY);
    buf->SetStart(buf->Start() - headerSize);
    uint16_t actualEncodedHeaderSize;
    ReturnErrorOnFailure(EncodeAtStart(buf, &actualEncodedHeaderSize));
    VerifyOrReturnError(actualEncodedHeaderSize == headerSize, CHIP_ERROR_INTERNAL);
    return CHIP_NO_ERROR;
}

CHIP_ERROR MessageAuthenticationCode::Decode(const PacketHeader & packetHeader, const uint8_t * const data, uint16_t size,
                                             uint16_t * decode_len)
{
    const uint16_t taglen = packetHeader.MICTagLength();

    VerifyOrReturnError(taglen != 0, CHIP_ERROR_WRONG_ENCRYPTION_TYPE_FROM_PEER);
    VerifyOrReturnError(size >= taglen, CHIP_ERROR_INVALID_ARGUMENT);

    memcpy(&mTag[0], data, taglen);

    *decode_len = taglen;

    return CHIP_NO_ERROR;
}

CHIP_ERROR MessageAuthenticationCode::Encode(const PacketHeader & packetHeader, uint8_t * data, uint16_t size,
                                             uint16_t * encode_size) const
{
    uint8_t * p           = data;
    const uint16_t taglen = packetHeader.MICTagLength();

    VerifyOrReturnError(taglen != 0, CHIP_ERROR_WRONG_ENCRYPTION_TYPE);
    VerifyOrReturnError(size >= taglen, CHIP_ERROR_INVALID_ARGUMENT);

    memcpy(p, &mTag[0], taglen);

    // Written data size provided to caller on success
    *encode_size = taglen;

    return CHIP_NO_ERROR;
}

} // namespace chip
