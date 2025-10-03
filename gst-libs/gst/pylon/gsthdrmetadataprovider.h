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

#ifndef __GST_HDR_METADATA_PROVIDER_H__
#define __GST_HDR_METADATA_PROVIDER_H__

#include <glib.h>
#include <unordered_map>
#include <vector>
#include <utility>

G_BEGIN_DECLS

class HdrMetadataProvider {
public:
    struct HdrMetadata {
        guint64 master_sequence;
        guint8 exposure_sequence_index;
        guint8 exposure_count;
        guint32 exposure_value;
        guint8 hdr_profile;
    };

    HdrMetadataProvider();
    ~HdrMetadataProvider();

    // Configuration methods
    void SetProfile0Sequence(const std::vector<guint32>& exposures);
    void SetProfile1Sequence(const std::vector<guint32>& exposures);
    void Reset();

    // Main processing method
    HdrMetadata ProcessFrame(guint32 actual_exposure_time, guint64 frame_number);

    // Query methods
    gboolean IsConfigured() const;
    guint8 GetActiveProfile() const;
    guint64 GetMasterSequence() const;

private:
    struct ProfileInfo {
        std::vector<guint32> exposures;
        guint8 exposure_count;
    };

    ProfileInfo profile0_;
    ProfileInfo profile1_;

    // Exposure to profile/index mapping
    // Maps exposure_value -> (profile_id, sequence_index)
    std::unordered_map<guint32, std::pair<guint8, guint8>> exposure_map_;

    // Tracking state
    guint64 master_sequence_;          // Current master sequence value
    guint8 last_profile_;
    guint8 last_sequence_index_;
    guint64 last_frame_number_;
    guint32 frames_in_current_window_; // Count of frames seen in current HDR window
    gboolean is_configured_;

    // Private methods
    void BuildExposureMap();
    void HandleDuplicateExposures();
    std::pair<guint8, guint8> LookupExposure(guint32 exposure_time) const;
};

G_END_DECLS

#endif /* __GST_HDR_METADATA_PROVIDER_H__ */