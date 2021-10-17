/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
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

// THIS FILE IS GENERATED BY ZAP

#include <cinttypes>
#include <cstdint>

#include "app/util/util.h"
#include <app-common/zap-generated/af-structs.h>
#include <app-common/zap-generated/callback.h>
#include <app-common/zap-generated/cluster-objects.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app-common/zap-generated/ids/Commands.h>
#include <app/InteractionModelEngine.h>
#include <lib/core/CHIPSafeCasts.h>
#include <lib/support/TypeTraits.h>

// Currently we need some work to keep compatible with ember lib.
#include <app/util/ember-compatibility-functions.h>

namespace chip {
namespace app {

namespace {
void ReportCommandUnsupported(Command * aCommandObj, const ConcreteCommandPath & aCommandPath)
{
    aCommandObj->AddStatus(aCommandPath, Protocols::InteractionModel::Status::UnsupportedCommand);
    ChipLogError(Zcl, "Unknown command " ChipLogFormatMEI " for cluster " ChipLogFormatMEI,
                 ChipLogValueMEI(aCommandPath.mCommandId), ChipLogValueMEI(aCommandPath.mClusterId));
}
} // anonymous namespace

// Cluster specific command parsing

namespace Clusters {

namespace AdministratorCommissioning {

void DispatchServerCommand(CommandHandler * apCommandObj, const ConcreteCommandPath & aCommandPath, TLV::TLVReader & aDataTlv)
{
    // We are using TLVUnpackError and TLVError here since both of them can be CHIP_END_OF_TLV
    // When TLVError is CHIP_END_OF_TLV, it means we have iterated all of the items, which is not a real error.
    // Any error value TLVUnpackError means we have received an illegal value.
    // The following variables are used for all commands to save code size.
    CHIP_ERROR TLVError = CHIP_NO_ERROR;
    bool wasHandled     = false;
    {
        switch (aCommandPath.mCommandId)
        {
        case Commands::OpenBasicCommissioningWindow::Id: {
            Commands::OpenBasicCommissioningWindow::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfAdministratorCommissioningClusterOpenBasicCommissioningWindowCallback(
                    apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        case Commands::OpenCommissioningWindow::Id: {
            Commands::OpenCommissioningWindow::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfAdministratorCommissioningClusterOpenCommissioningWindowCallback(apCommandObj, aCommandPath,
                                                                                                     commandData);
            }
            break;
        }
        case Commands::RevokeCommissioning::Id: {
            Commands::RevokeCommissioning::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled =
                    emberAfAdministratorCommissioningClusterRevokeCommissioningCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        default: {
            // Unrecognized command ID, error status will apply.
            ReportCommandUnsupported(apCommandObj, aCommandPath);
            return;
        }
        }
    }

    if (CHIP_NO_ERROR != TLVError || !wasHandled)
    {
        apCommandObj->AddStatus(aCommandPath, Protocols::InteractionModel::Status::InvalidCommand);
        ChipLogProgress(Zcl, "Failed to dispatch command, TLVError=%" CHIP_ERROR_FORMAT, TLVError.Format());
    }
}

} // namespace AdministratorCommissioning

namespace DiagnosticLogs {

void DispatchServerCommand(CommandHandler * apCommandObj, const ConcreteCommandPath & aCommandPath, TLV::TLVReader & aDataTlv)
{
    // We are using TLVUnpackError and TLVError here since both of them can be CHIP_END_OF_TLV
    // When TLVError is CHIP_END_OF_TLV, it means we have iterated all of the items, which is not a real error.
    // Any error value TLVUnpackError means we have received an illegal value.
    // The following variables are used for all commands to save code size.
    CHIP_ERROR TLVError = CHIP_NO_ERROR;
    bool wasHandled     = false;
    {
        switch (aCommandPath.mCommandId)
        {
        case Commands::RetrieveLogsRequest::Id: {
            Commands::RetrieveLogsRequest::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfDiagnosticLogsClusterRetrieveLogsRequestCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        default: {
            // Unrecognized command ID, error status will apply.
            ReportCommandUnsupported(apCommandObj, aCommandPath);
            return;
        }
        }
    }

    if (CHIP_NO_ERROR != TLVError || !wasHandled)
    {
        apCommandObj->AddStatus(aCommandPath, Protocols::InteractionModel::Status::InvalidCommand);
        ChipLogProgress(Zcl, "Failed to dispatch command, TLVError=%" CHIP_ERROR_FORMAT, TLVError.Format());
    }
}

} // namespace DiagnosticLogs

namespace GeneralCommissioning {

void DispatchServerCommand(CommandHandler * apCommandObj, const ConcreteCommandPath & aCommandPath, TLV::TLVReader & aDataTlv)
{
    // We are using TLVUnpackError and TLVError here since both of them can be CHIP_END_OF_TLV
    // When TLVError is CHIP_END_OF_TLV, it means we have iterated all of the items, which is not a real error.
    // Any error value TLVUnpackError means we have received an illegal value.
    // The following variables are used for all commands to save code size.
    CHIP_ERROR TLVError = CHIP_NO_ERROR;
    bool wasHandled     = false;
    {
        switch (aCommandPath.mCommandId)
        {
        case Commands::ArmFailSafe::Id: {
            Commands::ArmFailSafe::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfGeneralCommissioningClusterArmFailSafeCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        case Commands::CommissioningComplete::Id: {
            Commands::CommissioningComplete::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled =
                    emberAfGeneralCommissioningClusterCommissioningCompleteCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        case Commands::SetRegulatoryConfig::Id: {
            Commands::SetRegulatoryConfig::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfGeneralCommissioningClusterSetRegulatoryConfigCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        default: {
            // Unrecognized command ID, error status will apply.
            ReportCommandUnsupported(apCommandObj, aCommandPath);
            return;
        }
        }
    }

    if (CHIP_NO_ERROR != TLVError || !wasHandled)
    {
        apCommandObj->AddStatus(aCommandPath, Protocols::InteractionModel::Status::InvalidCommand);
        ChipLogProgress(Zcl, "Failed to dispatch command, TLVError=%" CHIP_ERROR_FORMAT, TLVError.Format());
    }
}

} // namespace GeneralCommissioning

namespace LevelControl {

void DispatchServerCommand(CommandHandler * apCommandObj, const ConcreteCommandPath & aCommandPath, TLV::TLVReader & aDataTlv)
{
    // We are using TLVUnpackError and TLVError here since both of them can be CHIP_END_OF_TLV
    // When TLVError is CHIP_END_OF_TLV, it means we have iterated all of the items, which is not a real error.
    // Any error value TLVUnpackError means we have received an illegal value.
    // The following variables are used for all commands to save code size.
    CHIP_ERROR TLVError = CHIP_NO_ERROR;
    bool wasHandled     = false;
    {
        switch (aCommandPath.mCommandId)
        {
        case Commands::Move::Id: {
            Commands::Move::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfLevelControlClusterMoveCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        case Commands::MoveToLevel::Id: {
            Commands::MoveToLevel::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfLevelControlClusterMoveToLevelCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        case Commands::MoveToLevelWithOnOff::Id: {
            Commands::MoveToLevelWithOnOff::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfLevelControlClusterMoveToLevelWithOnOffCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        case Commands::MoveWithOnOff::Id: {
            Commands::MoveWithOnOff::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfLevelControlClusterMoveWithOnOffCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        case Commands::Step::Id: {
            Commands::Step::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfLevelControlClusterStepCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        case Commands::StepWithOnOff::Id: {
            Commands::StepWithOnOff::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfLevelControlClusterStepWithOnOffCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        case Commands::Stop::Id: {
            Commands::Stop::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfLevelControlClusterStopCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        case Commands::StopWithOnOff::Id: {
            Commands::StopWithOnOff::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfLevelControlClusterStopWithOnOffCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        default: {
            // Unrecognized command ID, error status will apply.
            ReportCommandUnsupported(apCommandObj, aCommandPath);
            return;
        }
        }
    }

    if (CHIP_NO_ERROR != TLVError || !wasHandled)
    {
        apCommandObj->AddStatus(aCommandPath, Protocols::InteractionModel::Status::InvalidCommand);
        ChipLogProgress(Zcl, "Failed to dispatch command, TLVError=%" CHIP_ERROR_FORMAT, TLVError.Format());
    }
}

} // namespace LevelControl

namespace NetworkCommissioning {

void DispatchServerCommand(CommandHandler * apCommandObj, const ConcreteCommandPath & aCommandPath, TLV::TLVReader & aDataTlv)
{
    // We are using TLVUnpackError and TLVError here since both of them can be CHIP_END_OF_TLV
    // When TLVError is CHIP_END_OF_TLV, it means we have iterated all of the items, which is not a real error.
    // Any error value TLVUnpackError means we have received an illegal value.
    // The following variables are used for all commands to save code size.
    CHIP_ERROR TLVError = CHIP_NO_ERROR;
    bool wasHandled     = false;
    {
        switch (aCommandPath.mCommandId)
        {
        case Commands::AddThreadNetwork::Id: {
            Commands::AddThreadNetwork::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfNetworkCommissioningClusterAddThreadNetworkCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        case Commands::AddWiFiNetwork::Id: {
            Commands::AddWiFiNetwork::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfNetworkCommissioningClusterAddWiFiNetworkCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        case Commands::DisableNetwork::Id: {
            Commands::DisableNetwork::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfNetworkCommissioningClusterDisableNetworkCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        case Commands::EnableNetwork::Id: {
            Commands::EnableNetwork::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfNetworkCommissioningClusterEnableNetworkCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        case Commands::GetLastNetworkCommissioningResult::Id: {
            Commands::GetLastNetworkCommissioningResult::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfNetworkCommissioningClusterGetLastNetworkCommissioningResultCallback(apCommandObj, aCommandPath,
                                                                                                         commandData);
            }
            break;
        }
        case Commands::RemoveNetwork::Id: {
            Commands::RemoveNetwork::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfNetworkCommissioningClusterRemoveNetworkCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        case Commands::ScanNetworks::Id: {
            Commands::ScanNetworks::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfNetworkCommissioningClusterScanNetworksCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        case Commands::UpdateThreadNetwork::Id: {
            Commands::UpdateThreadNetwork::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfNetworkCommissioningClusterUpdateThreadNetworkCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        case Commands::UpdateWiFiNetwork::Id: {
            Commands::UpdateWiFiNetwork::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfNetworkCommissioningClusterUpdateWiFiNetworkCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        default: {
            // Unrecognized command ID, error status will apply.
            ReportCommandUnsupported(apCommandObj, aCommandPath);
            return;
        }
        }
    }

    if (CHIP_NO_ERROR != TLVError || !wasHandled)
    {
        apCommandObj->AddStatus(aCommandPath, Protocols::InteractionModel::Status::InvalidCommand);
        ChipLogProgress(Zcl, "Failed to dispatch command, TLVError=%" CHIP_ERROR_FORMAT, TLVError.Format());
    }
}

} // namespace NetworkCommissioning

namespace OnOff {

void DispatchServerCommand(CommandHandler * apCommandObj, const ConcreteCommandPath & aCommandPath, TLV::TLVReader & aDataTlv)
{
    // We are using TLVUnpackError and TLVError here since both of them can be CHIP_END_OF_TLV
    // When TLVError is CHIP_END_OF_TLV, it means we have iterated all of the items, which is not a real error.
    // Any error value TLVUnpackError means we have received an illegal value.
    // The following variables are used for all commands to save code size.
    CHIP_ERROR TLVError = CHIP_NO_ERROR;
    bool wasHandled     = false;
    {
        switch (aCommandPath.mCommandId)
        {
        case Commands::Off::Id: {
            Commands::Off::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfOnOffClusterOffCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        case Commands::On::Id: {
            Commands::On::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfOnOffClusterOnCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        case Commands::Toggle::Id: {
            Commands::Toggle::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfOnOffClusterToggleCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        default: {
            // Unrecognized command ID, error status will apply.
            ReportCommandUnsupported(apCommandObj, aCommandPath);
            return;
        }
        }
    }

    if (CHIP_NO_ERROR != TLVError || !wasHandled)
    {
        apCommandObj->AddStatus(aCommandPath, Protocols::InteractionModel::Status::InvalidCommand);
        ChipLogProgress(Zcl, "Failed to dispatch command, TLVError=%" CHIP_ERROR_FORMAT, TLVError.Format());
    }
}

} // namespace OnOff

namespace OperationalCredentials {

void DispatchServerCommand(CommandHandler * apCommandObj, const ConcreteCommandPath & aCommandPath, TLV::TLVReader & aDataTlv)
{
    // We are using TLVUnpackError and TLVError here since both of them can be CHIP_END_OF_TLV
    // When TLVError is CHIP_END_OF_TLV, it means we have iterated all of the items, which is not a real error.
    // Any error value TLVUnpackError means we have received an illegal value.
    // The following variables are used for all commands to save code size.
    CHIP_ERROR TLVError = CHIP_NO_ERROR;
    bool wasHandled     = false;
    {
        switch (aCommandPath.mCommandId)
        {
        case Commands::AddNOC::Id: {
            Commands::AddNOC::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfOperationalCredentialsClusterAddNOCCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        case Commands::AddTrustedRootCertificate::Id: {
            Commands::AddTrustedRootCertificate::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled =
                    emberAfOperationalCredentialsClusterAddTrustedRootCertificateCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        case Commands::AttestationRequest::Id: {
            Commands::AttestationRequest::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled =
                    emberAfOperationalCredentialsClusterAttestationRequestCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        case Commands::CertificateChainRequest::Id: {
            Commands::CertificateChainRequest::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled =
                    emberAfOperationalCredentialsClusterCertificateChainRequestCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        case Commands::OpCSRRequest::Id: {
            Commands::OpCSRRequest::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfOperationalCredentialsClusterOpCSRRequestCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        case Commands::RemoveFabric::Id: {
            Commands::RemoveFabric::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfOperationalCredentialsClusterRemoveFabricCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        case Commands::RemoveTrustedRootCertificate::Id: {
            Commands::RemoveTrustedRootCertificate::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfOperationalCredentialsClusterRemoveTrustedRootCertificateCallback(apCommandObj, aCommandPath,
                                                                                                      commandData);
            }
            break;
        }
        case Commands::UpdateFabricLabel::Id: {
            Commands::UpdateFabricLabel::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfOperationalCredentialsClusterUpdateFabricLabelCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        case Commands::UpdateNOC::Id: {
            Commands::UpdateNOC::DecodableType commandData;
            TLVError = DataModel::Decode(aDataTlv, commandData);
            if (TLVError == CHIP_NO_ERROR)
            {
                wasHandled = emberAfOperationalCredentialsClusterUpdateNOCCallback(apCommandObj, aCommandPath, commandData);
            }
            break;
        }
        default: {
            // Unrecognized command ID, error status will apply.
            ReportCommandUnsupported(apCommandObj, aCommandPath);
            return;
        }
        }
    }

    if (CHIP_NO_ERROR != TLVError || !wasHandled)
    {
        apCommandObj->AddStatus(aCommandPath, Protocols::InteractionModel::Status::InvalidCommand);
        ChipLogProgress(Zcl, "Failed to dispatch command, TLVError=%" CHIP_ERROR_FORMAT, TLVError.Format());
    }
}

} // namespace OperationalCredentials

} // namespace Clusters

void DispatchSingleClusterCommand(const ConcreteCommandPath & aCommandPath, TLV::TLVReader & aReader, CommandHandler * apCommandObj)
{
    Compatibility::SetupEmberAfObjects(apCommandObj, aCommandPath);

    switch (aCommandPath.mClusterId)
    {
    case Clusters::AdministratorCommissioning::Id:
        Clusters::AdministratorCommissioning::DispatchServerCommand(apCommandObj, aCommandPath, aReader);
        break;
    case Clusters::DiagnosticLogs::Id:
        Clusters::DiagnosticLogs::DispatchServerCommand(apCommandObj, aCommandPath, aReader);
        break;
    case Clusters::GeneralCommissioning::Id:
        Clusters::GeneralCommissioning::DispatchServerCommand(apCommandObj, aCommandPath, aReader);
        break;
    case Clusters::LevelControl::Id:
        Clusters::LevelControl::DispatchServerCommand(apCommandObj, aCommandPath, aReader);
        break;
    case Clusters::NetworkCommissioning::Id:
        Clusters::NetworkCommissioning::DispatchServerCommand(apCommandObj, aCommandPath, aReader);
        break;
    case Clusters::OnOff::Id:
        Clusters::OnOff::DispatchServerCommand(apCommandObj, aCommandPath, aReader);
        break;
    case Clusters::OperationalCredentials::Id:
        Clusters::OperationalCredentials::DispatchServerCommand(apCommandObj, aCommandPath, aReader);
        break;
    default:
        ChipLogError(Zcl, "Unknown cluster " ChipLogFormatMEI, ChipLogValueMEI(aCommandPath.mClusterId));
        apCommandObj->AddStatus(aCommandPath, Protocols::InteractionModel::Status::UnsupportedCluster);
        break;
    }

    Compatibility::ResetEmberAfObjects();
}

void DispatchSingleClusterResponseCommand(const ConcreteCommandPath & aCommandPath, TLV::TLVReader & aReader,
                                          CommandSender * apCommandObj)
{
    Compatibility::SetupEmberAfObjects(apCommandObj, aCommandPath);

    TLV::TLVType dataTlvType;
    SuccessOrExit(aReader.EnterContainer(dataTlvType));
    switch (aCommandPath.mClusterId)
    {
    default:
        ChipLogError(Zcl, "Unknown cluster " ChipLogFormatMEI, ChipLogValueMEI(aCommandPath.mClusterId));
        apCommandObj->AddStatus(aCommandPath, Protocols::InteractionModel::Status::UnsupportedCluster);
        break;
    }

exit:
    aReader.ExitContainer(dataTlvType);
    Compatibility::ResetEmberAfObjects();
}

} // namespace app
} // namespace chip
