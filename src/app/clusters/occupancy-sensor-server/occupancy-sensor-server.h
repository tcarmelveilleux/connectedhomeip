/*
 *
 *    Copyright (c) 2020-2024 Project CHIP Authors
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

#include <functional>

#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/cluster-enums.h>
#include <app-common/zap-generated/cluster-objects.h>
#include <app/AttributeAccessInterface.h>
#include <app/util/af-types.h>
#include <app/util/basic-types.h>
#include <app/util/config.h>
#include <lib/core/DataModelTypes.h>

namespace chip {
namespace app {
namespace Clusters {
namespace OccupancySensing {

class Instance : public AttributeAccessInterface
{
public:
    // Delegate/driver class for the occupancy cluster. Instantiate one per
    // endpoint. Your implementation should provide the correct values for all
    // members.
    class Delegate
    {
      public:
        Delegate() = default;

        // Not copyable.
        Delegate(const Delegate &)             = delete;
        Delegate & operator=(const Delegate &) = delete;

        virtual ~Delegate() = default;

        // ============= Cluster to driver interface ==========
        /**
         * Called whenever the cluster is initialized. This MAY be used
         * to initialize the occupancy sensor driver. The driver may be
         * otherwise initialized separately without any overrides.
         */
        virtual void Init() {}

        /**
         * Called whenever HoldTime is updated via Matter.
         *
         * NOTE: The hold time timer (if needed) is managed by the cluster Instance
         *       so the value provided here is merely for informational purpose for
         *       the product to know about it.
         */
        virtual void OnHoldTimeWrite(uint16_t holdTimeSeconds) = 0;

        /**
         * Called whenever Occupancy attribute changes (i.e. including hold time handling).
         */
        virtual void OnOccupancyChange(BitMask<OccupancyBitmap> occupancy) = 0;

        /**
         * Query the supported features for the given delegate.
         */
        virtual BitFlags<Feature> GetSupportedFeatures() = 0;

        /**
         * Getter to obtain the current hold time limits for the product.
         *
         * @return the actual hold time limits values.
         */
        virtual Structs::HoldTimeLimitsStruct::Type GetHoldTimeLimits() const = 0;

        /**
         * @brief Start a hold timer for `numSeconds` seconds, that will must callback in Matter context when expired.
         *
         * @returns true on success, false on failure.
         */
        virtual bool StartHoldTimer(uint16_t numSeconds, std::function<void()> callback) = 0;

        // Called by the instance when it's initializing.
        void SetInstance(Instance *instance) { mInstance = instance; }

        // ============= Call-in interface from delegate to cluster ==========
        /**
         * Call (from Matter stack context) whenever occupancy detection changes.
         * This will drive the cluster's HoldTime logic.
         *
         * @param detected - true if occupancy is currently detected, false otherwise.
         */
        virtual void SetOccupancyDetectedFromSensor(bool detected)
        {
            VerifyOrReturn(mInstance != nullptr);
            mInstance->SetOccupancyDetectedFromSensor(detected);
        }

        /**
         * Call (from Matter stack context) whenever the hold time needs to be
         * updated from the sensor due to changes in internal local.
         *
         * The value of the hold time will be clamped to the HoldTimeLimits.
         *
         * @param newHoldTimeSeconds - new hold time in seconds.
         */
        virtual void UpdateHoldTimeFromSensor(uint16_t newHoldTimeSeconds)
        {
            VerifyOrReturn(mInstance != nullptr);
            mInstance->UpdateHoldTimeFromSensor(newHoldTimeSeconds);
        }

      protected:
        Instance *mInstance = nullptr;
        std::function<void()> mHoldTimerCallback;
    };

    Instance(
      Delegate * delegate, EndpointId endpointId) :
        app::AttributeAccessInterface(MakeOptional(endpointId), app::Clusters::OccupancySensing::Id), mEndpointId(endpointId), mDelegate(delegate)
    {}

    ~Instance() override;

    // Not copyable.
    Instance(const Instance &)             = delete;
    Instance & operator=(const Instance &) = delete;

    /**
     * Initialize the server instance.
     *
     * This function must be called after defining an Instance class object.
     *
     * @return Returns an error if the given endpoint and cluster ID have not been enabled in zap or if the
     * CommandHandler or AttributeHandler registration fails, else returns CHIP_NO_ERROR.
     */
    CHIP_ERROR Init();

    // AttributeAccessInterface overrides
    CHIP_ERROR Read(const ConcreteReadAttributePath & aPath, AttributeValueEncoder & aEncoder) override;
    CHIP_ERROR Write(const ConcreteDataAttributePath & aPath, AttributeValueDecoder & aDecoder) override;

    // ============= Call-in interface from driver to cluster ==========

    /**
     * Called by the delegate on detection edges.
     *
     * NOTE: The cluster logic manages the hold time.
     *
     * @param detected - true if occupancy is currently detected, false otherwise.
     */
    void SetOccupancyDetectedFromSensor(bool detected);

    /**
     * Called by the delegate when something on the side of the product is configured
     * in a way that the hold time needs to be updated.
     *
     * @param newHoldTimeSeconds - new value for the hold time.
     */
    void UpdateHoldTimeFromSensor(uint16_t newHoldTimeSeconds);
private:
    void InternalSetOccupancy(bool occupied);
    void HandleHoldTimerExpiry();
    CHIP_ERROR HandleWriteHoldTime(uint16_t newHoldTimeSeconds);

    EndpointId mEndpointId;
    Delegate * mDelegate;
    BitFlags<Feature> mFeatures{};
    bool mHasHoldTime = false;
    bool mHasPirOccupiedToUnoccupiedDelaySeconds = false;
    uint16_t mHoldTimeSeconds = 0; // When set to zero, no hold timer will be called.
    bool mOccupancyDetected = false;
    bool mIsHoldTimerRunning = false;
    bool mLastOccupancyDetectedFromDelegate = false;
};

} // namespace OccupancySensing
} // namespace Clusters
} // namespace app
} // namespace chip
