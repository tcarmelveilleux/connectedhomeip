/*
 *
 *    Copyright (c) 2022 Project CHIP Authors
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

// Prevent multiple inclusion
#pragma once

#include <lib/core/CHIPConfig.h>

// Default values for the attributes longer than a pointer,
// in a form of a binary blob
// Separate block is generated for big-endian and little-endian cases.
#if BIGENDIAN_CPU
#define GENERATED_DEFAULTS                                                                                                         \
    {                                                                                                                              \
                                                                                                                                   \
        /* Endpoint: 0, Cluster: Localization Configuration (server), big-endian */                                                \
                                                                                                                                   \
        /* 0 - ActiveLocale, */                                                                                                    \
        5, 'e', 'n', '-', 'U', 'S',                                                                                                \
                                                                                                                                   \
            /* Endpoint: 0, Cluster: General Commissioning (server), big-endian */                                                 \
                                                                                                                                   \
            /* 6 - Breadcrumb, */                                                                                                  \
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                                                                        \
    }

#else // !BIGENDIAN_CPU
#define GENERATED_DEFAULTS                                                                                                         \
    {                                                                                                                              \
                                                                                                                                   \
        /* Endpoint: 0, Cluster: Localization Configuration (server), little-endian */                                             \
                                                                                                                                   \
        /* 0 - ActiveLocale, */                                                                                                    \
        5, 'e', 'n', '-', 'U', 'S',                                                                                                \
                                                                                                                                   \
            /* Endpoint: 0, Cluster: General Commissioning (server), little-endian */                                              \
                                                                                                                                   \
            /* 6 - Breadcrumb, */                                                                                                  \
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                                                                        \
    }

#endif // BIGENDIAN_CPU

#define GENERATED_DEFAULTS_COUNT (2)

#define ZAP_TYPE(type) ZCL_##type##_ATTRIBUTE_TYPE
#define ZAP_LONG_DEFAULTS_INDEX(index)                                                                                             \
    {                                                                                                                              \
        &generatedDefaults[index]                                                                                                  \
    }
#define ZAP_MIN_MAX_DEFAULTS_INDEX(index)                                                                                          \
    {                                                                                                                              \
        &minMaxDefaults[index]                                                                                                     \
    }
#define ZAP_EMPTY_DEFAULT()                                                                                                        \
    {                                                                                                                              \
        (uint32_t) 0                                                                                                               \
    }
#define ZAP_SIMPLE_DEFAULT(x)                                                                                                      \
    {                                                                                                                              \
        (uint32_t) x                                                                                                               \
    }

// This is an array of EmberAfAttributeMinMaxValue structures.
#define GENERATED_MIN_MAX_DEFAULT_COUNT 1
#define GENERATED_MIN_MAX_DEFAULTS                                                                                                 \
    {                                                                                                                              \
                                                                                                                                   \
        /* Endpoint: 0, Cluster: Time Format Localization (server) */                                                              \
        {                                                                                                                          \
            (uint16_t) 0x0, (uint16_t) 0x0, (uint16_t) 0x1                                                                         \
        } /* HourFormat */                                                                                                         \
    }

#define ZAP_ATTRIBUTE_MASK(mask) ATTRIBUTE_MASK_##mask
// This is an array of EmberAfAttributeMetadata structures.
#define GENERATED_ATTRIBUTE_COUNT 80
#define GENERATED_ATTRIBUTES                                                                                                       \
    {                                                                                                                              \
                                                                                                                                   \
        /* Endpoint: 0, Cluster: Access Control (server) */                                                                        \
        { 0x00000000, ZAP_TYPE(ARRAY), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE) | ZAP_ATTRIBUTE_MASK(WRITABLE),                     \
          ZAP_EMPTY_DEFAULT() }, /* ACL */                                                                                         \
            { 0x00000001, ZAP_TYPE(ARRAY), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE) | ZAP_ATTRIBUTE_MASK(WRITABLE),                 \
              ZAP_EMPTY_DEFAULT() }, /* Extension */                                                                               \
            { 0x00000002, ZAP_TYPE(INT16U), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE),                                               \
              ZAP_EMPTY_DEFAULT() }, /* SubjectsPerAccessControlEntry */                                                           \
            { 0x00000003, ZAP_TYPE(INT16U), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE),                                               \
              ZAP_EMPTY_DEFAULT() }, /* TargetsPerAccessControlEntry */                                                            \
            { 0x00000004, ZAP_TYPE(INT16U), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE),                                               \
              ZAP_EMPTY_DEFAULT() },                                         /* AccessControlEntriesPerFabric */                   \
            { 0x0000FFFC, ZAP_TYPE(BITMAP32), 4, 0, ZAP_SIMPLE_DEFAULT(0) }, /* FeatureMap */                                      \
            { 0x0000FFFD, ZAP_TYPE(INT16U), 2, 0, ZAP_SIMPLE_DEFAULT(1) },   /* ClusterRevision */                                 \
                                                                                                                                   \
            /* Endpoint: 0, Cluster: Basic (server) */                                                                             \
            { 0x00000000, ZAP_TYPE(INT16U), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE) | ZAP_ATTRIBUTE_MASK(SINGLETON),               \
              ZAP_EMPTY_DEFAULT() }, /* DataModelRevision */                                                                       \
            { 0x00000001, ZAP_TYPE(CHAR_STRING), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE) | ZAP_ATTRIBUTE_MASK(SINGLETON),          \
              ZAP_EMPTY_DEFAULT() }, /* VendorName */                                                                              \
            { 0x00000002, ZAP_TYPE(VENDOR_ID), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE) | ZAP_ATTRIBUTE_MASK(SINGLETON),            \
              ZAP_EMPTY_DEFAULT() }, /* VendorID */                                                                                \
            { 0x00000003, ZAP_TYPE(CHAR_STRING), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE) | ZAP_ATTRIBUTE_MASK(SINGLETON),          \
              ZAP_EMPTY_DEFAULT() }, /* ProductName */                                                                             \
            { 0x00000004, ZAP_TYPE(INT16U), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE) | ZAP_ATTRIBUTE_MASK(SINGLETON),               \
              ZAP_EMPTY_DEFAULT() }, /* ProductID */                                                                               \
            { 0x00000005, ZAP_TYPE(CHAR_STRING), 33,                                                                               \
              ZAP_ATTRIBUTE_MASK(TOKENIZE) | ZAP_ATTRIBUTE_MASK(SINGLETON) | ZAP_ATTRIBUTE_MASK(WRITABLE),                         \
              ZAP_EMPTY_DEFAULT() }, /* NodeLabel */                                                                               \
            { 0x00000006, ZAP_TYPE(CHAR_STRING), 0,                                                                                \
              ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE) | ZAP_ATTRIBUTE_MASK(SINGLETON) | ZAP_ATTRIBUTE_MASK(WRITABLE),                 \
              ZAP_EMPTY_DEFAULT() }, /* Location */                                                                                \
            { 0x00000007, ZAP_TYPE(INT16U), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE) | ZAP_ATTRIBUTE_MASK(SINGLETON),               \
              ZAP_EMPTY_DEFAULT() }, /* HardwareVersion */                                                                         \
            { 0x00000008, ZAP_TYPE(CHAR_STRING), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE) | ZAP_ATTRIBUTE_MASK(SINGLETON),          \
              ZAP_EMPTY_DEFAULT() }, /* HardwareVersionString */                                                                   \
            { 0x00000009, ZAP_TYPE(INT32U), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE) | ZAP_ATTRIBUTE_MASK(SINGLETON),               \
              ZAP_EMPTY_DEFAULT() }, /* SoftwareVersion */                                                                         \
            { 0x0000000A, ZAP_TYPE(CHAR_STRING), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE) | ZAP_ATTRIBUTE_MASK(SINGLETON),          \
              ZAP_EMPTY_DEFAULT() }, /* SoftwareVersionString */                                                                   \
            { 0x0000000B, ZAP_TYPE(CHAR_STRING), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE) | ZAP_ATTRIBUTE_MASK(SINGLETON),          \
              ZAP_EMPTY_DEFAULT() }, /* ManufacturingDate */                                                                       \
            { 0x0000000C, ZAP_TYPE(CHAR_STRING), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE) | ZAP_ATTRIBUTE_MASK(SINGLETON),          \
              ZAP_EMPTY_DEFAULT() }, /* PartNumber */                                                                              \
            { 0x0000000D, ZAP_TYPE(LONG_CHAR_STRING), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE) | ZAP_ATTRIBUTE_MASK(SINGLETON),     \
              ZAP_EMPTY_DEFAULT() }, /* ProductURL */                                                                              \
            { 0x0000000E, ZAP_TYPE(CHAR_STRING), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE) | ZAP_ATTRIBUTE_MASK(SINGLETON),          \
              ZAP_EMPTY_DEFAULT() }, /* ProductLabel */                                                                            \
            { 0x0000000F, ZAP_TYPE(CHAR_STRING), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE) | ZAP_ATTRIBUTE_MASK(SINGLETON),          \
              ZAP_EMPTY_DEFAULT() }, /* SerialNumber */                                                                            \
            { 0x00000010, ZAP_TYPE(BOOLEAN), 1,                                                                                    \
              ZAP_ATTRIBUTE_MASK(TOKENIZE) | ZAP_ATTRIBUTE_MASK(SINGLETON) | ZAP_ATTRIBUTE_MASK(WRITABLE),                         \
              ZAP_SIMPLE_DEFAULT(0) },                                                                  /* LocalConfigDisabled */  \
            { 0x00000011, ZAP_TYPE(BOOLEAN), 1, ZAP_ATTRIBUTE_MASK(SINGLETON), ZAP_SIMPLE_DEFAULT(1) }, /* Reachable */            \
            { 0x00000012, ZAP_TYPE(CHAR_STRING), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE) | ZAP_ATTRIBUTE_MASK(SINGLETON),          \
              ZAP_EMPTY_DEFAULT() },                                                                        /* UniqueID */         \
            { 0x00000013, ZAP_TYPE(STRUCT), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE), ZAP_EMPTY_DEFAULT() }, /* CapabilityMinima */ \
            { 0x0000FFFC, ZAP_TYPE(BITMAP32), 4, 0, ZAP_SIMPLE_DEFAULT(0) },                                /* FeatureMap */       \
            { 0x0000FFFD, ZAP_TYPE(INT16U), 2, ZAP_ATTRIBUTE_MASK(SINGLETON), ZAP_SIMPLE_DEFAULT(1) },      /* ClusterRevision */  \
                                                                                                                                   \
            /* Endpoint: 0, Cluster: OTA Software Update Requestor (server) */                                                     \
            { 0x00000000, ZAP_TYPE(ARRAY), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE) | ZAP_ATTRIBUTE_MASK(WRITABLE),                 \
              ZAP_EMPTY_DEFAULT() },                                                                 /* DefaultOtaProviders */     \
            { 0x00000001, ZAP_TYPE(BOOLEAN), 1, 0, ZAP_SIMPLE_DEFAULT(1) },                          /* UpdatePossible */          \
            { 0x00000002, ZAP_TYPE(ENUM8), 1, 0, ZAP_SIMPLE_DEFAULT(0) },                            /* UpdateState */             \
            { 0x00000003, ZAP_TYPE(INT8U), 1, ZAP_ATTRIBUTE_MASK(NULLABLE), ZAP_SIMPLE_DEFAULT(0) }, /* UpdateStateProgress */     \
            { 0x0000FFFC, ZAP_TYPE(BITMAP32), 4, 0, ZAP_SIMPLE_DEFAULT(0) },                         /* FeatureMap */              \
            { 0x0000FFFD, ZAP_TYPE(INT16U), 2, 0, ZAP_SIMPLE_DEFAULT(1) },                           /* ClusterRevision */         \
                                                                                                                                   \
            /* Endpoint: 0, Cluster: Localization Configuration (server) */                                                        \
            { 0x00000000, ZAP_TYPE(CHAR_STRING), 36, ZAP_ATTRIBUTE_MASK(TOKENIZE) | ZAP_ATTRIBUTE_MASK(WRITABLE),                  \
              ZAP_LONG_DEFAULTS_INDEX(0) },                                                                /* ActiveLocale */      \
            { 0x00000001, ZAP_TYPE(ARRAY), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE), ZAP_EMPTY_DEFAULT() }, /* SupportedLocales */  \
            { 0x0000FFFC, ZAP_TYPE(BITMAP32), 4, 0, ZAP_SIMPLE_DEFAULT(0) },                               /* FeatureMap */        \
            { 0x0000FFFD, ZAP_TYPE(INT16U), 2, 0, ZAP_SIMPLE_DEFAULT(1) },                                 /* ClusterRevision */   \
                                                                                                                                   \
            /* Endpoint: 0, Cluster: Time Format Localization (server) */                                                          \
            { 0x00000000, ZAP_TYPE(ENUM8), 1,                                                                                      \
              ZAP_ATTRIBUTE_MASK(MIN_MAX) | ZAP_ATTRIBUTE_MASK(TOKENIZE) | ZAP_ATTRIBUTE_MASK(WRITABLE),                           \
              ZAP_MIN_MAX_DEFAULTS_INDEX(0) }, /* HourFormat */                                                                    \
            { 0x00000001, ZAP_TYPE(ENUM8), 1, ZAP_ATTRIBUTE_MASK(TOKENIZE) | ZAP_ATTRIBUTE_MASK(WRITABLE),                         \
              ZAP_SIMPLE_DEFAULT(0) }, /* ActiveCalendarType */                                                                    \
            { 0x00000002, ZAP_TYPE(ARRAY), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE),                                                \
              ZAP_EMPTY_DEFAULT() },                                         /* SupportedCalendarTypes */                          \
            { 0x0000FFFC, ZAP_TYPE(BITMAP32), 4, 0, ZAP_SIMPLE_DEFAULT(0) }, /* FeatureMap */                                      \
            { 0x0000FFFD, ZAP_TYPE(INT16U), 2, 0, ZAP_SIMPLE_DEFAULT(1) },   /* ClusterRevision */                                 \
                                                                                                                                   \
            /* Endpoint: 0, Cluster: General Commissioning (server) */                                                             \
            { 0x00000000, ZAP_TYPE(INT64U), 8, ZAP_ATTRIBUTE_MASK(WRITABLE), ZAP_LONG_DEFAULTS_INDEX(6) }, /* Breadcrumb */        \
            { 0x00000001, ZAP_TYPE(STRUCT), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE),                                               \
              ZAP_EMPTY_DEFAULT() }, /* BasicCommissioningInfo */                                                                  \
            { 0x00000002, ZAP_TYPE(ENUM8), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE), ZAP_EMPTY_DEFAULT() }, /* RegulatoryConfig */  \
            { 0x00000003, ZAP_TYPE(ENUM8), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE),                                                \
              ZAP_EMPTY_DEFAULT() }, /* LocationCapability */                                                                      \
            { 0x00000004, ZAP_TYPE(BOOLEAN), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE),                                              \
              ZAP_EMPTY_DEFAULT() },                                         /* SupportsConcurrentConnection */                    \
            { 0x0000FFFC, ZAP_TYPE(BITMAP32), 4, 0, ZAP_SIMPLE_DEFAULT(0) }, /* FeatureMap */                                      \
            { 0x0000FFFD, ZAP_TYPE(INT16U), 2, 0, ZAP_SIMPLE_DEFAULT(1) },   /* ClusterRevision */                                 \
                                                                                                                                   \
            /* Endpoint: 0, Cluster: Network Commissioning (server) */                                                             \
            { 0x00000000, ZAP_TYPE(INT8U), 1, 0, ZAP_EMPTY_DEFAULT() },                                    /* MaxNetworks */       \
            { 0x00000001, ZAP_TYPE(ARRAY), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE), ZAP_EMPTY_DEFAULT() }, /* Networks */          \
            { 0x00000002, ZAP_TYPE(INT8U), 1, 0, ZAP_EMPTY_DEFAULT() },                              /* ScanMaxTimeSeconds */      \
            { 0x00000003, ZAP_TYPE(INT8U), 1, 0, ZAP_EMPTY_DEFAULT() },                              /* ConnectMaxTimeSeconds */   \
            { 0x00000004, ZAP_TYPE(BOOLEAN), 1, ZAP_ATTRIBUTE_MASK(WRITABLE), ZAP_EMPTY_DEFAULT() }, /* InterfaceEnabled */        \
            { 0x00000005, ZAP_TYPE(ENUM8), 1, ZAP_ATTRIBUTE_MASK(NULLABLE), ZAP_EMPTY_DEFAULT() },   /* LastNetworkingStatus */    \
            { 0x00000006, ZAP_TYPE(OCTET_STRING), 33, ZAP_ATTRIBUTE_MASK(NULLABLE), ZAP_EMPTY_DEFAULT() }, /* LastNetworkID */     \
            { 0x00000007, ZAP_TYPE(INT32S), 4, ZAP_ATTRIBUTE_MASK(NULLABLE), ZAP_EMPTY_DEFAULT() }, /* LastConnectErrorValue */    \
            { 0x0000FFFC, ZAP_TYPE(BITMAP32), 4, 0, ZAP_SIMPLE_DEFAULT(2) },                        /* FeatureMap */               \
            { 0x0000FFFD, ZAP_TYPE(INT16U), 2, 0, ZAP_SIMPLE_DEFAULT(1) },                          /* ClusterRevision */          \
                                                                                                                                   \
            /* Endpoint: 0, Cluster: AdministratorCommissioning (server) */                                                        \
            { 0x00000000, ZAP_TYPE(INT8U), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE), ZAP_EMPTY_DEFAULT() }, /* WindowStatus */      \
            { 0x00000001, ZAP_TYPE(FABRIC_IDX), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE),                                           \
              ZAP_EMPTY_DEFAULT() },                                                                        /* AdminFabricIndex */ \
            { 0x00000002, ZAP_TYPE(INT16U), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE), ZAP_EMPTY_DEFAULT() }, /* AdminVendorId */    \
            { 0x0000FFFC, ZAP_TYPE(BITMAP32), 4, 0, ZAP_SIMPLE_DEFAULT(0) },                                /* FeatureMap */       \
            { 0x0000FFFD, ZAP_TYPE(INT16U), 2, 0, ZAP_SIMPLE_DEFAULT(1) },                                  /* ClusterRevision */  \
                                                                                                                                   \
            /* Endpoint: 0, Cluster: Operational Credentials (server) */                                                           \
            { 0x00000000, ZAP_TYPE(ARRAY), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE), ZAP_EMPTY_DEFAULT() }, /* NOCs */              \
            { 0x00000001, ZAP_TYPE(ARRAY), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE), ZAP_EMPTY_DEFAULT() }, /* Fabrics */           \
            { 0x00000002, ZAP_TYPE(INT8U), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE), ZAP_EMPTY_DEFAULT() }, /* SupportedFabrics */  \
            { 0x00000003, ZAP_TYPE(INT8U), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE),                                                \
              ZAP_EMPTY_DEFAULT() }, /* CommissionedFabrics */                                                                     \
            { 0x00000004, ZAP_TYPE(ARRAY), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE),                                                \
              ZAP_EMPTY_DEFAULT() }, /* TrustedRootCertificates */                                                                 \
            { 0x00000005, ZAP_TYPE(FABRIC_IDX), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE),                                           \
              ZAP_EMPTY_DEFAULT() },                                         /* CurrentFabricIndex */                              \
            { 0x0000FFFC, ZAP_TYPE(BITMAP32), 4, 0, ZAP_SIMPLE_DEFAULT(0) }, /* FeatureMap */                                      \
            { 0x0000FFFD, ZAP_TYPE(INT16U), 2, 0, ZAP_SIMPLE_DEFAULT(1) },   /* ClusterRevision */                                 \
                                                                                                                                   \
            /* Endpoint: 0, Cluster: Fixed Label (server) */                                                                       \
            { 0x00000000, ZAP_TYPE(ARRAY), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE), ZAP_EMPTY_DEFAULT() }, /* label list */        \
            { 0x0000FFFC, ZAP_TYPE(BITMAP32), 4, 0, ZAP_SIMPLE_DEFAULT(0) },                               /* FeatureMap */        \
            { 0x0000FFFD, ZAP_TYPE(INT16U), 2, 0, ZAP_SIMPLE_DEFAULT(1) },                                 /* ClusterRevision */   \
                                                                                                                                   \
            /* Endpoint: 0, Cluster: User Label (server) */                                                                        \
            { 0x00000000, ZAP_TYPE(ARRAY), 0, ZAP_ATTRIBUTE_MASK(EXTERNAL_STORAGE) | ZAP_ATTRIBUTE_MASK(WRITABLE),                 \
              ZAP_EMPTY_DEFAULT() },                                         /* label list */                                      \
            { 0x0000FFFC, ZAP_TYPE(BITMAP32), 4, 0, ZAP_SIMPLE_DEFAULT(0) }, /* FeatureMap */                                      \
            { 0x0000FFFD, ZAP_TYPE(INT16U), 2, 0, ZAP_SIMPLE_DEFAULT(1) },   /* ClusterRevision */                                 \
    }

// This is an array of EmberAfCluster structures.
#define ZAP_ATTRIBUTE_INDEX(index) (&generatedAttributes[index])

#define ZAP_GENERATED_COMMANDS_INDEX(index) ((chip::CommandId *) (&generatedCommands[index]))

// Cluster function static arrays
#define GENERATED_FUNCTION_ARRAYS                                                                                                  \
    const EmberAfGenericClusterFunction chipFuncArrayBasicServer[] = {                                                             \
        (EmberAfGenericClusterFunction) emberAfBasicClusterServerInitCallback,                                                     \
    };                                                                                                                             \
    const EmberAfGenericClusterFunction chipFuncArrayLocalizationConfigurationServer[] = {                                         \
        (EmberAfGenericClusterFunction) emberAfLocalizationConfigurationClusterServerInitCallback,                                 \
        (EmberAfGenericClusterFunction) MatterLocalizationConfigurationClusterServerPreAttributeChangedCallback,                   \
    };                                                                                                                             \
    const EmberAfGenericClusterFunction chipFuncArrayTimeFormatLocalizationServer[] = {                                            \
        (EmberAfGenericClusterFunction) emberAfTimeFormatLocalizationClusterServerInitCallback,                                    \
        (EmberAfGenericClusterFunction) MatterTimeFormatLocalizationClusterServerPreAttributeChangedCallback,                      \
    };

// clang-format off
#define GENERATED_COMMANDS { \
  /* Endpoint: 0, Cluster: OTA Software Update Requestor (server) */\
  /*   AcceptedCommandList (index=0) */ \
  0x00000000 /* AnnounceOtaProvider */, \
  chip::kInvalidCommandId /* end of list */, \
  /* Endpoint: 0, Cluster: General Commissioning (server) */\
  /*   AcceptedCommandList (index=2) */ \
  0x00000000 /* ArmFailSafe */, \
  0x00000002 /* SetRegulatoryConfig */, \
  0x00000004 /* CommissioningComplete */, \
  chip::kInvalidCommandId /* end of list */, \
  /*   GeneratedCommandList (index=6)*/ \
  0x00000001 /* ArmFailSafeResponse */, \
  0x00000003 /* SetRegulatoryConfigResponse */, \
  0x00000005 /* CommissioningCompleteResponse */, \
  chip::kInvalidCommandId /* end of list */, \
  /* Endpoint: 0, Cluster: Network Commissioning (server) */\
  /*   AcceptedCommandList (index=10) */ \
  0x00000000 /* ScanNetworks */, \
  0x00000002 /* AddOrUpdateWiFiNetwork */, \
  0x00000003 /* AddOrUpdateThreadNetwork */, \
  0x00000004 /* RemoveNetwork */, \
  0x00000006 /* ConnectNetwork */, \
  0x00000008 /* ReorderNetwork */, \
  chip::kInvalidCommandId /* end of list */, \
  /*   GeneratedCommandList (index=17)*/ \
  0x00000001 /* ScanNetworksResponse */, \
  0x00000005 /* NetworkConfigResponse */, \
  0x00000007 /* ConnectNetworkResponse */, \
  chip::kInvalidCommandId /* end of list */, \
  /* Endpoint: 0, Cluster: AdministratorCommissioning (server) */\
  /*   AcceptedCommandList (index=21) */ \
  0x00000000 /* OpenCommissioningWindow */, \
  0x00000001 /* OpenBasicCommissioningWindow */, \
  0x00000002 /* RevokeCommissioning */, \
  chip::kInvalidCommandId /* end of list */, \
  /* Endpoint: 0, Cluster: Operational Credentials (server) */\
  /*   AcceptedCommandList (index=25) */ \
  0x00000000 /* AttestationRequest */, \
  0x00000002 /* CertificateChainRequest */, \
  0x00000004 /* CSRRequest */, \
  0x00000006 /* AddNOC */, \
  0x00000007 /* UpdateNOC */, \
  0x00000009 /* UpdateFabricLabel */, \
  0x0000000A /* RemoveFabric */, \
  0x0000000B /* AddTrustedRootCertificate */, \
  chip::kInvalidCommandId /* end of list */, \
  /*   GeneratedCommandList (index=34)*/ \
  0x00000001 /* AttestationResponse */, \
  0x00000003 /* CertificateChainResponse */, \
  0x00000005 /* CSRResponse */, \
  0x00000008 /* NOCResponse */, \
  chip::kInvalidCommandId /* end of list */, \
}

// clang-format on

#define ZAP_CLUSTER_MASK(mask) CLUSTER_MASK_##mask
#define GENERATED_CLUSTER_COUNT 12

// clang-format off
#define GENERATED_CLUSTERS { \
  { \
      /* Endpoint: 0, Cluster: Access Control (server) */ \
      .clusterId = 0x0000001F,  \
      .attributes = ZAP_ATTRIBUTE_INDEX(0), \
      .attributeCount = 7, \
      .clusterSize = 6, \
      .mask = ZAP_CLUSTER_MASK(SERVER), \
      .functions = NULL, \
      .acceptedCommandList = nullptr ,\
      .generatedCommandList = nullptr ,\
    },\
  { \
      /* Endpoint: 0, Cluster: Basic (server) */ \
      .clusterId = 0x00000028,  \
      .attributes = ZAP_ATTRIBUTE_INDEX(7), \
      .attributeCount = 22, \
      .clusterSize = 41, \
      .mask = ZAP_CLUSTER_MASK(SERVER) | ZAP_CLUSTER_MASK(INIT_FUNCTION), \
      .functions = chipFuncArrayBasicServer, \
      .acceptedCommandList = nullptr ,\
      .generatedCommandList = nullptr ,\
    },\
  { \
      /* Endpoint: 0, Cluster: OTA Software Update Provider (client) */ \
      .clusterId = 0x00000029,  \
      .attributes = ZAP_ATTRIBUTE_INDEX(29), \
      .attributeCount = 0, \
      .clusterSize = 0, \
      .mask = ZAP_CLUSTER_MASK(CLIENT), \
      .functions = NULL, \
      .acceptedCommandList = nullptr ,\
      .generatedCommandList = nullptr ,\
    },\
  { \
      /* Endpoint: 0, Cluster: OTA Software Update Requestor (server) */ \
      .clusterId = 0x0000002A,  \
      .attributes = ZAP_ATTRIBUTE_INDEX(29), \
      .attributeCount = 6, \
      .clusterSize = 9, \
      .mask = ZAP_CLUSTER_MASK(SERVER), \
      .functions = NULL, \
      .acceptedCommandList = ZAP_GENERATED_COMMANDS_INDEX( 0 ) ,\
      .generatedCommandList = nullptr ,\
    },\
  { \
      /* Endpoint: 0, Cluster: Localization Configuration (server) */ \
      .clusterId = 0x0000002B,  \
      .attributes = ZAP_ATTRIBUTE_INDEX(35), \
      .attributeCount = 4, \
      .clusterSize = 42, \
      .mask = ZAP_CLUSTER_MASK(SERVER) | ZAP_CLUSTER_MASK(INIT_FUNCTION) | ZAP_CLUSTER_MASK(PRE_ATTRIBUTE_CHANGED_FUNCTION), \
      .functions = chipFuncArrayLocalizationConfigurationServer, \
      .acceptedCommandList = nullptr ,\
      .generatedCommandList = nullptr ,\
    },\
  { \
      /* Endpoint: 0, Cluster: Time Format Localization (server) */ \
      .clusterId = 0x0000002C,  \
      .attributes = ZAP_ATTRIBUTE_INDEX(39), \
      .attributeCount = 5, \
      .clusterSize = 8, \
      .mask = ZAP_CLUSTER_MASK(SERVER) | ZAP_CLUSTER_MASK(INIT_FUNCTION) | ZAP_CLUSTER_MASK(PRE_ATTRIBUTE_CHANGED_FUNCTION), \
      .functions = chipFuncArrayTimeFormatLocalizationServer, \
      .acceptedCommandList = nullptr ,\
      .generatedCommandList = nullptr ,\
    },\
  { \
      /* Endpoint: 0, Cluster: General Commissioning (server) */ \
      .clusterId = 0x00000030,  \
      .attributes = ZAP_ATTRIBUTE_INDEX(44), \
      .attributeCount = 7, \
      .clusterSize = 14, \
      .mask = ZAP_CLUSTER_MASK(SERVER), \
      .functions = NULL, \
      .acceptedCommandList = ZAP_GENERATED_COMMANDS_INDEX( 2 ) ,\
      .generatedCommandList = ZAP_GENERATED_COMMANDS_INDEX( 6 ) ,\
    },\
  { \
      /* Endpoint: 0, Cluster: Network Commissioning (server) */ \
      .clusterId = 0x00000031,  \
      .attributes = ZAP_ATTRIBUTE_INDEX(51), \
      .attributeCount = 10, \
      .clusterSize = 48, \
      .mask = ZAP_CLUSTER_MASK(SERVER), \
      .functions = NULL, \
      .acceptedCommandList = ZAP_GENERATED_COMMANDS_INDEX( 10 ) ,\
      .generatedCommandList = ZAP_GENERATED_COMMANDS_INDEX( 17 ) ,\
    },\
  { \
      /* Endpoint: 0, Cluster: AdministratorCommissioning (server) */ \
      .clusterId = 0x0000003C,  \
      .attributes = ZAP_ATTRIBUTE_INDEX(61), \
      .attributeCount = 5, \
      .clusterSize = 6, \
      .mask = ZAP_CLUSTER_MASK(SERVER), \
      .functions = NULL, \
      .acceptedCommandList = ZAP_GENERATED_COMMANDS_INDEX( 21 ) ,\
      .generatedCommandList = nullptr ,\
    },\
  { \
      /* Endpoint: 0, Cluster: Operational Credentials (server) */ \
      .clusterId = 0x0000003E,  \
      .attributes = ZAP_ATTRIBUTE_INDEX(66), \
      .attributeCount = 8, \
      .clusterSize = 6, \
      .mask = ZAP_CLUSTER_MASK(SERVER), \
      .functions = NULL, \
      .acceptedCommandList = ZAP_GENERATED_COMMANDS_INDEX( 25 ) ,\
      .generatedCommandList = ZAP_GENERATED_COMMANDS_INDEX( 34 ) ,\
    },\
  { \
      /* Endpoint: 0, Cluster: Fixed Label (server) */ \
      .clusterId = 0x00000040,  \
      .attributes = ZAP_ATTRIBUTE_INDEX(74), \
      .attributeCount = 3, \
      .clusterSize = 6, \
      .mask = ZAP_CLUSTER_MASK(SERVER), \
      .functions = NULL, \
      .acceptedCommandList = nullptr ,\
      .generatedCommandList = nullptr ,\
    },\
  { \
      /* Endpoint: 0, Cluster: User Label (server) */ \
      .clusterId = 0x00000041,  \
      .attributes = ZAP_ATTRIBUTE_INDEX(77), \
      .attributeCount = 3, \
      .clusterSize = 6, \
      .mask = ZAP_CLUSTER_MASK(SERVER), \
      .functions = NULL, \
      .acceptedCommandList = nullptr ,\
      .generatedCommandList = nullptr ,\
    },\
}

// clang-format on

#define ZAP_CLUSTER_INDEX(index) (&generatedClusters[index])

#define ZAP_FIXED_ENDPOINT_DATA_VERSION_COUNT 11

// This is an array of EmberAfEndpointType structures.
#define GENERATED_ENDPOINT_TYPES                                                                                                   \
    {                                                                                                                              \
        { ZAP_CLUSTER_INDEX(0), 12, 192 },                                                                                         \
    }

// Largest attribute size is needed for various buffers
#define ATTRIBUTE_LARGEST (259)

static_assert(ATTRIBUTE_LARGEST <= CHIP_CONFIG_MAX_ATTRIBUTE_STORE_ELEMENT_SIZE, "ATTRIBUTE_LARGEST larger than expected");

// Total size of singleton attributes
#define ATTRIBUTE_SINGLETONS_SIZE (37)

// Total size of attribute storage
#define ATTRIBUTE_MAX_SIZE (192)

// Number of fixed endpoints
#define FIXED_ENDPOINT_COUNT (1)

// Array of endpoints that are supported, the data inside
// the array is the endpoint number.
#define FIXED_ENDPOINT_ARRAY                                                                                                       \
    {                                                                                                                              \
        0x0000                                                                                                                     \
    }

// Array of profile ids
#define FIXED_PROFILE_IDS                                                                                                          \
    {                                                                                                                              \
        0x0103                                                                                                                     \
    }

// Array of device types
#define FIXED_DEVICE_TYPES                                                                                                         \
    {                                                                                                                              \
        {                                                                                                                          \
            0x0016, 1                                                                                                              \
        }                                                                                                                          \
    }

// Array of device type offsets
#define FIXED_DEVICE_TYPE_OFFSETS                                                                                                  \
    {                                                                                                                              \
        0                                                                                                                          \
    }

// Array of device type lengths
#define FIXED_DEVICE_TYPE_LENGTHS                                                                                                  \
    {                                                                                                                              \
        1                                                                                                                          \
    }

// Array of endpoint types supported on each endpoint
#define FIXED_ENDPOINT_TYPES                                                                                                       \
    {                                                                                                                              \
        0                                                                                                                          \
    }

// Array of networks supported on each endpoint
#define FIXED_NETWORKS                                                                                                             \
    {                                                                                                                              \
        0                                                                                                                          \
    }
