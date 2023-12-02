/*
 *
 *    Copyright (c) 2023 Project CHIP Authors
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
package matter.devicecontroller.cluster.structs

import matter.devicecontroller.cluster.*
import matter.tlv.ContextSpecificTag
import matter.tlv.Tag
import matter.tlv.TlvReader
import matter.tlv.TlvWriter

class DiscoBallClusterPatternStruct(
  val duration: UShort,
  val duration: UInt?,
  val duration: UByte?,
  val duration: UByte?,
  val duration: UByte?,
  val duration: String?
) {
  override fun toString(): String = buildString {
    append("DiscoBallClusterPatternStruct {\n")
    append("\tduration : $duration\n")
    append("\tduration : $duration\n")
    append("\tduration : $duration\n")
    append("\tduration : $duration\n")
    append("\tduration : $duration\n")
    append("\tduration : $duration\n")
    append("}\n")
  }

  fun toTlv(tlvTag: Tag, tlvWriter: TlvWriter) {
    tlvWriter.apply {
      startStructure(tlvTag)
      put(ContextSpecificTag(TAG_DURATION), duration)
      if (duration != null) {
        put(ContextSpecificTag(TAG_DURATION), duration)
      } else {
        putNull(ContextSpecificTag(TAG_DURATION))
      }
      if (duration != null) {
        put(ContextSpecificTag(TAG_DURATION), duration)
      } else {
        putNull(ContextSpecificTag(TAG_DURATION))
      }
      if (duration != null) {
        put(ContextSpecificTag(TAG_DURATION), duration)
      } else {
        putNull(ContextSpecificTag(TAG_DURATION))
      }
      if (duration != null) {
        put(ContextSpecificTag(TAG_DURATION), duration)
      } else {
        putNull(ContextSpecificTag(TAG_DURATION))
      }
      if (duration != null) {
        put(ContextSpecificTag(TAG_DURATION), duration)
      } else {
        putNull(ContextSpecificTag(TAG_DURATION))
      }
      endStructure()
    }
  }

  companion object {
    private const val TAG_DURATION = 0
    private const val TAG_DURATION = 1
    private const val TAG_DURATION = 2
    private const val TAG_DURATION = 3
    private const val TAG_DURATION = 4
    private const val TAG_DURATION = 5

    fun fromTlv(tlvTag: Tag, tlvReader: TlvReader): DiscoBallClusterPatternStruct {
      tlvReader.enterStructure(tlvTag)
      val duration = tlvReader.getUShort(ContextSpecificTag(TAG_DURATION))
      val duration =
        if (!tlvReader.isNull()) {
          tlvReader.getUInt(ContextSpecificTag(TAG_DURATION))
        } else {
          tlvReader.getNull(ContextSpecificTag(TAG_DURATION))
          null
        }
      val duration =
        if (!tlvReader.isNull()) {
          tlvReader.getUByte(ContextSpecificTag(TAG_DURATION))
        } else {
          tlvReader.getNull(ContextSpecificTag(TAG_DURATION))
          null
        }
      val duration =
        if (!tlvReader.isNull()) {
          tlvReader.getUByte(ContextSpecificTag(TAG_DURATION))
        } else {
          tlvReader.getNull(ContextSpecificTag(TAG_DURATION))
          null
        }
      val duration =
        if (!tlvReader.isNull()) {
          tlvReader.getUByte(ContextSpecificTag(TAG_DURATION))
        } else {
          tlvReader.getNull(ContextSpecificTag(TAG_DURATION))
          null
        }
      val duration =
        if (!tlvReader.isNull()) {
          tlvReader.getString(ContextSpecificTag(TAG_DURATION))
        } else {
          tlvReader.getNull(ContextSpecificTag(TAG_DURATION))
          null
        }

      tlvReader.exitContainer()

      return DiscoBallClusterPatternStruct(
        duration,
        duration,
        duration,
        duration,
        duration,
        duration
      )
    }
  }
}
