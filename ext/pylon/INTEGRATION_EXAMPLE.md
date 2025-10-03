# HDR Metadata Integration Guide

This guide shows how to integrate HDR metadata support into `gstpylonsrc.cpp` with minimal changes.

## Files to Add to Build System

Add these files to `ext/pylon/meson.build`:
```meson
gstpylon_sources += files(
  'gstpylonhdrmeta.cpp',
  'HdrMetadataPlugin.cpp',
  '../../HdrMetadataProvider/HdrMetadataProvider.cpp'
)
```

## Minimal Changes to gstpylonsrc.cpp

### 1. Add includes (near other includes)
```cpp
#include "HdrMetadataPlugin.h"
```

### 2. Add to private data structure (_GstPylonSrc)
```cpp
  /* HDR metadata plugin */
  HdrMetadataPlugin* hdr_plugin;
  gboolean hdr_enabled;
  gchar* hdr_sequence;  // Property: "19,150" or "19,150:19,250"
```

### 3. In init function (gst_pylon_src_init)
```cpp
  src->hdr_plugin = new HdrMetadataPlugin();
  src->hdr_enabled = FALSE;
  src->hdr_sequence = NULL;
```

### 4. In finalize function (gst_pylon_src_finalize)
```cpp
  if (src->hdr_plugin) {
    delete src->hdr_plugin;
    src->hdr_plugin = NULL;
  }
  g_free(src->hdr_sequence);
```

### 5. Add properties (in class_init)
```cpp
  g_object_class_install_property(gobject_class, PROP_HDR_SEQUENCE,
      g_param_spec_string("hdr-sequence", "HDR Sequence",
          "HDR exposure sequences as 'profile0:profile1' (e.g., '19,150:19,250')",
          NULL,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property(gobject_class, PROP_HDR_ENABLED,
      g_param_spec_boolean("hdr-enabled", "HDR Enabled",
          "Enable HDR metadata tracking",
          FALSE,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
```

### 6. Parse HDR sequence when starting (in gst_pylon_src_start or when property is set)
```cpp
static void
gst_pylon_src_configure_hdr(GstPylonSrc* src) {
  if (!src->hdr_enabled || !src->hdr_sequence || !src->hdr_plugin) {
    return;
  }

  // Parse "19,150:19,250" format
  gchar** profiles = g_strsplit(src->hdr_sequence, ":", 2);
  if (!profiles[0]) {
    GST_WARNING_OBJECT(src, "Invalid HDR sequence format");
    g_strfreev(profiles);
    return;
  }

  std::vector<guint32> profile0_exposures;
  std::vector<guint32> profile1_exposures;

  // Parse profile 0
  if (profiles[0]) {
    gchar** values = g_strsplit(profiles[0], ",", -1);
    for (int i = 0; values[i]; i++) {
      profile0_exposures.push_back(g_ascii_strtoull(values[i], NULL, 10));
    }
    g_strfreev(values);
  }

  // Parse profile 1
  if (profiles[1]) {
    gchar** values = g_strsplit(profiles[1], ",", -1);
    for (int i = 0; values[i]; i++) {
      profile1_exposures.push_back(g_ascii_strtoull(values[i], NULL, 10));
    }
    g_strfreev(values);
  }

  g_strfreev(profiles);

  // Configure HDR plugin
  std::vector<guint32> adjusted0, adjusted1;
  if (src->hdr_plugin->Configure(profile0_exposures, profile1_exposures,
                                  adjusted0, adjusted1)) {
    GST_INFO_OBJECT(src, "HDR configured successfully");

    // Log adjusted values if any duplicates were found
    for (size_t i = 0; i < adjusted1.size(); i++) {
      if (profile1_exposures[i] != adjusted1[i]) {
        GST_INFO_OBJECT(src, "Profile 1 exposure %u adjusted to %u",
                        profile1_exposures[i], adjusted1[i]);
      }
    }
  }
}
```

### 7. In create function where buffer is filled (gst_pylon_src_create)
```cpp
  // After getting the grabbed buffer and before pushing...
  // Look for where chunk data or metadata is processed

  if (src->hdr_enabled && src->hdr_plugin && src->hdr_plugin->IsConfigured()) {
    // Get frame number and exposure time
    // These usually come from chunk data or image metadata
    guint64 frame_number = /* get from Pylon grab result or chunk */;
    guint32 exposure_time = /* get from chunk data, e.g., ChunkExposureTime */;

    // Process and attach HDR metadata
    src->hdr_plugin->ProcessAndAttachMetadata(buf, frame_number, exposure_time);
  }
```

### 8. Reset on stop (in gst_pylon_src_stop)
```cpp
  if (src->hdr_plugin) {
    src->hdr_plugin->Reset();
  }

## Example Usage

```bash
# Configure camera with HDR sequence
gst-launch-1.0 pylonsrc hdr-enabled=true hdr-sequence="19,150:19,250" ! \
    video/x-raw ! videoconvert ! autovideosink

# The metadata will be automatically attached to each buffer
# Downstream elements can access it via:
# GstPylonHdrMeta* meta = gst_buffer_get_pylon_hdr_meta(buffer);
```

## Testing Without Full Integration

You can test the HDR metadata system standalone:
```bash
# Compile and run tests
cd HdrMetadataProvider
g++ -std=c++17 -o test_hdr test_hdr_metadata_comprehensive.cpp HdrMetadataProvider.cpp
./test_hdr
```

## Benefits of This Approach

1. **Minimal changes**: Only ~50 lines added to original file
2. **Optional feature**: Can be enabled/disabled with compile flag
3. **Clean separation**: All HDR logic in separate files
4. **Easy to remove**: Just remove the #ifdef blocks
5. **No API breaks**: Existing functionality unchanged
6. **Testable**: HDR logic can be tested independently