#include "HdrMetadataProvider.h"
#include <iostream>
#include <algorithm>
#include <set>

namespace HdrMetadata {

HdrMetadataProvider::HdrMetadataProvider(const std::vector<uint32_t>& profile0Exposures,
                                         const std::vector<uint32_t>& profile1Exposures) {
    _profile0.Exposures = profile0Exposures;
    _profile1.Exposures = profile1Exposures;
    BuildExposureMap();
}

std::unique_ptr<HdrMetadataProvider> HdrMetadataProvider::Create(
    const std::vector<uint32_t>& profile0Exposures,
    const std::vector<uint32_t>& profile1Exposures,
    std::vector<uint32_t>& adjustedProfile0,
    std::vector<uint32_t>& adjustedProfile1) {

    auto provider = std::unique_ptr<HdrMetadataProvider>(
        new HdrMetadataProvider(profile0Exposures, profile1Exposures));

    // Build adjusted exposure arrays from the exposure map
    adjustedProfile0.resize(provider->_profile0.WindowSize());
    adjustedProfile1.resize(provider->_profile1.WindowSize());

    for (const auto& [exposure, profileIndex] : provider->_exposureMap) {
        if (profileIndex.first == 0) {
            adjustedProfile0[profileIndex.second] = exposure;
        } else {
            adjustedProfile1[profileIndex.second] = exposure;
        }
    }

    return provider;
}

HdrMetadata HdrMetadataProvider::ProcessFrame(uint64_t frameNumber, uint32_t actualExposureTime) {
    if (frameNumber == 0) {
        throw std::invalid_argument("Frame number must be greater than zero");
    }

    // Look up the exposure to find profile and index
    auto [profile, index] = LookupExposure(actualExposureTime);

    // Get exposure count for current profile
    uint8_t exposureCount = GetProfile(profile).WindowSize();

    // Detect profile switch
    if (_lastFrameNumber != UINT64_MAX && profile != _lastProfile) {
        const ProfileInfo& prvProfile = GetProfile(_lastProfile);
        const ProfileInfo& newProfile = GetProfile(profile);

        CalculateFrameOffset(frameNumber, prvProfile, newProfile, index);

        std::cerr << "Profile switch at frame " << frameNumber << ": "
                  << (int)_lastProfile << " -> " << (int)profile
                  << ", index=" << (int)index
                  << ", offset k=" << _frameOffset << std::endl;
    }

    // Calculate master sequence with offset
    int64_t adjustedFrame = static_cast<int64_t>(frameNumber) + _frameOffset;
    auto masterSequence = GetProfile(profile).GetMasterSequence(adjustedFrame);

    // Create metadata
    HdrMetadata metadata;
    metadata.MasterSequence = static_cast<uint64_t>(masterSequence);
    metadata.ExposureSequenceIndex = index;
    metadata.ExposureCount = exposureCount;
    metadata.ExposureValue = actualExposureTime;
    metadata.HdrProfile = profile;

    // Update state for next frame
    _lastProfile = profile;
    _lastSequenceIndex = index;
    _lastFrameNumber = frameNumber;

    return metadata;
}

void HdrMetadataProvider::CalculateFrameOffset(uint64_t n, const ProfileInfo& prv,
                                               const ProfileInfo& nx, uint8_t sequenceIndex) {
    auto prv_m = prv.GetMasterSequence(static_cast<int64_t>(n) + _frameOffset);

    bool mid_prv = prv.WindowSize() > 1 && _lastSequenceIndex < prv.WindowSize() - 1;

    int64_t adjustedFrameNumber = prv_m * nx.WindowSize();
    _frameOffset = adjustedFrameNumber - static_cast<int64_t>(n);

    for (int i = 1; i <= nx.WindowSize(); i++) {
        if (nx.GetMasterSequence(static_cast<int64_t>(n) + _frameOffset - 1) == prv_m) {
            _frameOffset -= 1;
        } else {
            break;
        }
    }

    _frameOffset += sequenceIndex;

    bool start_nx = sequenceIndex == 0;
    if (!mid_prv || !start_nx) return;
    if (nx.GetMasterSequence(static_cast<int64_t>(n) + _frameOffset) == prv_m) {
        _frameOffset += nx.WindowSize();
    }
}

void HdrMetadataProvider::BuildExposureMap() {
    // Check if at least one profile is configured
    if (_profile0.WindowSize() == 0 && _profile1.WindowSize() == 0) {
        return;
    }

    // Helper to add profile exposures to map
    auto addProfileToMap = [this](const ProfileInfo& profile, uint8_t profileNum) {
        for (uint8_t i = 0; i < profile.WindowSize(); i++) {
            _exposureMap[profile.Exposures[i]] = {profileNum, i};
        }
    };

    // Add both profiles
    addProfileToMap(_profile0, 0);
    addProfileToMap(_profile1, 1);

    // Handle duplicates
    HandleDuplicateExposures();
}

void HdrMetadataProvider::HandleDuplicateExposures() {
    std::set<uint32_t> seen;
    std::vector<uint32_t> duplicates;

    // Find duplicates across both profiles
    for (const auto& exp : _profile0.Exposures) {
        if (!seen.insert(exp).second) {
            duplicates.push_back(exp);
        }
    }

    for (const auto& exp : _profile1.Exposures) {
        if (!seen.insert(exp).second) {
            duplicates.push_back(exp);
        }
    }

    // Adjust duplicates
    for (uint32_t dupExposure : duplicates) {
        // Find which profiles have this exposure
        auto findExposureIndex = [](const ProfileInfo& profile, uint32_t exposure) -> uint8_t {
            for (uint8_t i = 0; i < profile.WindowSize(); i++) {
                if (profile.Exposures[i] == exposure) {
                    return i;
                }
            }
            return UINT8_MAX;
        };

        uint8_t profile0Index = findExposureIndex(_profile0, dupExposure);
        uint8_t profile1Index = findExposureIndex(_profile1, dupExposure);

        // If found in both, adjust profile 1's value
        if (profile0Index != UINT8_MAX && profile1Index != UINT8_MAX) {
            uint32_t adjustedExposure = dupExposure;

            // Find a unique value by incrementing
            while (_exposureMap.find(adjustedExposure) != _exposureMap.end()) {
                adjustedExposure++;
            }

            // Update mapping - Profile 0 keeps original, Profile 1 gets adjusted
            _exposureMap.erase(dupExposure);
            _exposureMap[dupExposure] = {0, profile0Index};
            _exposureMap[adjustedExposure] = {1, profile1Index};

            std::cerr << "Warning: Duplicate exposure " << dupExposure << "μs. "
                      << "Profile 1 index " << (int)profile1Index
                      << " adjusted to " << adjustedExposure << "μs" << std::endl;
        }
    }
}

std::pair<uint8_t, uint8_t> HdrMetadataProvider::LookupExposure(uint32_t exposureTime) const {
    auto it = _exposureMap.find(exposureTime);
    if (it != _exposureMap.end()) {
        return it->second;
    }

    // This should not happen - exposure values should exactly match configured values
    throw std::invalid_argument("Unexpected exposure time " + std::to_string(exposureTime) +
                               "μs not found in configured sequences");
}

HdrMetadataProvider::ProfileInfo& HdrMetadataProvider::GetProfile(uint8_t profile) {
    return profile == 0 ? _profile0 : _profile1;
}

const HdrMetadataProvider::ProfileInfo& HdrMetadataProvider::GetProfile(uint8_t profile) const {
    return profile == 0 ? _profile0 : _profile1;
}

} // namespace HdrMetadata