/* SPDX-License-Identifier: BSD-2-Clause
 *
 * HDR Metadata Plugin - Bridge between HdrMetadataProvider and GStreamer
 */

#include "HdrMetadataPlugin.h"
#include "../../HdrMetadataProvider/HdrMetadataProvider.h"
#include <gst/gst.h>

HdrMetadataPlugin::HdrMetadataPlugin()
    : provider_(nullptr), is_configured_(FALSE) {
}

HdrMetadataPlugin::~HdrMetadataPlugin() = default;

gboolean
HdrMetadataPlugin::Configure(const std::vector<guint32>& profile0_exposures,
                             const std::vector<guint32>& profile1_exposures,
                             std::vector<guint32>& adjusted_profile0,
                             std::vector<guint32>& adjusted_profile1) {
    try {
        // Convert guint32 vectors to uint32_t for the provider
        std::vector<uint32_t> p0(profile0_exposures.begin(), profile0_exposures.end());
        std::vector<uint32_t> p1(profile1_exposures.begin(), profile1_exposures.end());
        std::vector<uint32_t> adj0, adj1;

        // Create the provider
        provider_ = HdrMetadata::HdrMetadataProvider::Create(p0, p1, adj0, adj1);

        // Convert back to guint32
        adjusted_profile0.assign(adj0.begin(), adj0.end());
        adjusted_profile1.assign(adj1.begin(), adj1.end());

        is_configured_ = TRUE;

        GST_INFO("HDR metadata plugin configured with profiles: "
                 "P0=%zu exposures, P1=%zu exposures",
                 profile0_exposures.size(), profile1_exposures.size());

        return TRUE;
    } catch (const std::exception& e) {
        GST_ERROR("Failed to configure HDR metadata: %s", e.what());
        is_configured_ = FALSE;
        return FALSE;
    }
}

gboolean
HdrMetadataPlugin::ProcessAndAttachMetadata(GstBuffer* buffer,
                                             guint64 frame_number,
                                             guint32 exposure_time) {
    if (!is_configured_ || !provider_) {
        GST_WARNING("HDR metadata plugin not configured");
        return FALSE;
    }

    if (!buffer) {
        GST_ERROR("Invalid buffer");
        return FALSE;
    }

    try {
        // Process the frame through the provider
        auto hdr_meta = provider_->ProcessFrame(frame_number, exposure_time);

        // Attach metadata to the buffer
        GstHdrMeta* meta = gst_buffer_add_hdr_meta(
            buffer,
            hdr_meta.MasterSequence,
            hdr_meta.ExposureSequenceIndex,
            hdr_meta.ExposureCount,
            hdr_meta.ExposureValue,
            hdr_meta.HdrProfile
        );

        if (!meta) {
            GST_ERROR("Failed to attach HDR metadata to buffer");
            return FALSE;
        }

        GST_LOG("Attached HDR metadata: frame=%lu, master=%lu, profile=%d, "
                "exp_idx=%d/%d, exp_value=%u",
                frame_number, hdr_meta.MasterSequence, hdr_meta.HdrProfile,
                hdr_meta.ExposureSequenceIndex, hdr_meta.ExposureCount,
                hdr_meta.ExposureValue);

        return TRUE;
    } catch (const std::exception& e) {
        GST_ERROR("Failed to process HDR metadata: %s", e.what());
        return FALSE;
    }
}

gboolean
HdrMetadataPlugin::IsConfigured() const {
    return is_configured_;
}

void
HdrMetadataPlugin::Reset() {
    provider_.reset();
    is_configured_ = FALSE;
    GST_INFO("HDR metadata plugin reset");
}

gint
HdrMetadataPlugin::GetCurrentProfile() const {
    if (!is_configured_ || !provider_) {
        return -1;
    }
    return provider_->GetCurrentProfile();
}

gint
HdrMetadataPlugin::GetProfileWindowSize(gint profile) const {
    if (!is_configured_ || !provider_) {
        return 0;
    }
    return provider_->GetProfileWindowSize(profile);
}