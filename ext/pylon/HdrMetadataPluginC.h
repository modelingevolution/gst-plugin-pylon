/* SPDX-License-Identifier: BSD-2-Clause
 *
 * C-compatible wrapper for HdrMetadataPlugin
 * This allows safe usage from C code in gstpylonsrc.c
 */

#ifndef HDR_METADATA_PLUGIN_C_H
#define HDR_METADATA_PLUGIN_C_H

#include <gst/gst.h>

G_BEGIN_DECLS

typedef struct _HdrMetadataPlugin HdrMetadataPlugin;

/**
 * hdr_metadata_plugin_new:
 *
 * Creates a new HDR metadata plugin instance.
 *
 * Returns: (transfer full): a new #HdrMetadataPlugin
 */
HdrMetadataPlugin* hdr_metadata_plugin_new(void);

/**
 * hdr_metadata_plugin_free:
 * @plugin: a #HdrMetadataPlugin
 *
 * Frees the HDR metadata plugin instance.
 */
void hdr_metadata_plugin_free(HdrMetadataPlugin* plugin);

/**
 * hdr_metadata_plugin_configure:
 * @plugin: a #HdrMetadataPlugin
 * @hdr_sequence0: Profile 0 HDR sequence string like "19,150"
 * @hdr_sequence1: (nullable): Profile 1 HDR sequence string like "19,250"
 * @adjusted_sequence0: (out) (optional): location to store adjusted profile 0 sequence
 * @adjusted_sequence1: (out) (optional): location to store adjusted profile 1 sequence
 * @error: (out) (optional): location to store error message
 *
 * Configures the HDR plugin with exposure sequences. The sequences may be
 * adjusted to handle duplicate exposures between profiles.
 *
 * Returns: %TRUE on success
 */
gboolean hdr_metadata_plugin_configure(HdrMetadataPlugin* plugin,
                                       const gchar* hdr_sequence0,
                                       const gchar* hdr_sequence1,
                                       gchar** adjusted_sequence0,
                                       gchar** adjusted_sequence1,
                                       gchar** error);

/**
 * hdr_metadata_plugin_process_and_attach:
 * @plugin: a #HdrMetadataPlugin
 * @buffer: a #GstBuffer to attach metadata to
 * @frame_number: frame number from camera
 * @exposure_time: exposure time in microseconds
 *
 * Processes frame and attaches HDR metadata.
 *
 * Returns: %TRUE if metadata was attached
 */
gboolean hdr_metadata_plugin_process_and_attach(HdrMetadataPlugin* plugin,
                                                GstBuffer* buffer,
                                                guint64 frame_number,
                                                guint32 exposure_time);

/**
 * hdr_metadata_plugin_is_configured:
 * @plugin: a #HdrMetadataPlugin
 *
 * Checks if the plugin is configured.
 *
 * Returns: %TRUE if configured
 */
gboolean hdr_metadata_plugin_is_configured(const HdrMetadataPlugin* plugin);

/**
 * hdr_metadata_plugin_reset:
 * @plugin: a #HdrMetadataPlugin
 *
 * Resets the plugin state.
 */
void hdr_metadata_plugin_reset(HdrMetadataPlugin* plugin);

/**
 * hdr_metadata_plugin_get_current_profile:
 * @plugin: a #HdrMetadataPlugin
 *
 * Gets the current HDR profile being processed.
 *
 * Returns: current profile (0 or 1), or -1 if not configured
 */
gint hdr_metadata_plugin_get_current_profile(const HdrMetadataPlugin* plugin);

G_END_DECLS

#endif /* HDR_METADATA_PLUGIN_C_H */