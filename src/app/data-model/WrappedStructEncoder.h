/*
 *
 *    Copyright (c) 2023 Project CHIP Authors
 *    All rights reserved.
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

#pragma once

#include <app/data-model/Encode.h>
#include <app/data-model/List.h>
#include <app/data-model/FabricScoped.h>
#include <app/data-model/Nullable.h>

#include <lib/core/DataModelTypes.h>
#include <lib/core/Optional.h>
#include <lib/core/TLV.h>
#include <protocols/interaction_model/Constants.h>

#include <type_traits>

namespace chip {
namespace app {
namespace DataModel {

class WrappedStructEncoder {
  public:
    WrappedStructEncoder(TLV::TLVWriter & writer, TLV::Tag outerTag) : mWriter(writer)
    {
      mLastError = mWriter.StartContainer(outerTag, TLV::kTLVType_Structure, mOuter);
    }

    template <typename X>
    void Encode(uint8_t contextTag, X x)
    {
        VerifyOrReturn(mLastError == CHIP_NO_ERROR);
        mLastError = DataModel::Encode(mWriter, TLV::ContextTag(contextTag), x);
    }

    CHIP_ERROR Finalize()
    {
        if (mLastError == CHIP_NO_ERROR)
        {
            mWriter.EndContainer(mOuter);
        }
        return mLastError;
    }


private:
  TLV::TLVWriter & mWriter;
  CHIP_ERROR mLastError = CHIP_NO_ERROR;
  TLV::TLVType mOuter;
};

} // namespace DataModel
} // namespace app
} // namespace chip
