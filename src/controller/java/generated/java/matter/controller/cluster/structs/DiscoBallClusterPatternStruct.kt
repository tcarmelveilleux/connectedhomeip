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
package matter.controller.cluster.structs

import java.util.Optional
import matter.controller.cluster.*
import matter.tlv.ContextSpecificTag
import matter.tlv.Tag
import matter.tlv.TlvReader
import matter.tlv.TlvWriter

class DiscoBallClusterPatternStruct(
  val duration: UShort,
  val rotate: UByte?,
  val speed: UByte?,
  val axis: Optional<UByte>?,
  val wobbleSpeed: Optional<UByte>?,
  val passcode: String?,
  val fabricIndex: UByte
) {
  override fun toString(): String = buildString {
    append("DiscoBallClusterPatternStruct {\n")
    append("\tduration : $duration\n")
    append("\trotate : $rotate\n")
    append("\tspeed : $speed\n")
    append("\taxis : $axis\n")
    append("\twobbleSpeed : $wobbleSpeed\n")
    append("\tpasscode : $passcode\n")
    append("\tfabricIndex : $fabricIndex\n")
    append("}\n")
  }

  fun toTlv(tlvTag: Tag, tlvWriter: TlvWriter) {
    tlvWriter.apply {
      startStructure(tlvTag)
      put(ContextSpecificTag(TAG_DURATION), duration)
      if (rotate != null) {
        put(ContextSpecificTag(TAG_ROTATE), rotate)
      } else {
        putNull(ContextSpecificTag(TAG_ROTATE))
      }
      if (speed != null) {
        put(ContextSpecificTag(TAG_SPEED), speed)
      } else {
        putNull(ContextSpecificTag(TAG_SPEED))
      }
      if (axis != null) {
        if (axis.isPresent) {
          val optaxis = axis.get()
          put(ContextSpecificTag(TAG_AXIS), optaxis)
        }
      } else {
        putNull(ContextSpecificTag(TAG_AXIS))
      }
      if (wobbleSpeed != null) {
        if (wobbleSpeed.isPresent) {
          val optwobbleSpeed = wobbleSpeed.get()
          put(ContextSpecificTag(TAG_WOBBLE_SPEED), optwobbleSpeed)
        }
      } else {
        putNull(ContextSpecificTag(TAG_WOBBLE_SPEED))
      }
      if (passcode != null) {
        put(ContextSpecificTag(TAG_PASSCODE), passcode)
      } else {
        putNull(ContextSpecificTag(TAG_PASSCODE))
      }
      put(ContextSpecificTag(TAG_FABRIC_INDEX), fabricIndex)
      endStructure()
    }
  }

  companion object {
    private const val TAG_DURATION = 0
    private const val TAG_ROTATE = 1
    private const val TAG_SPEED = 2
    private const val TAG_AXIS = 3
    private const val TAG_WOBBLE_SPEED = 4
    private const val TAG_PASSCODE = 5
    private const val TAG_FABRIC_INDEX = 254

    fun fromTlv(tlvTag: Tag, tlvReader: TlvReader): DiscoBallClusterPatternStruct {
      tlvReader.enterStructure(tlvTag)
      val duration = tlvReader.getUShort(ContextSpecificTag(TAG_DURATION))
      val rotate =
        if (!tlvReader.isNull()) {
          tlvReader.getUByte(ContextSpecificTag(TAG_ROTATE))
        } else {
          tlvReader.getNull(ContextSpecificTag(TAG_ROTATE))
          null
        }
      val speed =
        if (!tlvReader.isNull()) {
          tlvReader.getUByte(ContextSpecificTag(TAG_SPEED))
        } else {
          tlvReader.getNull(ContextSpecificTag(TAG_SPEED))
          null
        }
      val axis =
        if (!tlvReader.isNull()) {
          if (tlvReader.isNextTag(ContextSpecificTag(TAG_AXIS))) {
            Optional.of(tlvReader.getUByte(ContextSpecificTag(TAG_AXIS)))
          } else {
            Optional.empty()
          }
        } else {
          tlvReader.getNull(ContextSpecificTag(TAG_AXIS))
          null
        }
      val wobbleSpeed =
        if (!tlvReader.isNull()) {
          if (tlvReader.isNextTag(ContextSpecificTag(TAG_WOBBLE_SPEED))) {
            Optional.of(tlvReader.getUByte(ContextSpecificTag(TAG_WOBBLE_SPEED)))
          } else {
            Optional.empty()
          }
        } else {
          tlvReader.getNull(ContextSpecificTag(TAG_WOBBLE_SPEED))
          null
        }
      val passcode =
        if (!tlvReader.isNull()) {
          tlvReader.getString(ContextSpecificTag(TAG_PASSCODE))
        } else {
          tlvReader.getNull(ContextSpecificTag(TAG_PASSCODE))
          null
        }
      val fabricIndex = tlvReader.getUByte(ContextSpecificTag(TAG_FABRIC_INDEX))

      tlvReader.exitContainer()

      return DiscoBallClusterPatternStruct(
        duration,
        rotate,
        speed,
        axis,
        wobbleSpeed,
        passcode,
        fabricIndex
      )
    }
  }
}
