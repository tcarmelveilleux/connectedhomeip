#include <stdint.h>
#include <stdlib.h>

namespace chip {
namespace Credentials {

constexpr size_t CHIP_CRYPTO_SYMMETRIC_KEY_LENGTH_BYTES = 16;
constexpr size_t kInvalidEpochStartTime = UINT64_MAX;

enum class GroupKeySecurityPolicy: uint8_t
{
  kStandard = 0,
  kLowLatency = 1,
};

struct EpochKey
{
    uint8_t key[CHIP_CRYPTO_SYMMETRIC_KEY_LENGTH_BYTES];
    uint64_t start_time_us;
};

struct GroupKeySet
{
    uint16_t group_key_index;
    EpochKey epoch_keys[3];
    GroupKeySecurityPolicy security_policy;
};

struct GroupStateMapping
{
    chip::FabricIndex fabric_index;
    uint16_t group_key_index;
    uint16_t group_id;
};

struct GroupKeyCandidate
{
    GroupStateMapping group_state_mapping;
    EpochKey epoch_key;
    GroupKeySecurityPolicy security_policy;
};

class GroupKeyManagementDataProvider
{
public:
    class GroupKeySetIterator
    {
        GroupKeySetIterator() = default
        virtual ~GroupKeySetIterator() = default;

        virtual bool HasNext() = 0;
        virtual const GroupKeySet * Next() = 0;
        // Do we need size_t TotalEntries() ?
    };

    class GroupStateMappingIterator
    {
        GroupStateMappingIterator() = default
        virtual ~GroupStateMappingIterator() = default;

        virtual bool HasNext() = 0;
        virtual const GroupStateMapping * Next() = 0;
        // Do we need size_t TotalEntries() ?
    };

    class GroupKeyCandidateIterator
    {
        GroupKeyCandidateIterator() = default
        virtual ~GroupKeyCandidateIterator() = default;

        virtual bool HasNext() = 0;
        virtual const GroupKeyCandidate * Next() = 0;
        // Do we need size_t TotalEntries() ?
    };

    GroupKeyManagementDataProvider()          = default;
    virtual ~GroupKeyManagementDataProvider() = default;

    // Not copyable
    GroupKeyManagementDataProvider(const GroupKeyManagementDataProvider &) = delete;
    GroupKeyManagementDataProvider & operator=(const GroupKeyManagementDataProvider &) = delete;

    virtual CHIP_ERROR Initialize() = 0;
    virtual void Finalize() = 0;

    // ==========  Group Key Set API
    virtual CHIP_ERROR AddKeySet(chip::FabricIndex fabric_index, const GroupKeySet & key_set) = 0;
    virtual CHIP_ERROR GetKeySet(chip::FabricIndex fabric_index, uint16_t key_set_index, GroupKeySet & out_key_set) = 0;
    virtual CHIP_ERROR RemoveKeySet(chip::FabricIndex fabric_index, uint16_t key_set_index) = 0;
    virtual CHIP_ERROR RemoveAllKeySets(chip::FabricIndex fabric_index) = 0;
    virtual CHIP_ERROR RemoveAllKeySetsExceptIpk(chip::FabricIndex fabric_index) = 0;
    virtual CHIP_ERROR GetGroupKeySetListSize(chip::FabricIndex fabric_index, size_t & out_list_size) = 0;
    virtual bool KeySetExists(chip::FabricIndex fabric_index, uint16_t key_set_index) = 0;

    // Iterate through all group keys sets for a given fabric.
    virtual GroupKeySetIterator * IterateGroupKeySets(chip::FabricIndex fabric_index, uint16_t start_index) = 0;

    // ========== Group State mapping API
    virtual CHIP_ERROR AddGroupState(uint16_t list_index, const GroupStateMapping & mapping) = 0;
    virtual CHIP_ERROR GetGroupState(uint16_t list_index, GroupStateMapping & out_mapping) = 0;
    virtual CHIP_ERROR RemoveGroupState(uint16_t list_index) = 0;
    virtual CHIP_ERROR GetGroupStateListSize(size_t & out_list_size) = 0;
    virtual bool GroupStateExists(uint16_t list_index) = 0;
    virtual void GetEpochKeyForGroupTransmit(chip::FabricIndex fabric_index, uint16_t group_id, EpochKey & out_epoch_key);

    // Iterate through all group states
    virtual GroupStateMappingIterator * IterateGroupStateMappings(uint16_t start_index) = 0;

    // ========== Combined Mapping + Keyset iteration (for group message decrypt)

    // From an incoming group message, iterate through possible keys, filtered by
    // group_id (if known) and group_session_id (key hash filter).
    virtual GroupKeyCandidateIterator * IterateGroupKeyCandidates(bool group_id_present, uint16_t group_id, uint16_t group_session_id) = 0;
};

} // namespace Credentials
} // namespace chip