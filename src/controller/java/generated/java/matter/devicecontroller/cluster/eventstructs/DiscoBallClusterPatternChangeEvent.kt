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
package matter.devicecontroller.cluster.eventstructs

import java.util.Optional
import matter.devicecontroller.cluster.*
import matter.tlv.ContextSpecificTag
import matter.tlv.Tag
import matter.tlv.TlvReader
import matter.tlv.TlvWriter

class DiscoBallClusterPatternChangeEvent(
  val prevPattern: matter.devicecontroller.cluster.structs.DiscoBallClusterPatternStruct?,
  val curPattern: matter.devicecontroller.cluster.structs.DiscoBallClusterPatternStruct,
  val nextPattern: matter.devicecontroller.cluster.structs.DiscoBallClusterPatternStruct?,
  val label: Optional<String>?
) {
  override fun toString(): String = buildString {
    append("DiscoBallClusterPatternChangeEvent {\n")
    append("\tprevPattern : $prevPattern\n")
    append("\tcurPattern : $curPattern\n")
    append("\tnextPattern : $nextPattern\n")
    append("\tlabel : $label\n")
    append("}\n")
  }

  fun toTlv(tlvTag: Tag, tlvWriter: TlvWriter) {
    tlvWriter.apply {
      startStructure(tlvTag)
      if (prevPattern != null) {
        prevPattern.toTlv(ContextSpecificTag(TAG_PREV_PATTERN), this)
      } else {
        putNull(ContextSpecificTag(TAG_PREV_PATTERN))
      }
      curPattern.toTlv(ContextSpecificTag(TAG_CUR_PATTERN), this)
      if (nextPattern != null) {
        nextPattern.toTlv(ContextSpecificTag(TAG_NEXT_PATTERN), this)
      } else {
        putNull(ContextSpecificTag(TAG_NEXT_PATTERN))
      }
      if (label != null) {
        if (label.isPresent) {
          val optlabel = label.get()
          put(ContextSpecificTag(TAG_LABEL), optlabel)
        }
      } else {
        putNull(ContextSpecificTag(TAG_LABEL))
      }
      endStructure()
    }
  }

  companion object {
    private const val TAG_PREV_PATTERN = 0
    private const val TAG_CUR_PATTERN = 1
    private const val TAG_NEXT_PATTERN = 2
    private const val TAG_LABEL = 3

    fun fromTlv(tlvTag: Tag, tlvReader: TlvReader): DiscoBallClusterPatternChangeEvent {
      tlvReader.enterStructure(tlvTag)
      val prevPattern =
        if (!tlvReader.isNull()) {
          matter.devicecontroller.cluster.structs.DiscoBallClusterPatternStruct.fromTlv(
            ContextSpecificTag(TAG_PREV_PATTERN),
            tlvReader
          )
        } else {
          tlvReader.getNull(ContextSpecificTag(TAG_PREV_PATTERN))
          null
        }
      val curPattern =
        matter.devicecontroller.cluster.structs.DiscoBallClusterPatternStruct.fromTlv(
          ContextSpecificTag(TAG_CUR_PATTERN),
          tlvReader
        )
      val nextPattern =
        if (!tlvReader.isNull()) {
          matter.devicecontroller.cluster.structs.DiscoBallClusterPatternStruct.fromTlv(
            ContextSpecificTag(TAG_NEXT_PATTERN),
            tlvReader
          )
        } else {
          tlvReader.getNull(ContextSpecificTag(TAG_NEXT_PATTERN))
          null
        }
      val label =
        if (!tlvReader.isNull()) {
          if (tlvReader.isNextTag(ContextSpecificTag(TAG_LABEL))) {
            Optional.of(tlvReader.getString(ContextSpecificTag(TAG_LABEL)))
          } else {
            Optional.empty()
          }
        } else {
          tlvReader.getNull(ContextSpecificTag(TAG_LABEL))
          null
        }

      tlvReader.exitContainer()

      return DiscoBallClusterPatternChangeEvent(prevPattern, curPattern, nextPattern, label)
    }
  }
}
