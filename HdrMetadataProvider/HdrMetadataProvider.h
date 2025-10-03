#ifndef HDR_METADATA_PROVIDER_H
#define HDR_METADATA_PROVIDER_H

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include <utility>

namespace HdrMetadata {

struct HdrMetadata {
    uint64_t MasterSequence;
    uint8_t ExposureSequenceIndex;
    uint8_t ExposureCount;
    uint32_t ExposureValue;
    uint8_t HdrProfile;
};

class HdrMetadataProvider {
public:
    // Factory method that returns adjusted exposure values
    static std::unique_ptr<HdrMetadataProvider> Create(
        const std::vector<uint32_t>& profile0Exposures,
        const std::vector<uint32_t>& profile1Exposures,
        std::vector<uint32_t>& adjustedProfile0,
        std::vector<uint32_t>& adjustedProfile1);

    // Process a frame and return its HDR metadata
    HdrMetadata ProcessFrame(uint64_t frameNumber, uint32_t actualExposureTime);

    // Get the current/last profile being processed
    int GetCurrentProfile() const { return _lastProfile; }
    uint8_t GetProfileWindowSize(const int profile) const { return profile == 0 ? _profile0.WindowSize() : _profile1.WindowSize(); }
private:
    struct ProfileInfo {
        std::vector<uint32_t> Exposures;

        uint8_t WindowSize() const { return static_cast<uint8_t>(Exposures.size()); }

        int64_t GetMasterSequence(int64_t n) const {
            if (WindowSize() == 0) return 0;
            return (n / WindowSize()) + ((n % WindowSize()) > 0 ? 1 : 0);
        }
    };

    // Constructor is private - use Create factory method
    HdrMetadataProvider(const std::vector<uint32_t>& profile0Exposures,
                        const std::vector<uint32_t>& profile1Exposures);

    // Deleted copy operations for clear ownership semantics
    HdrMetadataProvider(const HdrMetadataProvider&) = delete;
    HdrMetadataProvider& operator=(const HdrMetadataProvider&) = delete;

    // Default move operations
    HdrMetadataProvider(HdrMetadataProvider&&) = default;
    HdrMetadataProvider& operator=(HdrMetadataProvider&&) = default;

    void BuildExposureMap();
    void HandleDuplicateExposures();
    void CalculateFrameOffset(uint64_t frameNumber, const ProfileInfo& previousProfile,
                             const ProfileInfo& newProfile, uint8_t sequenceIndex);
    std::pair<uint8_t, uint8_t> LookupExposure(uint32_t exposureTime) const;
    ProfileInfo& GetProfile(uint8_t profile);
    const ProfileInfo& GetProfile(uint8_t profile) const;

    ProfileInfo _profile0;
    ProfileInfo _profile1;
    std::unordered_map<uint32_t, std::pair<uint8_t, uint8_t>> _exposureMap;

    uint8_t _lastProfile = 0;
    uint8_t _lastSequenceIndex = 0;
    uint64_t _lastFrameNumber = UINT64_MAX;
    int64_t _frameOffset = 0;
};

} // namespace HdrMetadata

#endif // HDR_METADATA_PROVIDER_H