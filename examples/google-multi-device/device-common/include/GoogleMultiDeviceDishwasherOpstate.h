// Copyright 2024 Google. All rights reserved.

#pragma once

#include <memory>

#include "app/clusters/operational-state-server/operational-state-server.h"
#include <lib/core/DataModelTypes.h>

namespace google {
namespace matter {

class GoogleFakeDishwasherInterface
{
  public:
    GoogleFakeDishwasherInterface(chip::EndpointId endpointId);

    chip::app::Clusters::OperationalState::Delegate* GetDelegate() { return mOpStateDelegate.get(); }

  private:
    std::unique_ptr<chip::app::Clusters::OperationalState::Delegate> mOpStateDelegate;
    chip::EndpointId mEndpointId;
};

std::unique_ptr<GoogleFakeDishwasherInterface> MakeGoogleFakeDishwasher(chip::EndpointId endpointId);

} // namespace matter
} // namespace google
