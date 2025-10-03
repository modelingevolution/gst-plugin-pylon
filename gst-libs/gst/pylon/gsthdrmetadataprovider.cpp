/* Copyright (C) 2024 Basler AG
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *     1. Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *     2. Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written
 *        permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gsthdrmetadataprovider.h"
#include <gst/gst.h>
#include <algorithm>
#include <set>

HdrMetadataProvider::HdrMetadataProvider()
    : master_sequence_(0),
      last_profile_(0),
      last_sequence_index_(0),
      last_frame_number_(G_MAXUINT64),
      frames_in_current_window_(0),
      is_configured_(FALSE) {
    profile0_.exposure_count = 0;
    profile1_.exposure_count = 0;
}

HdrMetadataProvider::~HdrMetadataProvider() {
}

void HdrMetadataProvider::SetProfile0Sequence(const std::vector<guint32>& exposures) {
    profile0_.exposures = exposures;
    profile0_.exposure_count = exposures.size();
    BuildExposureMap();
}

void HdrMetadataProvider::SetProfile1Sequence(const std::vector<guint32>& exposures) {
    profile1_.exposures = exposures;
    profile1_.exposure_count = exposures.size();
    BuildExposureMap();
}

void HdrMetadataProvider::Reset() {
    master_sequence_ = 0;
    last_profile_ = 0;
    last_sequence_index_ = 0;
    last_frame_number_ = G_MAXUINT64;
    frames_in_current_window_ = 0;
    is_configured_ = FALSE;
    exposure_map_.clear();
    profile0_.exposures.clear();
    profile0_.exposure_count = 0;
    profile1_.exposures.clear();
    profile1_.exposure_count = 0;
}

void HdrMetadataProvider::BuildExposureMap() {
    exposure_map_.clear();

    // Check if at least one profile is configured
    if (profile0_.exposure_count == 0 && profile1_.exposure_count == 0) {
        is_configured_ = FALSE;
        return;
    }

    // Add profile 0 exposures
    for (guint8 i = 0; i < profile0_.exposure_count; i++) {
        exposure_map_[profile0_.exposures[i]] = std::make_pair(0, i);
    }

    // Add profile 1 exposures
    for (guint8 i = 0; i < profile1_.exposure_count; i++) {
        exposure_map_[profile1_.exposures[i]] = std::make_pair(1, i);
    }

    // Handle duplicates
    HandleDuplicateExposures();

    is_configured_ = TRUE;
}

void HdrMetadataProvider::HandleDuplicateExposures() {
    std::set<guint32> seen_exposures;
    std::vector<guint32> duplicates;

    // Find all unique exposures from both profiles
    for (const auto& exp : profile0_.exposures) {
        if (!seen_exposures.insert(exp).second) {
            duplicates.push_back(exp);
        }
    }

    for (const auto& exp : profile1_.exposures) {
        if (!seen_exposures.insert(exp).second) {
            duplicates.push_back(exp);
        }
    }

    // For each duplicate, adjust the second occurrence
    for (guint32 dup_exposure : duplicates) {
        // Find which profile has the duplicate second
        gboolean found_in_profile0 = FALSE;
        gboolean found_in_profile1 = FALSE;
        guint8 profile0_index = 0;
        guint8 profile1_index = 0;

        for (guint8 i = 0; i < profile0_.exposure_count; i++) {
            if (profile0_.exposures[i] == dup_exposure) {
                found_in_profile0 = TRUE;
                profile0_index = i;
                break;
            }
        }

        for (guint8 i = 0; i < profile1_.exposure_count; i++) {
            if (profile1_.exposures[i] == dup_exposure) {
                found_in_profile1 = TRUE;
                profile1_index = i;
                break;
            }
        }

        // If found in both profiles, adjust profile1's value
        if (found_in_profile0 && found_in_profile1) {
            guint32 adjusted_exposure = dup_exposure;

            // Find a unique value by incrementing
            while (exposure_map_.find(adjusted_exposure) != exposure_map_.end()) {
                adjusted_exposure++;
            }

            // Update the exposure map with adjusted value
            exposure_map_.erase(dup_exposure);
            exposure_map_[dup_exposure] = std::make_pair(0, profile0_index);
            exposure_map_[adjusted_exposure] = std::make_pair(1, profile1_index);

            GST_WARNING("Duplicate exposure %u found in both profiles. "
                       "Profile 1 exposure mapped to %u for uniqueness",
                       dup_exposure, adjusted_exposure);
        }
    }
}

std::pair<guint8, guint8> HdrMetadataProvider::LookupExposure(guint32 exposure_time) const {
    auto it = exposure_map_.find(exposure_time);
    if (it != exposure_map_.end()) {
        return it->second;
    }

    // This should not happen - exposure values should exactly match configured values
    GST_ERROR("Unexpected exposure time %u not found in configured sequences", exposure_time);
    // Return profile 0, index 0 as fallback to avoid crash, but this indicates a configuration error
    return std::make_pair(0, 0);
}


HdrMetadataProvider::HdrMetadata HdrMetadataProvider::ProcessFrame(guint32 actual_exposure_time,
                                                                   guint64 frame_number) {
    HdrMetadata metadata;

    // Look up the exposure to find profile and index
    auto [profile, index] = LookupExposure(actual_exposure_time);

    // Get exposure count for current profile
    guint8 exposure_count = (profile == 0) ? profile0_.exposure_count : profile1_.exposure_count;

    // Detect profile switch
    if (last_frame_number_ != G_MAXUINT64 && profile != last_profile_) {
        // Profile switch detected - increment master sequence
        master_sequence_++;
        frames_in_current_window_ = 0;  // Reset window frame count

        GST_INFO("Profile switch detected: %u -> %u, master_sequence now %lu",
                 last_profile_, profile, master_sequence_);
    }
    // Check if we're starting a new window within the same profile
    else if (last_frame_number_ != G_MAXUINT64 &&
             profile == last_profile_) {
        // For single exposure sequences, every frame is a new window
        if (exposure_count == 1) {
            master_sequence_++;
            frames_in_current_window_ = 0;
            GST_DEBUG("New single-exposure window in profile %u, master_sequence now %lu",
                      profile, master_sequence_);
        }
        // For multi-exposure sequences, check if we've wrapped to index 0
        else if (index == 0 && last_sequence_index_ != 0) {
            master_sequence_++;
            frames_in_current_window_ = 0;
            GST_DEBUG("New window in profile %u, master_sequence now %lu",
                      profile, master_sequence_);
        }
    }
    // Check for gaps or out-of-order frames within the same window
    else if (last_frame_number_ != G_MAXUINT64 &&
             profile == last_profile_ &&
             index != (last_sequence_index_ + 1) % exposure_count) {
        // Detected gap or out-of-order frame
        GST_WARNING("Frame gap or out-of-order detected. Expected index %u, got %u",
                    (last_sequence_index_ + 1) % exposure_count, index);
    }

    // Increment frame count for this window
    frames_in_current_window_++;

    // Fill metadata structure
    metadata.master_sequence = master_sequence_;
    metadata.exposure_sequence_index = index;
    metadata.exposure_count = exposure_count;
    metadata.exposure_value = actual_exposure_time;
    metadata.hdr_profile = profile;

    // Update state for next frame
    last_profile_ = profile;
    last_sequence_index_ = index;
    last_frame_number_ = frame_number;

    return metadata;
}

gboolean HdrMetadataProvider::IsConfigured() const {
    return is_configured_;
}

guint8 HdrMetadataProvider::GetActiveProfile() const {
    return last_profile_;
}

guint64 HdrMetadataProvider::GetMasterSequence() const {
    return master_sequence_;
}