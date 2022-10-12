#!/usr/bin/env python

#
#    Copyright (c) 2022 Project CHIP Authors
#    All rights reserved.
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

from binascii import unhexlify
import enum
from dataclasses import dataclass
from typing import Tuple
import unittest

# ======= Verhoeff Code Bits (see `src/lib/support/verhoeff/Verhoeff.py`` for a standalone version)

CharSet_Base10 = "0123456789"
CharSet_Base16 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
CharSet_Base32 = "0123456789ABCDEFGHJKLMNPRSTUVWXY"  # Excludes I, O, Q and Z
CharSet_Base36 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"

PermTable_Base10 = [1,   5,  7,  6,  2,  8,  3,  0,  9,  4]
PermTable_Base16 = [4,   7,  5, 14,  8, 12,
                    15,  0,  2, 11,  3, 13, 10,  6,  9,  1]
PermTable_Base32 = [7,   2,  1, 30, 16, 20, 27, 11, 31,  6,  8, 13, 29,  5,
                    10, 21, 22,  3, 24,  0, 23, 25, 12,  9, 28, 14,  4, 15, 17, 18, 19, 26]
PermTable_Base36 = [29,  0, 32, 11, 35, 20,  7, 27,  2,  4, 19, 28, 30,  1,  5, 12,
                    3,  9, 16, 22,  6, 33,  8, 24, 26, 21, 14, 10, 34, 31, 15, 25, 17, 13, 23, 18]

def DihedralMultiply(x, y, n):
    n2 = n * 2

    x = x % n2
    y = y % n2

    if (x < n):
        if (y < n):
            return (x + y) % n
        else:
            return ((x + (y - n)) % n) + n
    else:
        if (y < n):
            return ((n + (x - n) - y) % n) + n
        else:
            return (n + (x - n) - (y - n)) % n


def DihedralInvert(val, n):
    if (val > 0 and val < n):
        return n - val
    else:
        return val


def Permute(val, permTable, iterCount):
    val = val % len(permTable)
    if (iterCount == 0):
        return val
    else:
        return Permute(permTable[val], permTable, iterCount - 1)


def _ComputeCheckChar(str, strLen, polygonSize, permTable, charSet):
    str = str.upper()
    c = 0
    for i in range(1, strLen+1):
        ch = str[strLen - i]
        val = charSet.index(ch)
        p = Permute(val, permTable, i)
        c = DihedralMultiply(c, p, polygonSize)
    c = DihedralInvert(c, polygonSize)
    return charSet[c]


def ComputeCheckChar(str, charSet=CharSet_Base10):
    return _ComputeCheckChar(str, len(str), polygonSize=5, permTable=PermTable_Base10, charSet=charSet)


def VerifyCheckChar(str, charSet=CharSet_Base10):
    expectedCheckCh = _ComputeCheckChar(str, len(
        str)-1, polygonSize=5, permTable=PermTable_Base10, charSet=CharSet_Base10)
    return str[-1] == expectedCheckCh


def ComputeCheckChar16(str, charSet=CharSet_Base16):
    return _ComputeCheckChar(str, len(str), polygonSize=8, permTable=PermTable_Base16, charSet=charSet)


def VerifyCheckChar16(str, charSet=CharSet_Base16):
    expectedCheckCh = _ComputeCheckChar(
        str, len(str)-1, polygonSize=8, permTable=PermTable_Base16, charSet=charSet)
    return str[-1] == expectedCheckCh


def ComputeCheckChar32(str, charSet=CharSet_Base32):
    return _ComputeCheckChar(str, len(str), polygonSize=16, permTable=PermTable_Base32, charSet=charSet)


def VerifyCheckChar32(str, charSet=CharSet_Base32):
    expectedCheckCh = _ComputeCheckChar(
        str, len(str)-1, polygonSize=16, permTable=PermTable_Base32, charSet=charSet)
    return str[-1] == expectedCheckCh


def ComputeCheckChar36(str, charSet=CharSet_Base36):
    return _ComputeCheckChar(str, len(str), polygonSize=18, permTable=PermTable_Base36, charSet=charSet)


def VerifyCheckChar36(str, charSet=CharSet_Base36):
    expectedCheckCh = _ComputeCheckChar(
        str, len(str)-1, polygonSize=18, permTable=PermTable_Base36, charSet=charSet)
    return str[-1] == expectedCheckCh

# ======= QR Code payload bits

MATTER_BASE38_ALPHABET = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-."
class BaseNCodec:
  def __init__(self, alphabet: str, input_chunk_size:int, output_chunk_sizes: list[int]):
    """Initialize against a particular alphabet whose length is the radix.

    - alphabet: 0-indexed ordered radix conversion alphabet
    - input_chunk_size: how many bytes of input (N) per radix conversions
    - output_chunk_size: list of output chunk sizes for sizes 1..N bytes remaining

    For example, Base38 has:
      - input_chunk_size: 3 for 3 bytes max input at a time
      - output_chunk_sizes: [2, 4, 5] for output cluster size when 1, 2, 3 bytes left respectively
    """
    self._alphabet = alphabet
    self._payload = bytearray([])
    self._input_chunk_size = input_chunk_size
    self._output_chunk_sizes = output_chunk_sizes
    self._radix = len(alphabet)

  def _get_next_encode_chunk(self) -> Tuple[int,int]:
    if len(self._payload) == 0:
      return 0, 0

    current_chunk = bytearray(self._payload[0:self._input_chunk_size])
    self._payload = self._payload[self._input_chunk_size:]
    chunk_size = len(current_chunk)

    # Little endian conversion of scalar
    chunk_scalar = 0
    for idx in range(chunk_size):
      chunk_scalar |= (current_chunk[idx] << (8 * idx))

    return chunk_size, chunk_scalar

  def _get_next_decode_chunk(self) -> Tuple[int,str]:
    if len(self._payload) == 0:
      return 0, ""

    nominal_size = self._output_chunk_sizes[-1]
    chunk_size = min(len(self._payload), nominal_size)

    if chunk_size not in self._output_chunk_sizes:
      raise ValueError("Found incorrectly padded input (unexpected len %d remaining)" % chunk_size)

    chunk_size_bytes = self._output_chunk_sizes.index(chunk_size) + 1

    chunk = "".join(self._payload[0:chunk_size])
    self._payload = self._payload[chunk_size:]

    return chunk_size_bytes, chunk

  def encode(self, payload_octets: bytes) -> str:
    self._payload = bytearray(payload_octets)

    output = []
    chunk_size, chunk_scalar = self._get_next_encode_chunk()
    while chunk_size > 0:
      for _ in range(self._output_chunk_sizes[chunk_size - 1]):
        output.append(self._alphabet[chunk_scalar % self._radix])
        chunk_scalar = chunk_scalar // self._radix

      chunk_size, chunk_scalar = self._get_next_encode_chunk()

    if chunk_scalar != 0:
        raise ValueError("Found chunk with too many bits, remainder %d" % chunk_scalar)

    return "".join(output)

  def decode(self, payload_string: str) -> bytes:
    self._payload = list(payload_string)

    output = bytearray()

    chunk_size_bytes, chunk_chars = self._get_next_decode_chunk()
    while chunk_size_bytes > 0:

      chunk_scalar = 0

      multiplier = 1
      for char in chunk_chars:
        if char not in self._alphabet:
          raise ValueError("Found invalid alphabet character %s" % char)

        char_index = self._alphabet.index(char)
        chunk_scalar += (char_index * multiplier)
        multiplier *= self._radix

      for _ in range(chunk_size_bytes):
        output.append(chunk_scalar & 0xFF)
        chunk_scalar >>= 8

      if chunk_scalar != 0:
        raise ValueError("Found chunk with too many bits: '%s'" % chunk_chars)

      chunk_size_bytes, chunk_chars = self._get_next_decode_chunk()

    return bytes(output)


def base38_encode(payload: bytes) -> str:
  codec = BaseNCodec(MATTER_BASE38_ALPHABET, input_chunk_size=3, output_chunk_sizes=[2, 4, 5])
  return codec.encode(payload)


def base38_decode(payload: str) -> bytes:
  codec = BaseNCodec(MATTER_BASE38_ALPHABET, input_chunk_size=3, output_chunk_sizes=[2, 4, 5])
  return codec.decode(payload)


class QrPayloadConstants(enum.IntEnum):
  # See section 5.1.3. QR Code in the Matter specification
  kVersionFieldLengthInBits              = 3
  kVendorIDFieldLengthInBits             = 16
  kProductIDFieldLengthInBits            = 16
  kCommissioningFlowFieldLengthInBits    = 2
  kRendezvousInfoFieldLengthInBits       = 8
  kPayloadDiscriminatorFieldLengthInBits = 12
  kSetupPINCodeFieldLengthInBits         = 27
  kPaddingFieldLengthInBits              = 4
  kRawVendorTagLengthInBits              = 7


# See section 5.1.4. Manual Pairing Code in the Matter specification
class ManualCodeConstants(enum.IntEnum):
  kManualSetupDiscriminatorFieldLengthInBits  = 4
  kManualSetupChunk1DiscriminatorMsbitsPos    = 0
  kManualSetupChunk1DiscriminatorMsbitsLength = 2
  kManualSetupChunk1VidPidPresentBitPos = \
      (kManualSetupChunk1DiscriminatorMsbitsPos + kManualSetupChunk1DiscriminatorMsbitsLength)
  kManualSetupChunk2PINCodeLsbitsPos       = 0
  kManualSetupChunk2PINCodeLsbitsLength    = 14
  kManualSetupChunk2DiscriminatorLsbitsPos = (kManualSetupChunk2PINCodeLsbitsPos + kManualSetupChunk2PINCodeLsbitsLength)
  kManualSetupChunk2DiscriminatorLsbitsLength = 2
  kManualSetupChunk3PINCodeMsbitsPos          = 0
  kManualSetupChunk3PINCodeMsbitsLength       = 13

  kManualSetupShortCodeCharLength  = 10
  kManualSetupLongCodeCharLength   = 20
  kManualSetupVendorIdCharLength   = 5
  kManualSetupProductIdCharLength  = 5

kTotalPayloadDataSizeInBits = \
    QrPayloadConstants.kVersionFieldLengthInBits + \
    QrPayloadConstants.kVendorIDFieldLengthInBits + \
    QrPayloadConstants.kProductIDFieldLengthInBits + \
    QrPayloadConstants.kCommissioningFlowFieldLengthInBits + \
    QrPayloadConstants.kRendezvousInfoFieldLengthInBits + \
    QrPayloadConstants.kPayloadDiscriminatorFieldLengthInBits + \
    QrPayloadConstants.kSetupPINCodeFieldLengthInBits + \
    QrPayloadConstants.kPaddingFieldLengthInBits

assert (kTotalPayloadDataSizeInBits % 8) == 0
kTotalPayloadDataSizeInBytes = kTotalPayloadDataSizeInBits // 8

kQRCodePrefix = "MT:"

class RendezvousInformationFlag(enum.IntFlag):
    kNone      = 0
    kSoftAP    = 1 << 0
    kBLE       = 1 << 1
    kOnNetwork = 1 << 2

class CommissioningFlow(enum.IntEnum):
    kStandard = 0
    kUserActionRequired = 1
    kCustom = 2


"""
/**
 * A parent struct to hold onboarding payload contents without optional info,
 * for compatibility with devices that don't support std::string or STL.
 */
struct PayloadContents
{
    uint8_t version                                  = 0
    uint16_t vendorID                                = 0
    uint16_t productID                               = 0
    CommissioningFlow commissioningFlow              = CommissioningFlow::kStandard
    RendezvousInformationFlags rendezvousInformation = RendezvousInformationFlag::kNone
    uint16_t discriminator                           = 0
    uint32_t setUpPINCode                            = 0

    bool isValidQRCodePayload() const
    bool isValidManualCode() const
    bool isShortDiscriminator = false
    bool operator==(PayloadContents & input) const
}
"""

@dataclass
class MatterOnboardingPayload:
  version: int = 0
  vendor_id: int = 0
  product_id: int = 0
  commissioning_flow: CommissioningFlow = CommissioningFlow.kStandard
  rendezvous_information: RendezvousInformationFlag = RendezvousInformationFlag.kNone
  discriminator: int = 0
  setup_passcode: int = 0
  tlv_contents: bytes = None


class QrBitstreamWriter:
  def __init__(self):
    self._bits = []

  def reset(self):
    self._bits = []

  def add_scalar(self, value: int, width_bits: int, is_little_endian: bool = True):
    """Add scalar `value` as a bitstring of width `width_bits` in little_endian or big_endian."""
    bits = []
    # Record as little endian
    for bit_idx in range(width_bits):
      is_bit_set = (value & (1 << bit_idx)) != 0
      bits.append(1 if is_bit_set else 0)

    if not is_little_endian:
      # Reverse all bits for big endian
      bits = list(reversed(bits))

    self._bits.extend(bits)

  def add_bits(self, bit_value: int, num_bits: int):
    """Add `num_bits` bits of value `bit_value` to the stream """
    self._bits.extend([bit_value] * num_bits)

  def add_bytes(self, value: bytes, is_little_endian: bool = True):
    """Add all bytes of `value` byte-wise in group of 8 bits."""
    for b in value:
      self.add_scalar(value=b, width_bits=8, is_little_endian=is_little_endian)

  def get_bytes(self) -> bytes:
    """Dump the bitstring to a bytes array, zero-padded at end to nearest byte"""
    num_bits = len(self._bits)

    bytes_needed = (num_bits + 7) // 8
    padding_needed = (bytes_needed * 8) - num_bits
    self.add_bits(bit_value=0, num_bits=padding_needed)

    out_bytes = bytearray()
    for byte_idx in range(bytes_needed):
      byte_pos = byte_idx * 8
      current_byte_val = int("".join(["%d" % bit for bit in self._bits[byte_pos:byte_pos+8]]), 2)
      out_bytes.append(current_byte_val)

    return bytes(out_bytes)

  def get_bitstring(self) -> str:
    return "".join(["%d" % bit for bit in self._bits])

  def __len__(self) -> int:
    return len(self._bits)


class MatterQrCode:
  def __init__(self):
    pass

  def extract_payload(self, qr_payload: str) -> str:
    """Extract the matter-specific payload, if any"""
    fields = qr_payload.split('%')
    for field in fields:
      # Return the first field that has MT:
      if field.startswith("MT:"):
        return field[len("MT:"):]

    # If no field had the MT: prefix, we have nothing
    return ""

  def encode_payload_to_qr(self, onboarding_payload: MatterOnboardingPayload) -> str:
    bitstream = QrBitstreamWriter()

    bitstream.add_scalar(onboarding_payload.version, QrPayloadConstants.kVersionFieldLengthInBits)
    bitstream.add_scalar(onboarding_payload.vendor_id, QrPayloadConstants.kVendorIDFieldLengthInBits)
    bitstream.add_scalar(onboarding_payload.product_id, QrPayloadConstants.kProductIDFieldLengthInBits)
    bitstream.add_scalar(onboarding_payload.commissioning_flow, QrPayloadConstants.kCommissioningFlowFieldLengthInBits)
    bitstream.add_scalar(onboarding_payload.rendezvous_information, QrPayloadConstants.kRendezvousInfoFieldLengthInBits)
    bitstream.add_scalar(onboarding_payload.discriminator, QrPayloadConstants.kPayloadDiscriminatorFieldLengthInBits)
    bitstream.add_scalar(onboarding_payload.setup_passcode, QrPayloadConstants.kSetupPINCodeFieldLengthInBits)
    # Add the mandatory padding as well
    bitstream.add_bits(bit_value=0, num_bits=QrPayloadConstants.kPaddingFieldLengthInBits)

    if onboarding_payload.tlv_contents is not None:
      bitstream.add_bytes(onboarding_payload.tlv_contents)

    assert(len(bitstream) % 8 == 0)

    # QR payload has each byte of the bitstream written one by one in little-endian.
    # This is odd, but matches spec
    final_bitstream = QrBitstreamWriter()
    final_bitstream.add_bytes(bitstream.get_bytes(), is_little_endian=True)
    qr_payload_bytes = final_bitstream.get_bytes()

    return kQRCodePrefix + base38_encode(qr_payload_bytes)

  def decode_payload_from_qr(self, qr_payload: str) -> MatterOnboardingPayload:
    pass


class MatterManualPairingCode:
  def __init__(self):
    pass

  def _encode_payload_to_code(self, onboarding_payload: MatterOnboardingPayload, is_long_code: bool=False) -> str:
    code_elements = []

    discriminator = onboarding_payload.discriminator & ((1 << QrPayloadConstants.kPayloadDiscriminatorFieldLengthInBits) - 1)
    vid_pid_present = 1 if is_long_code else 0
    passcode = onboarding_payload.setup_passcode

    # DIGIT[1] := (VID_PID_PRESENT << 2) | (DISCRIMINATOR >> 10)
    digit_1 = (vid_pid_present << 2) | ((discriminator >> 10) & 0x03)
    assert digit_1 >= 0 and digit_1 <= 9
    code_elements.append("%d" % digit_1)

    # DIGIT[2..6] := ((DISCRIMINATOR & 0x300) << 6) | (PASSCODE & 0x3FFF)
    digits_2_through_6 = ((discriminator & 0x300) << 6) | (passcode & 0x3FFF)
    assert digits_2_through_6 >= 0 and digits_2_through_6 <= 65535
    code_elements.append("%05d" % digits_2_through_6)

    # DIGIT[7..10] := (PASSCODE >> 14)
    digits_7_through_10 = ((passcode >> 14) & 0x1FFF)
    assert digits_7_through_10 >= 0 and digits_7_through_10 <= 8191
    code_elements.append("%04d" % digits_7_through_10)

    if is_long_code:
      # DIGIT[11..15] := (VENDOR_ID)
      digits_11_through_15 = onboarding_payload.vendor_id & 0xFFFF
      assert digits_11_through_15 >= 0 and digits_11_through_15 <= 65535
      code_elements.append("%05d" % digits_11_through_15)

      # DIGIT[16..20] := (PRODUCT_ID)
      digits_16_through_20 = onboarding_payload.product_id & 0xFFFF
      assert digits_16_through_20 >= 0 and digits_16_through_20 <= 65535
      code_elements.append("%05d" % digits_16_through_20)

    final_code = "".join(code_elements)
    final_code += ComputeCheckChar(final_code)
    assert len(final_code) == (21 if is_long_code else 11)

    return final_code

  def encode_payload_to_short_code(self, onboarding_payload: MatterOnboardingPayload) -> str:
    return self._encode_payload_to_code(onboarding_payload, is_long_code=False)

  def encode_payload_to_long_code(self, onboarding_payload: MatterOnboardingPayload) -> str:
    return self._encode_payload_to_code(onboarding_payload, is_long_code=True)


class TestBase38(unittest.TestCase):
  def test_codec(self):
    input = bytearray([ 10, 10, 10 ])

    # Basic stuff
    self.assertEqual(base38_encode(b''), "")
    self.assertEqual(base38_encode(input[0:1]), "A0")
    self.assertEqual(base38_encode(input[0:2]), "OT10")
    self.assertEqual(base38_encode(input[0:3]), "-N.B0")

    # Test single odd byte corner conditions
    input[2] = 0
    self.assertEqual(base38_encode(input), "OT100")
    input[2] = 40
    self.assertEqual(base38_encode(input), "Y6V91")
    input[2] = 41
    self.assertEqual(base38_encode(input), "KL0B1")
    input[2] = 255
    self.assertEqual(base38_encode(input), "Q-M08")

    # Verify chunks of 1,2 and 3 bytes result in fixed-length strings padded with '0'
    # For 1 byte we need always 2 characters
    input[0] = 35
    self.assertEqual(base38_encode(input[0:1]), "Z0")

    # For 2 bytes we need always 4 characters
    input[0] = 255
    input[1] = 0
    self.assertEqual(base38_encode(input[0:2]), "R600")

    # For 3 bytes we need always 5 characters
    input[0] = 46
    input[1] = 0
    input[2] = 0
    self.assertEqual(base38_encode(input[0:3]), "81000")

    # Verify maximum available values for each chunk size to check selecting proper characters number

    # For 1 byte we need 2 characters
    input[0] = 255
    self.assertEqual(base38_encode(input[0:1]), "R6")

    # For 2 bytes we need 4 characters
    input[0] = 255
    input[1] = 255
    self.assertEqual(base38_encode(input[0:2]), "NE71")

    # For 3 bytes we need 5 characters
    input[0] = 255
    input[1] = 255
    input[2] = 255
    self.assertEqual(base38_encode(input[0:3]), "PLS18")

    encoded = base38_encode(b"Hello World!")
    self.assertEqual(encoded, "KKHF3W2S013OPM3EJX11")
    self.assertEqual(base38_decode(encoded), b"Hello World!")

    # Short input
    decoded = base38_decode("A0")
    self.assertEqual(len(decoded), 1)
    self.assertEqual(decoded[0], 10)

    # Empty == empty
    decoded = base38_decode("")
    self.assertEqual(len(decoded), 0)

    # Test invalid characters
    self.assertRaises(ValueError, base38_decode, "0\001")
    self.assertRaises(ValueError, base38_decode, "\0010")
    self.assertRaises(ValueError, base38_decode, "[0")
    self.assertRaises(ValueError, base38_decode, "0[")
    self.assertRaises(ValueError, base38_decode, " 0")
    self.assertRaises(ValueError, base38_decode, "!0")
    self.assertRaises(ValueError, base38_decode, "\"0")
    self.assertRaises(ValueError, base38_decode, "#0")
    self.assertRaises(ValueError, base38_decode, "$0")
    self.assertRaises(ValueError, base38_decode, "%0")
    self.assertRaises(ValueError, base38_decode, "&0")
    self.assertRaises(ValueError, base38_decode, "'0")
    self.assertRaises(ValueError, base38_decode, "(0")
    self.assertRaises(ValueError, base38_decode, ")0")
    self.assertRaises(ValueError, base38_decode, "*0")
    self.assertRaises(ValueError, base38_decode, "+0")
    self.assertRaises(ValueError, base38_decode, ",0")
    self.assertRaises(ValueError, base38_decode, "0")
    self.assertRaises(ValueError, base38_decode, "<0")
    self.assertRaises(ValueError, base38_decode, "=0")
    self.assertRaises(ValueError, base38_decode, ">0")
    self.assertRaises(ValueError, base38_decode, "@0")

    # Test strings that encode maximum values
    # This is 0xFF, needs 2 chars
    self.assertEqual(base38_decode("R6"), bytes([0xFF]))

    # Trying to encode 0xFF + 1 in 2 chars
    self.assertRaises(ValueError, base38_decode, "S6")

    # 0x0100, 0xFFFF
    self.assertEqual(base38_decode("S600"), bytes([0x00, 0x01]))
    self.assertEqual(base38_decode("NE71"), bytes([0xFF, 0xFF]))

    # Trying to encode 0xFFFF + 1 in 4 chars
    self.assertRaises(ValueError, base38_decode, "OE71")

    # 0x010000, 0xFFFFFF
    self.assertEqual(base38_decode("OE710"), bytes([0x00, 0x00, 0x01]))
    self.assertEqual(base38_decode("PLS18"), bytes([0xFF, 0xFF, 0xFF]))

    # Trying to encode 0xFFFFFF + 1
    self.assertRaises(ValueError, base38_decode, "QLS18")


class TestQrBitstreamWriter(unittest.TestCase):
  def test_write(self):
    writer = QrBitstreamWriter()

    writer.add_scalar(value=2, width_bits=5, is_little_endian=True)
    self.assertEqual(writer.get_bitstring(), "01000")
    self.assertEqual(len(writer), 5)
    self.assertSequenceEqual(writer.get_bytes(), b"\x40", bytes)

    writer.reset()

    writer.add_scalar(value=2, width_bits=5, is_little_endian=False)
    self.assertEqual(writer.get_bitstring(), "00010")
    self.assertEqual(len(writer), 5)
    self.assertSequenceEqual(writer.get_bytes(), b"\x10", bytes)

    writer.reset()

    writer.add_scalar(0x1dead, 17, is_little_endian=False)
    self.assertEqual(writer.get_bitstring(), "11101111010101101")
    self.assertEqual(len(writer), 17)
    self.assertSequenceEqual(writer.get_bytes(), b"\xEF\x56\x80", bytes)

    writer.reset()

    writer.add_bytes(b"\xAA\x55", is_little_endian=False)
    self.assertEqual(writer.get_bitstring(), "1010101001010101")
    self.assertEqual(len(writer), 16)
    self.assertSequenceEqual(writer.get_bytes(), b"\xAA\x55", bytes)


class TestQrCode(unittest.TestCase):
  def test_extract_payload(self):
    qr_encoder = MatterQrCode()

    self.assertEqual(qr_encoder.extract_payload("MT:ABC"), "ABC")
    self.assertEqual(qr_encoder.extract_payload("MT:"), "")
    self.assertEqual(qr_encoder.extract_payload("H:"), "")
    self.assertEqual(qr_encoder.extract_payload("ASMT:"), "")
    self.assertEqual(qr_encoder.extract_payload("Z%MT:ABC%"), "ABC")
    self.assertEqual(qr_encoder.extract_payload("Z%MT:ABC"), "ABC")
    self.assertEqual(qr_encoder.extract_payload("%Z%MT:ABC"), "ABC")
    self.assertEqual(qr_encoder.extract_payload("%Z%MT:ABC%"), "ABC")
    self.assertEqual(qr_encoder.extract_payload("%Z%MT:ABC%DDD"), "ABC")
    self.assertEqual(qr_encoder.extract_payload("MT:ABC%DDD"), "ABC")
    self.assertEqual(qr_encoder.extract_payload("MT:ABC%"), "ABC")
    self.assertEqual(qr_encoder.extract_payload("%MT:"), "")
    self.assertEqual(qr_encoder.extract_payload("%MT:%"), "")
    self.assertEqual(qr_encoder.extract_payload("A%"), "")
    self.assertEqual(qr_encoder.extract_payload("MT:%"), "")
    self.assertEqual(qr_encoder.extract_payload("%MT:ABC"), "ABC")
    self.assertEqual(qr_encoder.extract_payload("ABC"), "")

  def test_encode(self):
    # chip-tool payload generate-qrcode --discriminator 933 --setup-pin-code 20202021 --version 0 --vendor-id 0xFFF1 --product-id 0x8000 --commissioning-mode 2 --rendezvous 2 --tlvBytes hex:152C810656656E646F722C000A3132333435363738393018
    # QR Code: MT:Y.K90-II15-0064IJ3P008T706CWH3GOPM3IXZB0DK5N1K8SQ1RYCU1-A40

    qr_encoder = MatterQrCode()

    payload1 = MatterOnboardingPayload()
    payload1.discriminator = 933
    payload1.setup_passcode = 20202021
    payload1.version = 0
    payload1.vendor_id = 0xFFF1
    payload1.product_id = 0x8000
    payload1.commissioning_flow = 2
    payload1.rendezvous_information = 2
    payload1.tlv_contents = unhexlify("152C810656656E646F722C000A3132333435363738393018")

    qr_code1 = qr_encoder.encode_payload_to_qr(payload1)
    self.assertEqual(qr_code1, "MT:Y.K90-II15-0064IJ3P008T706CWH3GOPM3IXZB0DK5N1K8SQ1RYCU1-A40")

    # chip-tool payload generate-qrcode --discriminator 933 --setup-pin-code 20202021 --version 0 --vendor-id 0xa25f --product-id 0x8000 --commissioning-mode 2 --rendezvous 2
    # QR Code: MT:-A260-II15-00648G00
    payload2 = MatterOnboardingPayload()
    payload2.discriminator = 933
    payload2.setup_passcode = 20202021
    payload2.version = 0
    payload2.vendor_id = 0xa25f
    payload2.product_id = 0x8000
    payload2.commissioning_flow = 2
    payload2.rendezvous_information = 2
    payload2.tlv_contents = None

    qr_code2 = qr_encoder.encode_payload_to_qr(payload2)
    self.assertEqual(qr_code2, "MT:-A260-II15-00648G00")


class TestManualCode(unittest.TestCase):
  def test_encode(self):
    # chip-tool payload generate-manualcode --discriminator 933 --setup-pin-code 20202021 --version 0 --vendor-id 0xa25f --product-id 0x8000 --commissioning-mode 2
    # Manual Code: 449701123341567327689
    #
    manual_encoder = MatterManualPairingCode()

    payload1 = MatterOnboardingPayload()
    payload1.discriminator = 933
    payload1.setup_passcode = 20202021
    payload1.version = 0
    payload1.vendor_id = 0xa25f
    payload1.product_id = 0x8000
    payload1.commissioning_flow = 2

    manual_code1 = manual_encoder.encode_payload_to_long_code(payload1)
    self.assertEqual(manual_code1, "449701123341567327689")

    # chip-tool payload generate-manualcode --discriminator 933 --setup-pin-code 20202021 --version 0 --commissioning-mode 0
    # Manual Code: 04970112335

    payload2 = MatterOnboardingPayload()
    payload2.discriminator = 933
    payload2.setup_passcode = 20202021
    payload2.version = 0
    payload2.vendor_id = 0xa25f
    payload2.product_id = 0x8000
    payload2.commissioning_flow = 0

    manual_code1 = manual_encoder.encode_payload_to_short_code(payload2)
    self.assertEqual(manual_code1, "04970112335")


def main():
  unittest.main()


if __name__ == "__main__":
  main()