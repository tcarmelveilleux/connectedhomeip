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
package matter.controller.cluster.eventstructs

import java.util.Optional
import matter.controller.cluster.*
import matter.tlv.AnonymousTag
import matter.tlv.ContextSpecificTag
import matter.tlv.Tag
import matter.tlv.TlvReader
import matter.tlv.TlvWriter

class OperationalStateClusterOperationCompletionEvent(
  val completionErrorCode: UByte,
  val totalOperationalTime: Optional<UInt>?,
  val pausedTime: Optional<UInt>?
) {
  override fun toString(): String = buildString {
    append("OperationalStateClusterOperationCompletionEvent {\n")
    append("\tcompletionErrorCode : $completionErrorCode\n")
    append("\ttotalOperationalTime : $totalOperationalTime\n")
    append("\tpausedTime : $pausedTime\n")
    append("}\n")
  }

  fun toTlv(tlvTag: Tag, tlvWriter: TlvWriter) {
    tlvWriter.apply {
      startStructure(tlvTag)
      put(ContextSpecificTag(TAG_COMPLETION_ERROR_CODE), completionErrorCode)
      if (totalOperationalTime != null) {
        if (totalOperationalTime.isPresent) {
        val opttotalOperationalTime = totalOperationalTime.get()
        put(ContextSpecificTag(TAG_TOTAL_OPERATIONAL_TIME), opttotalOperationalTime)
      }
      } else {
        putNull(ContextSpecificTag(TAG_TOTAL_OPERATIONAL_TIME))
      }
      if (pausedTime != null) {
        if (pausedTime.isPresent) {
        val optpausedTime = pausedTime.get()
        put(ContextSpecificTag(TAG_PAUSED_TIME), optpausedTime)
      }
      } else {
        putNull(ContextSpecificTag(TAG_PAUSED_TIME))
      }
      endStructure()
    }
  }

  companion object {
    private const val TAG_COMPLETION_ERROR_CODE = 0
    private const val TAG_TOTAL_OPERATIONAL_TIME = 1
    private const val TAG_PAUSED_TIME = 2

    fun fromTlv(tlvTag: Tag, tlvReader: TlvReader) : OperationalStateClusterOperationCompletionEvent {
      tlvReader.enterStructure(tlvTag)
      val completionErrorCode = tlvReader.getUByte(ContextSpecificTag(TAG_COMPLETION_ERROR_CODE))
      val totalOperationalTime = if (!tlvReader.isNull()) {
        if (tlvReader.isNextTag(ContextSpecificTag(TAG_TOTAL_OPERATIONAL_TIME))) {
        Optional.of(tlvReader.getUInt(ContextSpecificTag(TAG_TOTAL_OPERATIONAL_TIME)))
      } else {
        Optional.empty()
      }
      } else {
        tlvReader.getNull(ContextSpecificTag(TAG_TOTAL_OPERATIONAL_TIME))
        null
      }
      val pausedTime = if (!tlvReader.isNull()) {
        if (tlvReader.isNextTag(ContextSpecificTag(TAG_PAUSED_TIME))) {
        Optional.of(tlvReader.getUInt(ContextSpecificTag(TAG_PAUSED_TIME)))
      } else {
        Optional.empty()
      }
      } else {
        tlvReader.getNull(ContextSpecificTag(TAG_PAUSED_TIME))
        null
      }
      
      tlvReader.exitContainer()

      return OperationalStateClusterOperationCompletionEvent(completionErrorCode, totalOperationalTime, pausedTime)
    }
  }
}
