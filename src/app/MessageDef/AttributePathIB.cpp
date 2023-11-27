/**
 *
 *    Copyright (c) 2020-2021 Project CHIP Authors
 *    Copyright (c) 2018 Google LLC.
 *    Copyright (c) 2016-2017 Nest Labs, Inc.
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

#include "AttributePathIB.h"
#include "MessageDefHelper.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>

#include <app-common/zap-generated/ids/Attributes.h>

#include <app/AppConfig.h>
#include <app/data-model/Encode.h>
#include <app/data-model/Nullable.h>

namespace chip {
namespace app {

namespace {

constexpr bool IsDiagnosticsCluster(ClusterId clusterId)
{
    // The only Diagnostics clusters (K quality) at time of writing are WiFi/Thread/Ethernet/Software diagnostics
    // which are IDs in [0x0034, 0x0035, 0x0036, 0x0037]. For efficiency/ease's sake, we use this knowledge
    // to simplify the implementation.
    // TODO: Need to use a generated list of well-known diagnostics clusters managed elsewhere.
    return (clusterId >= 0x0034) && (clusterId <= 0x0037);
}

constexpr bool IsFixedAttribute(ClusterId clusterId, AttributeId attributeId)
{
    // TODO: Implement.
    return false;
}

constexpr bool IsChangesOmittedAttribute(ClusterId clusterId, AttributeId attributeId)
{
    // TODO: Implement.
    return false;
}

constexpr bool IsSkippedGlobalAttribute(AttributeId attributeId)
{

    // The only skipped global attributes at time of writing are:
    //
    // 0xFFFB: AttributeList
    // 0xFFFA: EventList
    // 0xFFF9: AcceptedCommandList
    // 0xFFF8: GeneratedCommandList
    //
    // Because of this, we can optimize the check to be the range.

    return (attributeId >= 0xFFF8) && (attributeId <= 0xFFFB);
}

} // namespace

#if CHIP_CONFIG_IM_PRETTY_PRINT
CHIP_ERROR AttributePathIB::Parser::PrettyPrint() const
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    TLV::TLVReader reader;

    PRETTY_PRINT("AttributePathIB =");
    PRETTY_PRINT("{");

    // make a copy of the Path reader
    reader.Init(mReader);

    while (CHIP_NO_ERROR == (err = reader.Next()))
    {
        if (!TLV::IsContextTag(reader.GetTag()))
        {
            continue;
        }
        uint32_t tagNum = TLV::TagNumFromTag(reader.GetTag());
        switch (tagNum)
        {
        case to_underlying(Tag::kEnableTagCompression):
#if CHIP_DETAIL_LOGGING
        {
            bool enableTagCompression;
            ReturnErrorOnFailure(reader.Get(enableTagCompression));
            PRETTY_PRINT("\tenableTagCompression = %s, ", enableTagCompression ? "true" : "false");
        }
#endif // CHIP_DETAIL_LOGGING
        break;
        case to_underlying(Tag::kNode):
            VerifyOrReturnError(TLV::kTLVType_UnsignedInteger == reader.GetType(), err = CHIP_ERROR_WRONG_TLV_TYPE);

#if CHIP_DETAIL_LOGGING
            {
                NodeId node;
                reader.Get(node);
                PRETTY_PRINT("\tNode = 0x" ChipLogFormatX64 ",", ChipLogValueX64(node));
            }
#endif // CHIP_DETAIL_LOGGING
            break;
        case to_underlying(Tag::kEndpoint):
            VerifyOrReturnError(TLV::kTLVType_UnsignedInteger == reader.GetType(), CHIP_ERROR_WRONG_TLV_TYPE);
#if CHIP_DETAIL_LOGGING
            {
                EndpointId endpoint;
                reader.Get(endpoint);
                PRETTY_PRINT("\tEndpoint = 0x%x,", endpoint);
            }
#endif // CHIP_DETAIL_LOGGING
            break;
        case to_underlying(Tag::kCluster):
            VerifyOrReturnError(TLV::kTLVType_UnsignedInteger == reader.GetType(), err = CHIP_ERROR_WRONG_TLV_TYPE);

#if CHIP_DETAIL_LOGGING
            {
                ClusterId cluster;
                ReturnErrorOnFailure(reader.Get(cluster));
                PRETTY_PRINT("\tCluster = 0x%" PRIx32 ",", cluster);
            }
#endif // CHIP_DETAIL_LOGGING
            break;
        case to_underlying(Tag::kAttribute):
            VerifyOrReturnError(TLV::kTLVType_UnsignedInteger == reader.GetType(), CHIP_ERROR_WRONG_TLV_TYPE);
#if CHIP_DETAIL_LOGGING
            {
                AttributeId attribute;
                ReturnErrorOnFailure(reader.Get(attribute));
                PRETTY_PRINT("\tAttribute = " ChipLogFormatMEI ",", ChipLogValueMEI(attribute));
            }
#endif // CHIP_DETAIL_LOGGING
            break;
        case to_underlying(Tag::kListIndex):
            VerifyOrReturnError(TLV::kTLVType_UnsignedInteger == reader.GetType() || TLV::kTLVType_Null == reader.GetType(),
                                CHIP_ERROR_WRONG_TLV_TYPE);
#if CHIP_DETAIL_LOGGING
            // We have checked the element is either uint or null
            if (TLV::kTLVType_UnsignedInteger == reader.GetType())
            {
                uint16_t listIndex;
                ReturnErrorOnFailure(reader.Get(listIndex));
                PRETTY_PRINT("\tListIndex = 0x%x,", listIndex);
            }
            else
            {
                PRETTY_PRINT("\tListIndex = Null,");
            }
#endif // CHIP_DETAIL_LOGGING
            break;
        default:
            PRETTY_PRINT("Unknown tag num %" PRIu32, tagNum);
            break;
        }
    }

    PRETTY_PRINT("}");
    PRETTY_PRINT("\t");
    // if we have exhausted this container
    if (CHIP_END_OF_TLV == err)
    {
        err = CHIP_NO_ERROR;
    }
    ReturnErrorOnFailure(err);
    return reader.ExitContainer(mOuterContainerType);
}
#endif // CHIP_CONFIG_IM_PRETTY_PRINT

CHIP_ERROR AttributePathIB::Parser::GetEnableTagCompression(bool * const apEnableTagCompression) const
{
    return GetSimpleValue(to_underlying(Tag::kEnableTagCompression), TLV::kTLVType_Boolean, apEnableTagCompression);
}

CHIP_ERROR AttributePathIB::Parser::GetNode(NodeId * const apNode) const
{
    return GetUnsignedInteger(to_underlying(Tag::kNode), apNode);
}

CHIP_ERROR AttributePathIB::Parser::GetEndpoint(EndpointId * const apEndpoint) const
{
    return GetUnsignedInteger(to_underlying(Tag::kEndpoint), apEndpoint);
}

CHIP_ERROR AttributePathIB::Parser::GetCluster(ClusterId * const apCluster) const
{
    return GetUnsignedInteger(to_underlying(Tag::kCluster), apCluster);
}

CHIP_ERROR AttributePathIB::Parser::GetAttribute(AttributeId * const apAttribute) const
{
    return GetUnsignedInteger(to_underlying(Tag::kAttribute), apAttribute);
}

CHIP_ERROR AttributePathIB::Parser::GetListIndex(ListIndex * const apListIndex) const
{
    return GetUnsignedInteger(to_underlying(Tag::kListIndex), apListIndex);
}

CHIP_ERROR AttributePathIB::Parser::GetListIndex(DataModel::Nullable<ListIndex> * const apListIndex) const
{
    return GetNullableUnsignedInteger(to_underlying(Tag::kListIndex), apListIndex);
}

CHIP_ERROR AttributePathIB::Parser::GetPathFlags(uint32_t * apPathFlags) const
{
    static_assert(sizeof(apPathFlags) >= sizeof(PathFlags), "We must be able to read at least enough bits from wire to cover PathFlags type");
    return GetUnsignedInteger(to_underlying(Tag::kPathFlags), apPathFlags);
}

CHIP_ERROR AttributePathIB::Parser::GetGroupAttributePath(ConcreteDataAttributePath & aAttributePath,
                                                          ValidateIdRanges aValidateRanges) const
{
    ReturnErrorOnFailure(GetCluster(&aAttributePath.mClusterId));
    ReturnErrorOnFailure(GetAttribute(&aAttributePath.mAttributeId));

    if (aValidateRanges == ValidateIdRanges::kYes)
    {
        VerifyOrReturnError(IsValidClusterId(aAttributePath.mClusterId), CHIP_IM_GLOBAL_STATUS(InvalidAction));
        VerifyOrReturnError(IsValidAttributeId(aAttributePath.mAttributeId), CHIP_IM_GLOBAL_STATUS(InvalidAction));
    }

    CHIP_ERROR err = CHIP_NO_ERROR;
    DataModel::Nullable<ListIndex> listIndex;
    err = GetListIndex(&(listIndex));
    if (err == CHIP_NO_ERROR)
    {
        if (listIndex.IsNull())
        {
            aAttributePath.mListOp = ConcreteDataAttributePath::ListOperation::AppendItem;
        }
        else
        {
            // TODO: Add ListOperation::ReplaceItem support. (Attribute path with valid list index)
            err = CHIP_ERROR_IM_MALFORMED_ATTRIBUTE_PATH_IB;
        }
    }
    else if (CHIP_END_OF_TLV == err)
    {
        // We do not have the context for the actual data type here. We always set the list operation to not list and the users
        // should interpret it as ReplaceAll when the attribute type is a list.
        aAttributePath.mListOp = ConcreteDataAttributePath::ListOperation::NotList;
        err                    = CHIP_NO_ERROR;
    }
    return err;
}

CHIP_ERROR AttributePathIB::Parser::GetConcreteAttributePath(ConcreteDataAttributePath & aAttributePath,
                                                             ValidateIdRanges aValidateRanges) const
{
    ReturnErrorOnFailure(GetGroupAttributePath(aAttributePath, aValidateRanges));

    // And now read our endpoint.
    return GetEndpoint(&aAttributePath.mEndpointId);
}

CHIP_ERROR AttributePathIB::Parser::ParsePath(AttributePathParams & aAttribute) const
{
    CHIP_ERROR err = CHIP_NO_ERROR;

    err = GetEndpoint(&(aAttribute.mEndpointId));
    if (err == CHIP_NO_ERROR)
    {
        VerifyOrReturnError(!aAttribute.HasWildcardEndpointId(), CHIP_IM_GLOBAL_STATUS(InvalidAction));
    }
    else if (err == CHIP_END_OF_TLV)
    {
        err = CHIP_NO_ERROR;
    }
    VerifyOrReturnError(err == CHIP_NO_ERROR, CHIP_IM_GLOBAL_STATUS(InvalidAction));

    err = GetCluster(&aAttribute.mClusterId);
    if (err == CHIP_NO_ERROR)
    {
        VerifyOrReturnError(IsValidClusterId(aAttribute.mClusterId), CHIP_IM_GLOBAL_STATUS(InvalidAction));
    }
    else if (err == CHIP_END_OF_TLV)
    {
        err = CHIP_NO_ERROR;
    }
    VerifyOrReturnError(err == CHIP_NO_ERROR, CHIP_IM_GLOBAL_STATUS(InvalidAction));

    err = GetAttribute(&aAttribute.mAttributeId);
    if (err == CHIP_NO_ERROR)
    {
        VerifyOrReturnError(IsValidAttributeId(aAttribute.mAttributeId), CHIP_IM_GLOBAL_STATUS(InvalidAction));
    }
    else if (err == CHIP_END_OF_TLV)
    {
        err = CHIP_NO_ERROR;
    }
    VerifyOrReturnError(err == CHIP_NO_ERROR, CHIP_IM_GLOBAL_STATUS(InvalidAction));

    // A wildcard cluster requires that the attribute path either be
    // wildcard or a global attribute.
    VerifyOrReturnError(!aAttribute.HasWildcardClusterId() || aAttribute.HasWildcardAttributeId() ||
                            IsGlobalAttribute(aAttribute.mAttributeId),
                        CHIP_IM_GLOBAL_STATUS(InvalidAction));

    err = GetListIndex(&aAttribute.mListIndex);
    if (err == CHIP_NO_ERROR)
    {
        VerifyOrReturnError(!aAttribute.HasWildcardAttributeId() && !aAttribute.HasWildcardListIndex(),
                            CHIP_IM_GLOBAL_STATUS(InvalidAction));
    }
    else if (err == CHIP_END_OF_TLV)
    {
        err = CHIP_NO_ERROR;
    }
    VerifyOrReturnError(err == CHIP_NO_ERROR, CHIP_IM_GLOBAL_STATUS(InvalidAction));

    uint32_t incomingPathFlags = 0;
    aAttribute.mPathFlags = 0;

    err = GetPathFlags(&incomingPathFlags);
    if (err == CHIP_NO_ERROR)
    {
        // Current implementation only knows up to 16 bits even though the wire may have up to 32 bits.
        // We just truncate all the unknown flags since would always come from a newer client and by
        // design the client has to be resilient to our ignoring them.
        aAttribute.mPathFlags = static_cast<PathFlags>(incomingPathFlags & 0xFFFFu);
    }
    else if (err == CHIP_END_OF_TLV)
    {
        err = CHIP_NO_ERROR;
    }

    VerifyOrReturnError(err == CHIP_NO_ERROR, CHIP_IM_GLOBAL_STATUS(InvalidAction));
    return CHIP_NO_ERROR;
}

AttributePathIB::Builder & AttributePathIB::Builder::EnableTagCompression(const bool aEnableTagCompression)
{
    // skip if error has already been set
    if (mError == CHIP_NO_ERROR)
    {
        mError = mpWriter->PutBoolean(TLV::ContextTag(Tag::kEnableTagCompression), aEnableTagCompression);
    }
    return *this;
}

AttributePathIB::Builder & AttributePathIB::Builder::Node(const NodeId aNode)
{
    // skip if error has already been set
    if (mError == CHIP_NO_ERROR)
    {
        mError = mpWriter->Put(TLV::ContextTag(Tag::kNode), aNode);
    }
    return *this;
}

AttributePathIB::Builder & AttributePathIB::Builder::Endpoint(const EndpointId aEndpoint)
{
    // skip if error has already been set
    if (mError == CHIP_NO_ERROR)
    {
        mError = mpWriter->Put(TLV::ContextTag(Tag::kEndpoint), aEndpoint);
    }
    return *this;
}

AttributePathIB::Builder & AttributePathIB::Builder::Cluster(const ClusterId aCluster)
{
    // skip if error has already been set
    if (mError == CHIP_NO_ERROR)
    {
        mError = mpWriter->Put(TLV::ContextTag(Tag::kCluster), aCluster);
    }
    return *this;
}

AttributePathIB::Builder & AttributePathIB::Builder::Attribute(const AttributeId aAttribute)
{
    // skip if error has already been set
    if (mError == CHIP_NO_ERROR)
    {
        mError = mpWriter->Put(TLV::ContextTag(Tag::kAttribute), aAttribute);
    }
    return *this;
}

AttributePathIB::Builder & AttributePathIB::Builder::ListIndex(const DataModel::Nullable<chip::ListIndex> & aListIndex)
{
    // skip if error has already been set
    if (mError == CHIP_NO_ERROR)
    {
        mError = DataModel::Encode(*mpWriter, TLV::ContextTag(Tag::kListIndex), aListIndex);
    }
    return *this;
}

AttributePathIB::Builder & AttributePathIB::Builder::ListIndex(const chip::ListIndex aListIndex)
{
    // skip if error has already been set
    if (mError == CHIP_NO_ERROR)
    {
        mError = mpWriter->Put(TLV::ContextTag(Tag::kListIndex), aListIndex);
    }
    return *this;
}

AttributePathIB::Builder & AttributePathIB::Builder::PathFlags(const chip::PathFlags aPathFlags)
{
    // skip if error has already been set
    if (mError == CHIP_NO_ERROR)
    {
        mError = mpWriter->Put(TLV::ContextTag(Tag::kPathFlags), aPathFlags);
    }
    return *this;
}

CHIP_ERROR AttributePathIB::Builder::EndOfAttributePathIB()
{
    EndOfContainer();
    return GetError();
}

CHIP_ERROR AttributePathIB::Builder::Encode(const AttributePathParams & aAttributePathParams)
{
    if (!(aAttributePathParams.HasWildcardEndpointId()))
    {
        Endpoint(aAttributePathParams.mEndpointId);
    }

    if (!(aAttributePathParams.HasWildcardClusterId()))
    {
        Cluster(aAttributePathParams.mClusterId);
    }

    if (!(aAttributePathParams.HasWildcardAttributeId()))
    {
        Attribute(aAttributePathParams.mAttributeId);
    }

    if (!(aAttributePathParams.HasWildcardListIndex()))
    {
        ListIndex(aAttributePathParams.mListIndex);
    }

    if (aAttributePathParams.mPathFlags != 0)
    {
        PathFlags(aAttributePathParams.mPathFlags);
    }

    return EndOfAttributePathIB();
}

CHIP_ERROR AttributePathIB::Builder::Encode(const ConcreteDataAttributePath & aAttributePath)
{
    Endpoint(aAttributePath.mEndpointId);
    Cluster(aAttributePath.mClusterId);
    Attribute(aAttributePath.mAttributeId);

    if (!aAttributePath.IsListOperation() || aAttributePath.mListOp == ConcreteDataAttributePath::ListOperation::ReplaceAll)
    {
        /* noop */
    }
    else if (aAttributePath.mListOp == ConcreteDataAttributePath::ListOperation::AppendItem)
    {
        ListIndex(DataModel::NullNullable);
    }
    else
    {
        // TODO: Add ListOperation::ReplaceItem support. (Attribute path with valid list index)
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    return EndOfAttributePathIB();
}

using PathFlagsBitmap = BitFlags<AttributePathIB::PathFlagsEnum, PathFlags>;

bool AttributePathIB::HasOmittedEndpointInWildcardDueToPathFlags(const AttributePathParams & attributePathParams, EndpointId endpointId)
{
    if (attributePathParams.mPathFlags == 0)
    {
        return false;
    }

    if (!attributePathParams.HasWildcardEndpointId())
    {
        return false;
    }

    PathFlagsBitmap flags{attributePathParams.mPathFlags};

    return flags.Has(PathFlagsEnum::kWildcardSkipRootNode) && (endpointId == 0);
}

bool AttributePathIB::HasOmittedClusterInWildcardDueToPathFlags(const AttributePathParams & attributePathParams, ClusterId clusterId)
{
    if (attributePathParams.mPathFlags == 0)
    {
        return false;
    }

    if (!attributePathParams.HasWildcardClusterId())
    {
        return false;
    }

    PathFlagsBitmap flags{attributePathParams.mPathFlags};
    // Skipping of diagnostics clusters.
    if (flags.Has(PathFlagsEnum::kWildcardSkipDiagnosticsClusters) && IsDiagnosticsCluster(clusterId))
    {
        return true;
    }

    // Skipping of MS clusters (MEI Vendor ID != 0).
    if (flags.Has(PathFlagsEnum::kWildcardSkipCustomElements) && (ExtractVendorFromMEI(clusterId) != 0))
    {
        return true;
    }

    return false;
}

bool AttributePathIB::HasOmittedAttributeInWildcardDueToPathFlags(const AttributePathParams & attributePathParams, ClusterId clusterId, AttributeId attributeId)
{
    if (attributePathParams.mPathFlags == 0)
    {
        return false;
    }

    if (!attributePathParams.HasWildcardAttributeId())
    {
        return false;
    }

    PathFlagsBitmap flags{attributePathParams.mPathFlags};

    // Handle global list attributes filter.
    if (flags.Has(PathFlagsEnum::kWildcardSkipGlobalAttributes) && (IsSkippedGlobalAttribute(attributeId)))
    {
        return true;
    }

    // Handle AttributeList filter.
    if (flags.Has(PathFlagsEnum::kWildcardSkipAttributeList) && (clusterId == Clusters::Globals::Attributes::AttributeList::Id))
    {
        return true;
    }

    // Handle Accepted/Generated command list filter.
    if (flags.Has(PathFlagsEnum::kWildcardSkipCommandLists))
    {
        if ((clusterId == Clusters::Globals::Attributes::GeneratedCommandList::Id) || (clusterId == Clusters::Globals::Attributes::AcceptedCommandList::Id))
        {
          return true;
        }
    }

    // Handle EventList filter.
#if CHIP_CONFIG_ENABLE_EVENTLIST_ATTRIBUTE
    if (flags.Has(PathFlagsEnum::kWildcardSkipEventList) && (clusterId == Clusters::Globals::Attributes::EventList::Id))
    {
        return true;
    }
#endif // CHIP_CONFIG_ENABLE_EVENTLIST_ATTRIBUTE

    // Handle skipping MS-specific clusters/attributes.
    if (flags.Has(PathFlagsEnum::kWildcardSkipCustomElements))
    {
        if ((ExtractVendorFromMEI(clusterId) != 0) || (ExtractVendorFromMEI(attributeId) != 0))
        {
            return true;
        }
    }

    // Handle Changed Omitted ('C') quality attribute filter.
    if (flags.Has(PathFlagsEnum::kWildcardSkipChangesOmittedAttributes) && IsChangesOmittedAttribute(clusterId, attributeId))
    {
        return true;
    }

    // Handle fixed ('F') quality attribute filter.
    if (flags.Has(PathFlagsEnum::kWildcardSkipFixedAttributes) && IsFixedAttribute(clusterId, attributeId))
    {
        return true;
    }

    // Got here: none of the filters applied, so we don't skip.
    return false;
}

} // namespace app
} // namespace chip
