# Illumination Feature Requirements

## Date
December 17, 2025

## Requirement
Add an "Illumination" control feature to the `pylonsrc` GStreamer plugin for Basler cameras.

## Specifications

### Property
- **Name**: `illumination`
- **Type**: Boolean (GParamSpec boolean)
- **Default**: FALSE
- **Access**: Read/Write, mutable in READY state

### Behavior

When `illumination=true`:
- LineSelector = Line2
- LineMode = Output
- LineSource = ExposureActive

When `illumination=false`:
- LineSelector = Line2
- LineMode = Input
- LineSource = Off

### Implementation Details

1. **Direct Pylon C++ API Usage**: The feature uses the Pylon C++ API directly instead of GObject properties because camera properties are dynamically discovered after initialization and are not available during the `gst_pylon_src_start()` call.

2. **Function Added**: `gst_pylon_configure_line2()` in `gstpylon.cpp`
   - Accesses the `Pylon::CBaslerUniversalInstantCamera` object directly
   - Uses `GenApi::IsWritable()` checks before setting values
   - Proper error handling with GError

3. **Files Modified**:
   - `ext/pylon/gstpylon.h` - Function declaration
   - `ext/pylon/gstpylon.cpp` - Function implementation
   - `ext/pylon/gstpylonsrc.cpp` - Property infrastructure and invocation

### Usage Example

```bash
# Enable illumination (Line2 output synchronized with exposure)
gst-launch-1.0 pylonsrc illumination=true device-serial-number="40637130" ! ...

# Disable illumination (Line2 as input)
gst-launch-1.0 pylonsrc illumination=false device-serial-number="40637130" ! ...
```

### Testing

Tested with Basler a2A1920-51gcPRO camera (serial: 40637130)

**Test Results**:
- ✅ Line2 correctly configured as Output with ExposureActive when illumination=true
- ✅ Line2 correctly configured as Input with LineSource Off when illumination=false
- ✅ No impact on existing functionality
- ✅ Proper error logging if configuration fails

### Technical Notes

- The configuration happens in `gst_pylon_src_start()` after the camera is opened
- Errors during Line2 configuration do not abort the pipeline (logged as warnings)
- Uses Pylon SDK 25.11.0 (ABI version 11.2.2)
