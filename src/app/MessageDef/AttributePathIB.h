/**
 *
 *    Copyright (c) 2020-2021 Project CHIP Authors
 *    Copyright (c) 2016-2017 Nest Labs, Inc.
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

#include "ListBuilder.h"
#include "ListParser.h"

#include <app/AppConfig.h>
#include <app/AttributePathParams.h>
#include <app/ConcreteAttributePath.h>
#include <app/data-model/Nullable.h>
#include <app/util/basic-types.h>
#include <lib/core/CHIPCore.h>
#include <lib/core/NodeId.h>
#include <lib/core/TLV.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>

namespace chip {
namespace app {
namespace AttributePathIB {
enum class Tag : uint8_t
{
    kEnableTagCompression = 0,
    kNode                 = 1,
    kEndpoint             = 2,
    kCluster              = 3,
    kAttribute            = 4,
    kListIndex            = 5,
    kPathFlags            = 6,
};

enum class PathFlagsEnum : uint16_t
{
    kWildcardSkipRootNode                 = 1 << 0, // Skip the Root Node endpoint (endpoint 0) during wildcard expansion.
    kWildcardSkipGlobalAttributes         = 1 << 1, // Skip several large global attributes during wildcard expansion.
    kWildcardSkipAttributeList            = 1 << 2, // Skip the AttributeList global attribute during wildcard expansion.
    kWildcardSkipEventList                = 1 << 3, // Skip the EventList global attribute during wildcard expansion.
    kWildcardSkipCommandLists             = 1 << 4, // Skip the AcceptedCommandList and GeneratedCommandList global attributes during wildcard expansion.
    kWildcardSkipCustomElements           = 1 << 5, // Skip any manufacturer-specific clusters or attributes during wildcard expansion.
    kWildcardSkipFixedAttributes          = 1 << 6, // Skip any Fixed (F) quality attributes during wildcard expansion.
    kWildcardSkipChangesOmittedAttributes = 1 << 7, // Skip any Changes Omitted (C) quality attributes during wildcard expansion.
    kWildcardSkipDiagnosticsClusters      = 1 << 8, // Skip all clusters within the well-known list of diagnostics clusters during wildcard expansion.
};

enum class ValidateIdRanges : uint8_t
{
    kYes,
    kNo,
};

class Parser : public ListParser
{
public:
#if CHIP_CONFIG_IM_PRETTY_PRINT
    CHIP_ERROR PrettyPrint() const;
#endif // CHIP_CONFIG_IM_PRETTY_PRINT

    /**
     *  @brief Get the EnableTagCompression
     *
     *  @param [in] apEnableTagCompression    A pointer to apEnableTagCompression
     *
     *  @return #CHIP_NO_ERROR on success
     *          #CHIP_ERROR_WRONG_TLV_TYPE if there is such element but it's not boolean types
     *          #CHIP_END_OF_TLV if there is no such element
     */
    CHIP_ERROR GetEnableTagCompression(bool * const apEnableTagCompression) const;

    /**
     *  @brief Get the NodeId
     *
     *  @param [in] apNodeId    A pointer to apNodeId
     *
     *  @return #CHIP_NO_ERROR on success
     *          #CHIP_ERROR_WRONG_TLV_TYPE if there is such element but it's not any of the defined unsigned integer types
     *          #CHIP_END_OF_TLV if there is no such element
     */
    CHIP_ERROR GetNode(NodeId * const apNodeId) const;

    /**
     *  @brief Get the Endpoint.
     *
     *  @param [in] apEndpoint    A pointer to apEndpoint
     *
     *  @return #CHIP_NO_ERROR on success
     *          #CHIP_ERROR_WRONG_TLV_TYPE if there is such element but it's not any of the defined unsigned integer types
     *          #CHIP_END_OF_TLV if there is no such element
     */
    CHIP_ERROR GetEndpoint(EndpointId * const apEndpoint) const;

    /**
     *  @brief Get the Cluster.
     *
     *  @param [in] apCluster    A pointer to apCluster
     *
     *  @return #CHIP_NO_ERROR on success
     *          #CHIP_ERROR_WRONG_TLV_TYPE if there is such element but it's not any of the defined unsigned integer types
     *          #CHIP_END_OF_TLV if there is no such element
     */
    CHIP_ERROR GetCluster(ClusterId * const apCluster) const;

    /**
     *  @brief Get the Attribute.
     *
     *  @param [in] apAttribute    A pointer to apAttribute
     *
     *  @return #CHIP_NO_ERROR on success
     *          #CHIP_ERROR_WRONG_TLV_TYPE if there is such element but it's not any of the defined unsigned integer types
     *          #CHIP_END_OF_TLV if there is no such element
     */
    CHIP_ERROR GetAttribute(AttributeId * const apAttribute) const;

    /**
     *  @brief Get the ListIndex, the list index should not be a null value.
     *
     *  @param [in] apListIndex    A pointer to apListIndex
     *
     *  @return #CHIP_NO_ERROR on success
     *          #CHIP_ERROR_WRONG_TLV_TYPE if there is such element but it's not any of the defined unsigned integer types
     *          #CHIP_END_OF_TLV if there is no such element
     */
    CHIP_ERROR GetListIndex(ListIndex * const apListIndex) const;

    /**
     *  @brief Get the ListIndex, the list index can be a null value.
     *
     *  @param [in] apListIndex    A pointer to apListIndex
     *
     *  @return #CHIP_NO_ERROR on success
     *          #CHIP_ERROR_WRONG_TLV_TYPE if there is such element but it's not any of the defined unsigned integer types or null
     *                                     type.
     *          #CHIP_END_OF_TLV if there is no such element
     */
    CHIP_ERROR GetListIndex(DataModel::Nullable<ListIndex> * const apListIndex) const;

    /**
     *  @brief Get the PathFlags part of the AttributePathIB.
     *
     *  Note that the underlying implementation of AttributePathIB may only support 16 out of 32
     *  bits. This method will extract up to 32 bits (as per spec), but `PathFlags` type internally
     *  may be smaller.
     *
     *  @param [in] apPathFlags    A pointer to receive the read apPathFlags .
     *
     *  @return #CHIP_NO_ERROR on success
     *          #CHIP_ERROR_WRONG_TLV_TYPE if there is such element but it's not any of the defined unsigned integer types
     *          #CHIP_END_OF_TLV if there is no such element
     */
    CHIP_ERROR GetPathFlags(uint32_t * apPathFlags) const;

    /**
     * @brief Get the concrete attribute path.  This will set the ListOp to
     *        NotList when there is no ListIndex.  Consumers should interpret NotList
     *        as ReplaceAll if that's appropriate to their context.
     *
     *  @param [in] aAttributePath    The attribute path object to write to.
     *  @param [in] aValidateRanges   Whether to validate that the cluster/attribute
     *                                IDs in the path are in the right ranges.
     *
     *  @return #CHIP_NO_ERROR on success
     */
    CHIP_ERROR GetConcreteAttributePath(ConcreteDataAttributePath & aAttributePath,
                                        ValidateIdRanges aValidateRanges = ValidateIdRanges::kYes) const;

    /**
     * @brief Get a group attribute path.  This will set the ListOp to
     *        NotList when there is no ListIndex.  Consumers should interpret NotList
     *        as ReplaceAll if that's appropriate to their context.  The
     *        endpoint id of the resulting path might have any value.
     *
     *  @param [in] aAttributePath    The attribute path object to write to.
     *  @param [in] aValidateRanges   Whether to validate that the cluster/attribute
     *                                IDs in the path are in the right ranges.
     *
     *  @return #CHIP_NO_ERROR on success
     */
    CHIP_ERROR GetGroupAttributePath(ConcreteDataAttributePath & aAttributePath,
                                     ValidateIdRanges aValidateRanges = ValidateIdRanges::kYes) const;

    // TODO(#14934) Add a function to get ConcreteDataAttributePath from AttributePathIB::Parser directly.

    /**
     * @brief Parse the attribute path into an AttributePathParams object. As part of parsing,
     * validity checks for each path item will be done as well.
     *
     * If any errors are encountered, an IM error of 'InvalidAction' will be returned.
     */
    CHIP_ERROR ParsePath(AttributePathParams & attribute) const;
};

class Builder : public ListBuilder
{
public:
    /**
     *  @brief Inject EnableTagCompression into the TLV stream.
     *
     *  @param [in] aEnableTagCompression When false or not present, omission of any of the tags in question
     *              (with the exception of Node) indicates wildcard semantics. When true, indicates the use of
     *              a tag compression scheme
     *
     *  @return A reference to *this
     */
    AttributePathIB::Builder & EnableTagCompression(const bool aEnableTagCompression);

    /**
     *  @brief Inject Node into the TLV stream.
     *
     *  @param [in] aNode Node for this attribute path
     *
     *  @return A reference to *this
     */
    AttributePathIB::Builder & Node(const NodeId aNode);

    /**
     *  @brief Inject Endpoint into the TLV stream.
     *
     *  @param [in] aEndpoint Endpoint for this attribute path
     *
     *  @return A reference to *this
     */
    AttributePathIB::Builder & Endpoint(const EndpointId aEndpoint);

    /**
     *  @brief Inject Cluster into the TLV stream.
     *
     *  @param [in] aCluster Cluster for this attribute path
     *
     *  @return A reference to *this
     */
    AttributePathIB::Builder & Cluster(const ClusterId aCluster);

    /**
     *  @brief Inject Attribute into the TLV stream.
     *
     *  @param [in] aAttribute Attribute for this attribute path
     *
     *  @return A reference to *this
     */
    AttributePathIB::Builder & Attribute(const AttributeId aAttribute);

    /**
     *  @brief Inject ListIndex into the TLV stream.
     *
     *  @param [in] aListIndex ListIndex for this attribute path
     *
     *  @return A reference to *this
     */
    AttributePathIB::Builder & ListIndex(const chip::ListIndex aListIndex);
    AttributePathIB::Builder & ListIndex(const DataModel::Nullable<chip::ListIndex> & aListIndex);

    /**
     *  @brief Inject the PathFlags into the TLV stream (if non-zero).
     *
     *  @param [in] aPathFlags Path flags for this attribute path
     *
     *  @return A reference to *this
     */
    AttributePathIB::Builder & PathFlags(const chip::PathFlags aPathFlags);

    /**
     *
     *  @brief Mark the end of this AttributePathIB
     *
     *  @return The builder's final status.
     */
    CHIP_ERROR EndOfAttributePathIB();

    CHIP_ERROR Encode(const AttributePathParams & aAttributePathParams);
    CHIP_ERROR Encode(const ConcreteDataAttributePath & aAttributePathParams);
};

/**
 * @brief Determines if an endpoint ID is skipped from wildcard expansion based on PathFlags.
 *
 * @param attributePathParams - attribute path against which to check
 * @param endpointId - candidate endpoint ID
 * @return true - if the attributePathParams has an endpoint wildcard and endpointId is among the endpoints to skip
 * @return false - otherwise (i.e. not an endpoint wildcard, or it's an endpoint wildcard but we don't skip)
 */
bool HasOmittedEndpointInWildcardDueToPathFlags(const AttributePathParams & attributePathParams, EndpointId endpointId);

/**
 * @brief Determines if a cluster ID is skipped from wildcard expansion based on PathFlags.
 *
 * @param attributePathParams - attribute path against which to check
 * @param clusterId - candidate cluster ID
 * @return true - if the attributePathParams has a cluster wildcard and clusterId is among the clusters to skip
 * @return false - otherwise (i.e. not a cluster wildcard, or it's a cluster wildcard but we don't skip)
 */
bool HasOmittedClusterInWildcardDueToPathFlags(const AttributePathParams & attributePathParams, ClusterId clusterId);

/**
 * @brief Determines if an attribute is skipped from wildcard expansion based on PathFlags.
 *
 * Note that the PathFlags rules have some particular rules whereby only some combinations of
 * <Cluster, Attribute> are skipped (e.g. "C" or "F" quality) where the attribute ID alone
 * is insufficient.
 *
 * @param attributePathParams - attribute path against which to check
 * @param clusterId - candidate cluster ID
 * @param attributeId - candidate attribute ID
 * @return true - if the attributePathParams has an attribute wildcard and <clusterId,attributeId> is among the attributes to skip
 * @return false - otherwise (i.e. not an attribute wildcard, or it's an attribute wildcard but we don't skip)
 */
bool HasOmittedAttributeInWildcardDueToPathFlags(const AttributePathParams & attributePathParams, ClusterId clusterId, AttributeId attributeId);

} // namespace AttributePathIB
} // namespace app
} // namespace chip
