// Copyright 2024 Google. All rights reserved.

#include "GoogleMultiDeviceInfoProvider.h"

#include "GoogleMultiDeviceAttestationProvider.h"

#include <include/platform/DeviceInstanceInfoProvider.h>
#include <lib/support/CodeUtils.h>

namespace google {
namespace matter {

namespace {

GoogleMultiDeviceInfoProvider sDeviceInfoProvider;
GoogleMultiDeviceAttestationProvider sAttestationProvider;

} // namespace

void InitializeProduct()
{
    chip::DeviceLayer::SetDeviceInstanceInfoProvider(&sDeviceInfoProvider);
    chip::Credentials::SetDeviceAttestationCredentialsProvider(&sAttestationProvider);
}

} // namespace matter
} // namespace google
