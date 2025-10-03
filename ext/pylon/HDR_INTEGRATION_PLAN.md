# HDR Metadata Integration Plan for gstpylonsrc.cpp

## Overview
Replace the old HDR implementation with our new HDR metadata tracking system that:
- Automatically detects profile from exposure values
- Maintains master sequence continuity
- Attaches proper GStreamer metadata to buffers
- Updates sequences when adjustments are needed

## Changes Required

### 1. Remove Old Implementation

**Remove from _GstPylonSrc struct:**
```cpp
// REMOVE these:
gboolean profile_switch_pending;
gint target_profile;
guint profile0_window_size;
guint profile1_window_size;
gint switch_retry_count;

// KEEP these (but update usage):
gchar *hdr_sequence;      // Profile 0 exposures
gchar *hdr_sequence2;     // Profile 1 exposures
gint hdr_profile;         // Make READ-ONLY (from provider)
```

**Remove functions:**
- Profile switching logic in `gst_pylon_src_create()` (lines 1162-1220)
- Complex sequencer configuration in `gst_pylon_configure_dual_hdr_sequence()`
- `gst_pylon_switch_hdr_profile()` calls

### 2. Add New Implementation

**Add to includes:**
```cpp
#include "HdrMetadataPluginC.h"
#include "gsthdrmeta.h"
```

**Add to _GstPylonSrc struct:**
```cpp
HdrMetadataPlugin *hdr_plugin;  // HDR metadata provider
```

**In gst_pylon_src_init():**
```cpp
self->hdr_plugin = hdr_metadata_plugin_new();
```

**In gst_pylon_src_finalize():**
```cpp
if (self->hdr_plugin) {
    hdr_metadata_plugin_free(self->hdr_plugin);
    self->hdr_plugin = NULL;
}
```

### 3. Update Property Handling

**Make hdr_profile READ-ONLY in class_init:**
```cpp
g_object_class_install_property(
    gobject_class, PROP_HDR_PROFILE,
    g_param_spec_int(
        "hdr-profile", "Active HDR Profile",
        "Currently active HDR profile (0 or 1) - automatically detected from exposure values",
        -1, 1, -1,  // -1 means not configured/unknown
        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
```

**In get_property:**
```cpp
case PROP_HDR_PROFILE:
    g_value_set_int(value,
        hdr_metadata_plugin_get_current_profile(self->hdr_plugin));
    break;
```

**Remove from set_property:**
```cpp
// REMOVE case PROP_HDR_PROFILE - it's now read-only
```

### 4. Configure HDR with Adjusted Sequences

**In gst_pylon_src_start() - replace old HDR config:**
```cpp
/* Configure HDR sequence if specified */
if (self->hdr_sequence && strlen(self->hdr_sequence) > 0) {
    gchar *adjusted_seq0 = NULL;
    gchar *adjusted_seq1 = NULL;
    gchar *error = NULL;

    // Configure HDR plugin - it may adjust sequences for duplicates
    if (hdr_metadata_plugin_configure(self->hdr_plugin,
                                      self->hdr_sequence,
                                      self->hdr_sequence2,  // Can be NULL
                                      &adjusted_seq0,
                                      &adjusted_seq1,
                                      &error)) {

        // Update sequences if they were adjusted
        if (adjusted_seq0 && g_strcmp0(adjusted_seq0, self->hdr_sequence) != 0) {
            GST_INFO_OBJECT(self, "Profile 0 sequence adjusted: %s -> %s",
                           self->hdr_sequence, adjusted_seq0);
            g_free(self->hdr_sequence);
            self->hdr_sequence = adjusted_seq0;
            adjusted_seq0 = NULL;  // Transfer ownership
        }

        if (adjusted_seq1 && g_strcmp0(adjusted_seq1, self->hdr_sequence2) != 0) {
            GST_INFO_OBJECT(self, "Profile 1 sequence adjusted: %s -> %s",
                           self->hdr_sequence2, adjusted_seq1);
            g_free(self->hdr_sequence2);
            self->hdr_sequence2 = adjusted_seq1;
            adjusted_seq1 = NULL;  // Transfer ownership
        }

        // Enable chunks for HDR metadata
        gst_pylon_src_enable_hdr_chunks(self);

        // Configure camera with (possibly adjusted) sequences
        if (self->hdr_sequence2) {
            ret = gst_pylon_configure_dual_hdr_sequence(self->pylon,
                                                        self->hdr_sequence,
                                                        self->hdr_sequence2,
                                                        &error);
        } else {
            ret = gst_pylon_configure_hdr_sequence(self->pylon,
                                                   self->hdr_sequence,
                                                   &error);
        }

        if (ret) {
            GST_INFO_OBJECT(self, "HDR sequences configured successfully");
        } else {
            GST_ERROR_OBJECT(self, "Failed to configure camera: %s",
                            error ? error : "Unknown error");
        }
    } else {
        GST_ERROR_OBJECT(self, "Failed to configure HDR plugin: %s",
                        error ? error : "Unknown error");
        ret = FALSE;
    }

    g_free(adjusted_seq0);
    g_free(adjusted_seq1);
    g_free(error);
}
```

### 5. Process and Attach HDR Metadata

**In gst_pylon_src_create() - after getting buffer, extract exposure from chunks:**
```cpp
// Get exposure time from chunk data (this code likely exists)
guint32 exposure_time = 0;
guint64 frame_number = /* get from grab result */;

// Extract exposure from chunks (example - adapt to actual chunk access)
if (/* chunk data available */) {
    // Get ChunkExposureTime value in microseconds
    exposure_time = /* extract from chunk */;
}

// Process and attach HDR metadata
if (self->hdr_plugin &&
    hdr_metadata_plugin_is_configured(self->hdr_plugin) &&
    exposure_time > 0) {

    if (!hdr_metadata_plugin_process_and_attach(self->hdr_plugin,
                                                buf,
                                                frame_number,
                                                exposure_time)) {
        GST_WARNING_OBJECT(self, "Failed to attach HDR metadata");
    }
}
```

### 6. Reset on Stop

**In gst_pylon_src_stop():**
```cpp
if (self->hdr_plugin) {
    hdr_metadata_plugin_reset(self->hdr_plugin);
}
```

## Benefits of New Implementation

1. **Automatic profile detection** - No manual switching needed
2. **Sequence adjustment** - Handles duplicate exposures transparently
3. **Metadata on buffers** - Downstream elements can access HDR info
4. **Master sequence continuity** - Maintains sequence across profile changes
5. **Cleaner code** - Removes complex switching logic

## Testing

```bash
# Test with adjusted sequences
gst-launch-1.0 pylonsrc hdr-sequence="19,150" hdr-sequence2="19,250" ! \
    fakesink silent=false -v

# Check metadata
# The plugin will log if sequences are adjusted
# Each buffer will have GstHdrMeta attached
```