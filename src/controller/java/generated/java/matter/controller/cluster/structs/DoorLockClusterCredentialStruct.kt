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
import matter.tlv.AnonymousTag
import matter.tlv.ContextSpecificTag
import matter.tlv.Tag
import matter.tlv.TlvReader
import matter.tlv.TlvWriter

class DoorLockClusterCredentialStruct(
  val credentialType: UByte,
  val credentialIndex: UShort
) {
  override fun toString(): String = buildString {
    append("DoorLockClusterCredentialStruct {\n")
    append("\tcredentialType : $credentialType\n")
    append("\tcredentialIndex : $credentialIndex\n")
    append("}\n")
  }

  fun toTlv(tlvTag: Tag, tlvWriter: TlvWriter) {
    tlvWriter.apply {
      startStructure(tlvTag)
      put(ContextSpecificTag(TAG_CREDENTIAL_TYPE), credentialType)
      put(ContextSpecificTag(TAG_CREDENTIAL_INDEX), credentialIndex)
      endStructure()
    }
  }

  companion object {
    private const val TAG_CREDENTIAL_TYPE = 0
    private const val TAG_CREDENTIAL_INDEX = 1

    fun fromTlv(tlvTag: Tag, tlvReader: TlvReader): DoorLockClusterCredentialStruct {
      tlvReader.enterStructure(tlvTag)
      val credentialType = tlvReader.getUByte(ContextSpecificTag(TAG_CREDENTIAL_TYPE))
      val credentialIndex = tlvReader.getUShort(ContextSpecificTag(TAG_CREDENTIAL_INDEX))
      
      tlvReader.exitContainer()

      return DoorLockClusterCredentialStruct(credentialType, credentialIndex)
    }
  }
}
