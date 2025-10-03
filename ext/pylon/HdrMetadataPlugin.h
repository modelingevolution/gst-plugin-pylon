/* SPDX-License-Identifier: BSD-2-Clause
 *
 * HDR Metadata Plugin - Bridge between HdrMetadataProvider and GStreamer
 */

#ifndef HDR_METADATA_PLUGIN_H
#define HDR_METADATA_PLUGIN_H

#include <memory>
#include <vector>
#include <gst/gst.h>
#include "gsthdrmeta.h"

// Forward declaration to avoid including the full provider header
namespace HdrMetadata {
    class HdrMetadataProvider;
}

/**
 * HdrMetadataPlugin:
 *
 * HDR metadata plugin for the GStreamer Pylon source.
 * This class bridges the C++ HdrMetadataProvider with GStreamer's C API.
 *
 * Usage in gstpylonsrc.cpp:
 * 1. Create instance in class private data
 * 2. Configure when HDR sequence is detected
 * 3. Call ProcessAndAttachMetadata for each frame
 */
class HdrMetadataPlugin {
public:
    HdrMetadataPlugin();
    ~HdrMetadataPlugin();

    /**
     * Configure HDR profiles
     * @param profile0_exposures Exposure values for profile 0 in microseconds
     * @param profile1_exposures Exposure values for profile 1 in microseconds
     * @param adjusted_profile0 Output: Adjusted exposure values for profile 0
     * @param adjusted_profile1 Output: Adjusted exposure values for profile 1 (with duplicates resolved)
     * @return TRUE on success
     */
    gboolean Configure(const std::vector<guint32>& profile0_exposures,
                       const std::vector<guint32>& profile1_exposures,
                       std::vector<guint32>& adjusted_profile0,
                       std::vector<guint32>& adjusted_profile1);

    /**
     * Process frame and attach HDR metadata to buffer
     * @param buffer GStreamer buffer to attach metadata to
     * @param frame_number Frame number from camera
     * @param exposure_time Actual exposure time in microseconds (from chunk data)
     * @return TRUE if metadata was attached, FALSE on error
     */
    gboolean ProcessAndAttachMetadata(GstBuffer* buffer,
                                      guint64 frame_number,
                                      guint32 exposure_time);

    /**
     * Check if HDR is configured
     * @return TRUE if configured
     */
    gboolean IsConfigured() const;

    /**
     * Reset the manager
     */
    void Reset();

    /**
     * Get current HDR profile
     * @return Current profile (0 or 1), or -1 if not configured
     */
    gint GetCurrentProfile() const;

    /**
     * Get window size for a profile
     * @param profile Profile number (0 or 1)
     * @return Window size (number of exposures)
     */
    gint GetProfileWindowSize(gint profile) const;

private:
    std::unique_ptr<HdrMetadata::HdrMetadataProvider> provider_;
    gboolean is_configured_;
};

#endif // HDR_METADATA_PLUGIN_H