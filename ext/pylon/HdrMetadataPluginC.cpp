/* SPDX-License-Identifier: BSD-2-Clause
 *
 * C-compatible wrapper for HdrMetadataPlugin
 */

// Include C++ header first to get the class definition
#include "HdrMetadataPlugin.h"

// Then the C header for the wrapper API
extern "C" {
#include "HdrMetadataPluginC.h"
}

#include <vector>
#include <string>
#include <memory>

// Helper function to parse comma-separated exposure values
static std::vector<guint32> parse_exposure_sequence(const gchar* sequence) {
    std::vector<guint32> exposures;
    if (!sequence || !*sequence) {
        return exposures;
    }

    std::string seq(sequence);
    size_t start = 0;
    size_t comma = seq.find(',');

    while (comma != std::string::npos) {
        exposures.push_back(std::stoul(seq.substr(start, comma - start)));
        start = comma + 1;
        comma = seq.find(',', start);
    }

    if (start < seq.length()) {
        exposures.push_back(std::stoul(seq.substr(start)));
    }

    return exposures;
}

// Helper function to convert exposure values back to comma-separated string
static gchar* format_exposure_sequence(const std::vector<guint32>& exposures) {
    if (exposures.empty()) {
        return NULL;
    }

    GString *str = g_string_new(NULL);
    for (size_t i = 0; i < exposures.size(); i++) {
        if (i > 0) {
            g_string_append_c(str, ',');
        }
        g_string_append_printf(str, "%u", exposures[i]);
    }
    return g_string_free(str, FALSE);
}

extern "C" {

HdrMetadataPlugin*
hdr_metadata_plugin_new(void) {
    // Cast C++ object to opaque C pointer
    return reinterpret_cast<HdrMetadataPlugin*>(new class HdrMetadataPlugin());
}

void
hdr_metadata_plugin_free(HdrMetadataPlugin* plugin) {
    delete reinterpret_cast<::HdrMetadataPlugin*>(plugin);  // delete is safe with nullptr
}

gboolean
hdr_metadata_plugin_configure(HdrMetadataPlugin* plugin,
                              const gchar* hdr_sequence0,
                              const gchar* hdr_sequence1,
                              gchar** adjusted_sequence0,
                              gchar** adjusted_sequence1,
                              gchar** error) {
    if (!plugin) {
        if (error) {
            *error = g_strdup("Plugin is NULL");
        }
        return FALSE;
    }

    if (!hdr_sequence0 || !*hdr_sequence0) {
        if (error) {
            *error = g_strdup("Profile 0 HDR sequence is empty");
        }
        return FALSE;
    }

    // Parse comma-separated exposure values
    try {
        std::vector<guint32> profile0_exposures = parse_exposure_sequence(hdr_sequence0);
        std::vector<guint32> profile1_exposures = parse_exposure_sequence(hdr_sequence1);

        // Configure the plugin
        std::vector<guint32> adjusted0, adjusted1;
        auto* cpp_plugin = reinterpret_cast<::HdrMetadataPlugin*>(plugin);
        gboolean result = cpp_plugin->Configure(profile0_exposures, profile1_exposures,
                                           adjusted0, adjusted1);

        if (result) {
            // Convert adjusted sequences back to strings
            if (adjusted_sequence0) {
                *adjusted_sequence0 = format_exposure_sequence(adjusted0);
            }

            if (adjusted_sequence1) {
                *adjusted_sequence1 = format_exposure_sequence(adjusted1);
            }
        } else if (error) {
            *error = g_strdup("Failed to configure HDR plugin");
        }

        return result;

    } catch (const std::exception& e) {
        if (error) {
            *error = g_strdup_printf("Configuration error: %s", e.what());
        }
        return FALSE;
    } catch (...) {
        if (error) {
            *error = g_strdup("Unknown configuration error");
        }
        return FALSE;
    }
}

gboolean
hdr_metadata_plugin_process_and_attach(HdrMetadataPlugin* plugin,
                                       GstBuffer* buffer,
                                       guint64 frame_number,
                                       guint32 exposure_time) {
    if (!plugin || !buffer) {
        return FALSE;
    }

    auto* cpp_plugin = reinterpret_cast<::HdrMetadataPlugin*>(plugin);
    return cpp_plugin->ProcessAndAttachMetadata(buffer, frame_number, exposure_time);
}

gboolean
hdr_metadata_plugin_is_configured(const HdrMetadataPlugin* plugin) {
    if (!plugin) {
        return FALSE;
    }

    auto* cpp_plugin = reinterpret_cast<const ::HdrMetadataPlugin*>(plugin);
    return cpp_plugin->IsConfigured();
}

void
hdr_metadata_plugin_reset(HdrMetadataPlugin* plugin) {
    if (!plugin) {
        return;
    }

    auto* cpp_plugin = reinterpret_cast<::HdrMetadataPlugin*>(plugin);
    cpp_plugin->Reset();
}

gint
hdr_metadata_plugin_get_current_profile(const HdrMetadataPlugin* plugin) {
    if (!plugin) {
        return -1;
    }

    auto* cpp_plugin = reinterpret_cast<const ::HdrMetadataPlugin*>(plugin);
    return cpp_plugin->GetCurrentProfile();
}

} // extern "C"