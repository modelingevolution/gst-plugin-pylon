/* Copyright (C) 2022 Basler AG
 *
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
#  include "config.h"
#endif

#ifdef NVMM_ENABLED
#  include "cuda_runtime.h"
#  include "gstpylondsnvmmbufferfactory.h"
#endif

#include "gst/pylon/gstpyloncache.h"
#include "gst/pylon/gstpylondebug.h"
#include "gst/pylon/gstpylonformatmapping.h"
#include "gst/pylon/gstpylonincludes.h"
#include "gst/pylon/gstpylonmetaprivate.h"
#include "gst/pylon/gstpylonobject.h"
#include "gstchildinspector.h"
#include "gstpylon.h"
#include "gstpylondisconnecthandler.h"
#include "gstpylonimagehandler.h"
#include "gstpylonsysmembufferfactory.h"

#include <map>
#include <vector>

/* retry open camera limits in case of collision with other
 * process
 */
constexpr int FAILED_OPEN_RETRY_COUNT = 30;
constexpr int FAILED_OPEN_RETRY_WAIT_TIME_MS = 1000;

/* Mapping of GstStructure with its corresponding formats */
typedef struct {
  const std::string st_name;
  std::vector<PixelFormatMappingType> format_map;
} GstStPixelFormats;

typedef enum {
  MEM_SYSMEM,
  MEM_NVMM,
} GstPylonMemoryTypeEnum;

/* prototypes */
static std::string gst_pylon_query_default_set(
    const Pylon::CBaslerUniversalInstantCamera &camera);
static void gst_pylon_apply_set(GstPylon *self, std::string &set);
static std::string gst_pylon_get_camera_fullname(
    Pylon::CBaslerUniversalInstantCamera &camera);
static std::string gst_pylon_get_sgrabber_name(
    Pylon::CBaslerUniversalInstantCamera &camera);
static void free_ptr_grab_result(gpointer data);
static void gst_pylon_query_format(
    GstPylon *self, GValue *outvalue,
    const std::vector<PixelFormatMappingType> &pixel_format_mapping);
static void gst_pylon_query_integer(GstPylon *self, GValue *outvalue,
                                    const std::string &name);
static void gst_pylon_query_width(GstPylon *self, GValue *outvalue);
static void gst_pylon_query_height(GstPylon *self, GValue *outvalue);
static void gst_pylon_query_framerate(GstPylon *self, GValue *outvalue);
static void gst_pylon_query_caps(
    GstPylon *self, GstStructure *st,
    const std::vector<PixelFormatMappingType> &pixel_format_mapping);
static void gst_pylon_add_result_meta(
    GstPylon *self, GstBuffer *buf,
    Pylon::CBaslerUniversalGrabResultPtr &grab_result_ptr);
static std::vector<std::string> gst_pylon_gst_to_pfnc(
    const std::string &gst_format,
    const std::vector<PixelFormatMappingType> &pixel_format_mapping);
static std::vector<std::string> gst_pylon_pfnc_to_gst(
    const std::string &genapi_format,
    const std::vector<PixelFormatMappingType> &pixel_format_mapping);
static std::vector<std::string> gst_pylon_pfnc_list_to_gst(
    const GenApi::StringList_t &genapi_formats,
    const std::vector<PixelFormatMappingType> &pixel_format_mapping);
static void gst_pylon_append_properties(
    Pylon::CBaslerUniversalInstantCamera *camera,
    const std::string &device_full_name, const std::string &device_type_str,
    GstPylonCache &feature_cache, GenApi::INodeMap &nodemap,
    gchar **device_properties, guint alignment);
static void gst_pylon_append_camera_properties(
    Pylon::CBaslerUniversalInstantCamera *camera, gchar **camera_properties,
    guint alignment);
static void gst_pylon_append_stream_grabber_properties(
    Pylon::CBaslerUniversalInstantCamera *camera, gchar **sgrabber_properties,
    guint alignment);
typedef void (*GetStringProperties)(Pylon::CBaslerUniversalInstantCamera *,
                                    gchar **, guint);
static gchar *gst_pylon_get_string_properties(
    GetStringProperties get_device_string_properties);

static constexpr gint DEFAULT_ALIGNMENT = 35;

// Default trigger source for HDR sequencer transitions
static constexpr const char* HDR_SEQUENCER_TRIGGER = "ExposureActive";

struct _GstPylon {
  GstElement *gstpylonsrc;
  std::shared_ptr<Pylon::CBaslerUniversalInstantCamera> camera =
      std::make_shared<Pylon::CBaslerUniversalInstantCamera>();
  GObject *gcamera;
  GObject *gstream_grabber;
  GstPylonImageHandler image_handler;
  GstPylonDisconnectHandler disconnect_handler;

  std::shared_ptr<GstPylonBufferFactory> buffer_factory;
  GstPylonMemoryTypeEnum mem_type;

  std::string requested_device_user_name;
  std::string requested_device_serial_number;
  gint requested_device_index;

#ifdef NVMM_ENABLED
  GstPylonNvsurfaceLayoutEnum nvsurface_layout;
  guint gpu_id;
#endif
};

using GrabResultPair = std::pair<std::shared_ptr<GstPylonBufferFactory>,
                                 Pylon::CBaslerUniversalGrabResultPtr *>;

static const std::vector<GstStPixelFormats> gst_structure_formats = {
    {"video/x-raw", pixel_format_mapping_raw},
    {"video/x-bayer", pixel_format_mapping_bayer}};

static std::string gst_pylon_get_camera_fullname(
    Pylon::CBaslerUniversalInstantCamera &camera) {
  return std::string(camera.GetDeviceInfo().GetFullName());
}

static std::string gst_pylon_get_sgrabber_name(
    Pylon::CBaslerUniversalInstantCamera &camera) {
  return gst_pylon_get_camera_fullname(camera) + " StreamGrabber";
}

static std::string gst_pylon_query_default_set(
    const Pylon::CBaslerUniversalInstantCamera &camera) {
  std::string set;

  /* Return default for cameras that don't support wake up default sets e.g
   * CamEmulator */
  if (!camera.UserSetDefault.IsReadable() &&
      !camera.UserSetDefaultSelector.IsReadable()) {
    set = "Default";
  } else if (camera.UserSetDefault.IsReadable()) {
    set = std::string(camera.UserSetDefault.ToString());
  } else {
    set = std::string(camera.UserSetDefaultSelector.ToString());
  }

  return set;
}

static void gst_pylon_apply_set(GstPylon *self, std::string &set) {
  g_return_if_fail(self);

  /* If auto or nothing is set, return default config */
  if ("Auto" == set || set.empty()) {
    set = gst_pylon_query_default_set(*self->camera);
  }

  if (self->camera->UserSetSelector.CanSetValue(set.c_str())) {
    self->camera->UserSetSelector.SetValue(set.c_str());
  } else {
    GenApi::StringList_t values;
    self->camera->UserSetSelector.GetSettableValues(values);
    std::string msg = "Invalid user set, has to be one of the following:\n";
    msg += "Auto\n";

    for (const auto &value : values) {
      msg += std::string(value) + "\n";
    }
    throw Pylon::GenericException(msg.c_str(), __FILE__, __LINE__);
  }

  self->camera->UserSetLoad.Execute();
}

GstPylon *gst_pylon_new(GstElement *gstpylonsrc, const gchar *device_user_name,
                        const gchar *device_serial_number, gint device_index,
                        gboolean enable_correction, GError **err) {
  GstPylon *self = new GstPylon;

  self->gstpylonsrc = gstpylonsrc;

  g_return_val_if_fail(self, NULL);
  g_return_val_if_fail(err && *err == NULL, NULL);

  self->requested_device_index = device_index;
  self->requested_device_user_name = device_user_name ? device_user_name : "";
  self->requested_device_serial_number =
      device_serial_number ? device_serial_number : "";

  try {
    Pylon::CTlFactory &factory = Pylon::CTlFactory::GetInstance();
    Pylon::DeviceInfoList_t filter(1);
    Pylon::DeviceInfoList_t device_list;
    Pylon::CDeviceInfo device_info;

    if (device_user_name) {
      filter[0].SetUserDefinedName(device_user_name);
    }

    if (device_serial_number) {
      filter[0].SetSerialNumber(device_serial_number);
    }

    factory.EnumerateDevices(device_list, filter);

    gint n_devices = device_list.size();
    if (0 == n_devices) {
      throw Pylon::GenericException(
          "No devices found matching the specified criteria", __FILE__,
          __LINE__);
    }

    if (n_devices > 1 && -1 == device_index) {
      std::string msg =
          "At least " + std::to_string(n_devices) +
          " devices match the specified criteria, use "
          "\"device-index\", \"device-serial-number\" or \"device-user-name\""
          " to select one from the following list:\n";

      for (gint i = 0; i < n_devices; i++) {
        msg += "[" + std::to_string(i) +
               "]: " + std::string(device_list.at(i).GetSerialNumber()) + "\t" +
               std::string(device_list.at(i).GetModelName()) + "\t" +
               std::string(device_list.at(i).GetUserDefinedName()) + "\n";
      }
      throw Pylon::GenericException(msg.c_str(), __FILE__, __LINE__);
    }

    if (device_index >= n_devices) {
      std::string msg = "Device index " + std::to_string(device_index) +
                        " exceeds the " + std::to_string(n_devices) +
                        " devices found to match the given criteria";
      throw Pylon::GenericException(msg.c_str(), __FILE__, __LINE__);
    }

    /* Only one device was found, we don't require the user specifying an
     * index
     * and if they did, we already checked for out-of-range errors above */
    if (1 == n_devices) {
      device_index = 0;
    }

    device_info = device_list.at(device_index);

    /* retry loop to start camera
     * handles the cornercase of multiprocess pipelines started
     * concurrently
     */
    for (auto retry_idx = 0; retry_idx <= FAILED_OPEN_RETRY_COUNT;
         retry_idx++) {
      try {
        self->camera->Attach(factory.CreateDevice(device_info));
        break;
      } catch (GenICam::GenericException &e) {
        GST_INFO_OBJECT(gstpylonsrc, "Failed to Open %s (%s)\n",
                        device_info.GetSerialNumber().c_str(),
                        e.GetDescription());
        /* wait for before new open attempt */
        g_usleep(FAILED_OPEN_RETRY_WAIT_TIME_MS * 1000);
      }
    }
    self->camera->Open();

    /* Set the camera to a valid state
     * close left open transactions on the device
     */
    self->camera->DeviceFeaturePersistenceEnd.TryExecute();
    self->camera->DeviceRegistersStreamingEnd.TryExecute();

    /* Set the camera to a valid state
     * load the poweron user set
     */
    if (self->camera->UserSetSelector.IsWritable()) {
      std::string default_set = "Auto";
      gst_pylon_apply_set(self, default_set);
    }

    GenApi::INodeMap &cam_nodemap = self->camera->GetNodeMap();
    self->gcamera = gst_pylon_object_new(
        self->camera, gst_pylon_get_camera_fullname(*self->camera),
        &cam_nodemap, enable_correction);

    GenApi::INodeMap &sgrabber_nodemap =
        self->camera->GetStreamGrabberNodeMap();
    self->gstream_grabber = gst_pylon_object_new(
        self->camera, gst_pylon_get_sgrabber_name(*self->camera),
        &sgrabber_nodemap, enable_correction);

    /* Register event handlers after device instances are requested so they do
     * not get registered if creating the device instances fails */
    self->camera->RegisterImageEventHandler(&self->image_handler,
                                            Pylon::RegistrationMode_Append,
                                            Pylon::Cleanup_None);
    self->disconnect_handler.SetData(self->gstpylonsrc, &self->image_handler);
    self->camera->RegisterConfiguration(&self->disconnect_handler,
                                        Pylon::RegistrationMode_Append,
                                        Pylon::Cleanup_None);
    self->mem_type = MEM_SYSMEM;

#ifdef NVMM_ENABLED
    self->nvsurface_layout = PROP_NVSURFACE_LAYOUT_DEFAULT;
    self->gpu_id = PROP_GPU_ID_DEFAULT;
#endif

  } catch (const Pylon::GenericException &e) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
                e.GetDescription());
    delete self;
    self = NULL;
  }

  return self;
}

gboolean gst_pylon_set_user_config(GstPylon *self, const gchar *user_set,
                                   GError **err) {
  g_return_val_if_fail(self, FALSE);
  g_return_val_if_fail(err && *err == NULL, FALSE);

  try {
    if (!self->camera->UserSetSelector.IsWritable()) {
      GST_INFO(
          "UserSet feature not available"
          " camera will start in internal default state");

      return TRUE;
    }

    std::string set;
    if (user_set) {
      set = std::string(user_set);
    }

    gst_pylon_apply_set(self, set);

  } catch (const Pylon::GenericException &e) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
                e.GetDescription());
    return FALSE;
  }

  return TRUE;
}

gboolean gst_pylon_get_startup_geometry(GstPylon *self, gint *start_width,
                                        gint *start_height) {
  g_return_val_if_fail(self, FALSE);
  g_return_val_if_fail(start_height, FALSE);
  g_return_val_if_fail(start_width, FALSE);

  *start_height = self->camera->Height.GetValue();
  *start_width = self->camera->Width.GetValue();

  return TRUE;
}

gboolean gst_pylon_set_pfs_config(GstPylon *self, const gchar *pfs_location,
                                  GError **err) {
  g_return_val_if_fail(self, FALSE);
  g_return_val_if_fail(pfs_location, FALSE);
  g_return_val_if_fail(err && *err == NULL, FALSE);

  static const bool check_nodemap_sanity = true;

  try {
    Pylon::CFeaturePersistence::Load(pfs_location, &self->camera->GetNodeMap(),
                                     check_nodemap_sanity);
  } catch (const Pylon::GenericException &e) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                "PFS file error: %s", e.GetDescription());
    return FALSE;
  }

  return TRUE;
}

gboolean gst_pylon_configure_hdr_sequence(GstPylon *self, const gchar *hdr_sequence,
                                          gint offset_x, gint offset_y,
                                          GError **err) {
  g_return_val_if_fail(self, FALSE);
  g_return_val_if_fail(err && *err == NULL, FALSE);

  if (!hdr_sequence || strlen(hdr_sequence) == 0) {
    GST_DEBUG("No HDR sequence specified, skipping sequencer configuration");
    return TRUE;
  }

  GST_INFO("Configuring HDR sequence: %s (offset_x=%d, offset_y=%d)",
           hdr_sequence, offset_x, offset_y);

  try {
    GenApi::INodeMap &nodemap = self->camera->GetNodeMap();

    // Parse exposure:gain pairs from comma-separated string
    // Format: "exposure1:gain1,exposure2:gain2" or "exposure1,exposure2" (gain defaults to 0)
    gchar **steps = g_strsplit(hdr_sequence, ",", -1);
    guint num_steps = g_strv_length(steps);

    if (num_steps < 2) {
      g_strfreev(steps);
      g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_SETTINGS,
                  "HDR sequence requires at least 2 steps, got %d", num_steps);
      return FALSE;
    }

    // Parse each step into exposure and gain
    gdouble *exposures = g_new0(gdouble, num_steps);
    gdouble *gains = g_new0(gdouble, num_steps);

    for (guint i = 0; i < num_steps; i++) {
      gchar **parts = g_strsplit(steps[i], ":", 2);

      if (parts[0]) {
        exposures[i] = g_strtod(parts[0], NULL);

        // Parse gain if provided, otherwise default to 0
        if (parts[1]) {
          gains[i] = g_strtod(parts[1], NULL);
        } else {
          gains[i] = 0.0;
        }

        GST_DEBUG("Step %d: exposure=%.2f μs, gain=%.2f", i, exposures[i], gains[i]);
      } else {
        g_strfreev(parts);
        g_strfreev(steps);
        g_free(exposures);
        g_free(gains);
        g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_SETTINGS,
                    "Failed to parse step %d", i);
        return FALSE;
      }

      g_strfreev(parts);
    }

    GST_DEBUG("Configuring sequencer for %d steps", num_steps);

    // Check if sequencer features are available
    Pylon::CEnumParameter sequencerMode(nodemap, "SequencerMode");
    if (!sequencerMode.IsValid()) {
      g_strfreev(steps);
      g_free(exposures);
      g_free(gains);
      g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_SETTINGS,
                  "Camera does not support sequencer mode");
      return FALSE;
    }

    // Get current camera settings BEFORE entering configuration mode
    Pylon::CIntegerParameter currentWidth(nodemap, "Width");
    Pylon::CIntegerParameter currentHeight(nodemap, "Height");
    Pylon::CEnumParameter currentPixelFormat(nodemap, "PixelFormat");

    gint64 width_val = currentWidth.GetValue();
    gint64 height_val = currentHeight.GetValue();
    Pylon::String_t pixelformat_val = currentPixelFormat.GetValue();

    GST_INFO("Current camera settings before sequencer config: Width=%ld, Height=%ld, PixelFormat=%s",
              width_val, height_val, pixelformat_val.c_str());

    // First make sure sequencer mode is OFF before configuring
    if (sequencerMode.IsWritable()) {
      Pylon::String_t current_mode = sequencerMode.GetValue();
      GST_INFO("Current SequencerMode: %s", current_mode.c_str());
      if (current_mode == "On") {
        GST_INFO("Disabling sequencer mode before configuration");
        sequencerMode.SetValue("Off");
        GST_INFO("SequencerMode set to: Off");
      }
    }

    // Enter sequencer configuration mode
    Pylon::CEnumParameter seqConfigMode(nodemap, "SequencerConfigurationMode");
    if (seqConfigMode.IsValid() && seqConfigMode.IsWritable()) {
      Pylon::String_t current_config = seqConfigMode.GetValue();
      GST_INFO("Current SequencerConfigurationMode: %s", current_config.c_str());
      GST_INFO("Entering sequencer configuration mode");
      seqConfigMode.SetValue("On");
      GST_INFO("SequencerConfigurationMode set to: On");
    }

    // Set sequencer trigger source - try different valid options
    Pylon::CEnumParameter seqTriggerSource(nodemap, "SequencerTriggerSource");
    if (seqTriggerSource.IsValid() && seqTriggerSource.IsWritable()) {
      // Get available values for logging
      GenApi::StringList_t entries;
      seqTriggerSource.GetSettableValues(entries);

      GST_DEBUG("Available SequencerTriggerSource values:");
      for (const auto& entry : entries) {
        GST_DEBUG("  - %s", entry.c_str());
      }

      // Try to find a suitable trigger source
      bool trigger_set = false;

      // Try common trigger sources in order of preference
      // ExposureActive confirmed to work on a2A1920-51gcPRO
      const char* trigger_options[] = {HDR_SEQUENCER_TRIGGER, "ExposureStart", "AcquisitionActive", "FrameStart", "AcquisitionStart", NULL};
      for (int i = 0; trigger_options[i] != NULL; i++) {
        if (seqTriggerSource.CanSetValue(trigger_options[i])) {
          GST_INFO("Setting sequencer trigger source to %s", trigger_options[i]);
          seqTriggerSource.SetValue(trigger_options[i]);
          trigger_set = true;
          break;
        }
      }

      // If none of the preferred options work, just use the first available
      if (!trigger_set && entries.size() > 0) {
        GST_WARNING("Using first available trigger source: %s", entries[0].c_str());
        seqTriggerSource.SetValue(entries[0]);
      }
    } else {
      GST_WARNING("SequencerTriggerSource not available or not writable - continuing without setting it");
    }

    // Configure sequencer start set
    Pylon::CIntegerParameter seqSetStart(nodemap, "SequencerSetStart");
    if (seqSetStart.IsValid() && seqSetStart.IsWritable()) {
      gint64 current_start = seqSetStart.GetValue();
      GST_INFO("Current SequencerSetStart: %ld", current_start);
      GST_INFO("Setting sequencer start set to 0");
      seqSetStart.SetValue(0);
      GST_INFO("SequencerSetStart set to: 0");
    }

    // Configure each sequencer set
    Pylon::CIntegerParameter setSelector(nodemap, "SequencerSetSelector");
    Pylon::CIntegerParameter setNext(nodemap, "SequencerSetNext");
    Pylon::CIntegerParameter seqWidth(nodemap, "Width");
    Pylon::CIntegerParameter seqHeight(nodemap, "Height");
    Pylon::CIntegerParameter seqOffsetX(nodemap, "OffsetX");
    Pylon::CIntegerParameter seqOffsetY(nodemap, "OffsetY");
    Pylon::CEnumParameter seqPixelFormat(nodemap, "PixelFormat");

    // Try to get Gain parameter (might be Gain or GainRaw)
    Pylon::CFloatParameter seqGain;
    Pylon::CIntegerParameter seqGainRaw;
    bool has_gain_float = false;
    bool has_gain_raw = false;

    if (nodemap.GetNode("Gain")) {
      seqGain.Attach(nodemap.GetNode("Gain"));
      if (seqGain.IsValid()) {
        has_gain_float = true;
        GST_INFO("Camera supports Gain parameter (float)");
      }
    }

    if (!has_gain_float && nodemap.GetNode("GainRaw")) {
      seqGainRaw.Attach(nodemap.GetNode("GainRaw"));
      if (seqGainRaw.IsValid()) {
        has_gain_raw = true;
        GST_INFO("Camera supports GainRaw parameter (integer)");
      }
    }

    // Try to find the exposure time parameter (might have different names)
    Pylon::CFloatParameter exposureTime;
    if (nodemap.GetNode("ExposureTime")) {
      exposureTime.Attach(nodemap.GetNode("ExposureTime"));
    } else if (nodemap.GetNode("ExposureTimeAbs")) {
      exposureTime.Attach(nodemap.GetNode("ExposureTimeAbs"));
      GST_DEBUG("Using ExposureTimeAbs instead of ExposureTime");
    }

    if (!exposureTime.IsValid()) {
      g_strfreev(steps);
      g_free(exposures);
      g_free(gains);
      g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_SETTINGS,
                  "Camera does not have ExposureTime parameter");
      return FALSE;
    }

    for (guint i = 0; i < num_steps; i++) {
      gdouble exposure = exposures[i];
      gdouble gain = gains[i];
      guint next_set = (i + 1) % num_steps;  // Loop back to first set

      GST_DEBUG("Configuring set %d: exposure=%.2f μs, gain=%.2f, next=%d",
                i, exposure, gain, next_set);

      // Select the set
      if (setSelector.IsValid() && setSelector.IsWritable()) {
        GST_INFO("=== Configuring Sequencer Set %d ===", i);
        setSelector.SetValue(i);
        GST_INFO("SequencerSetSelector set to: %d", i);

        // Load the current set configuration (ace 2 uses SequencerSetLoad)
        GenApi::INode* loadNode = nodemap.GetNode("SequencerSetLoad");
        if (loadNode) {
          GenApi::ICommand* loadCmd = dynamic_cast<GenApi::ICommand*>(loadNode);
          if (loadCmd && GenApi::IsWritable(loadCmd)) {
            GST_INFO("Executing SequencerSetLoad for Set %d", i);
            loadCmd->Execute();
            GST_INFO("SequencerSetLoad executed successfully for Set %d", i);
          } else {
            GST_DEBUG("SequencerSetLoad command not writable for Set %d", i);
          }
        } else {
          GST_DEBUG("SequencerSetLoad command not found - changes may apply immediately");
        }
      } else {
        GST_WARNING("Cannot select sequencer set %d - SequencerSetSelector not available", i);
      }

      // Preserve image format settings in each set
      if (seqWidth.IsValid() && seqWidth.IsWritable()) {
        gint64 current_w = seqWidth.GetValue();
        GST_INFO("Set %d: Width is %ld, setting to %ld", i, current_w, width_val);
        seqWidth.SetValue(width_val);
        GST_INFO("Set %d: Width set to %ld", i, width_val);
      }
      if (seqHeight.IsValid() && seqHeight.IsWritable()) {
        gint64 current_h = seqHeight.GetValue();
        GST_INFO("Set %d: Height is %ld, setting to %ld", i, current_h, height_val);
        seqHeight.SetValue(height_val);
        GST_INFO("Set %d: Height set to %ld", i, height_val);
      }

      // Set sensor offsets (must be set after Width/Height)
      if (offset_x > 0 && seqOffsetX.IsValid() && seqOffsetX.IsWritable()) {
        gint64 current_ox = seqOffsetX.GetValue();
        GST_INFO("Set %d: OffsetX is %ld, setting to %d", i, current_ox, offset_x);
        seqOffsetX.SetValue(offset_x);
        GST_INFO("Set %d: OffsetX set to %d", i, offset_x);
      }
      if (offset_y > 0 && seqOffsetY.IsValid() && seqOffsetY.IsWritable()) {
        gint64 current_oy = seqOffsetY.GetValue();
        GST_INFO("Set %d: OffsetY is %ld, setting to %d", i, current_oy, offset_y);
        seqOffsetY.SetValue(offset_y);
        GST_INFO("Set %d: OffsetY set to %d", i, offset_y);
      }

      if (seqPixelFormat.IsValid() && seqPixelFormat.IsWritable()) {
        Pylon::String_t current_fmt = seqPixelFormat.GetValue();
        GST_INFO("Set %d: PixelFormat is %s, setting to %s", i,
                 current_fmt.c_str(), pixelformat_val.c_str());
        seqPixelFormat.SetValue(pixelformat_val);
        GST_INFO("Set %d: PixelFormat set to %s", i, pixelformat_val.c_str());
      }

      // Set Gain for this set
      if (has_gain_float && seqGain.IsWritable()) {
        gdouble current_gain = seqGain.IsReadable() ? seqGain.GetValue() : -1.0;
        GST_INFO("Set %d: Gain is %.2f, setting to %.2f", i, current_gain, gain);
        seqGain.SetValue(gain);
        GST_INFO("Set %d: Gain set to %.2f", i, gain);
      } else if (has_gain_raw && seqGainRaw.IsWritable()) {
        gint64 current_gain = seqGainRaw.IsReadable() ? seqGainRaw.GetValue() : -1;
        GST_INFO("Set %d: GainRaw is %ld, setting to %.0f", i, current_gain, gain);
        seqGainRaw.SetValue((gint64)gain);
        GST_INFO("Set %d: GainRaw set to %.0f", i, gain);
      } else if (gain != 0.0) {
        GST_WARNING("Set %d: Gain parameter not available or not writable, cannot set gain=%.2f",
                    i, gain);
      }

      // Set exposure time for this set
      if (exposureTime.IsValid() && exposureTime.IsWritable()) {
        gdouble current_exp = exposureTime.GetValue();
        GST_INFO("Set %d: ExposureTime was %.2f μs, setting to %.2f μs",
                 i, current_exp, exposure);
        exposureTime.SetValue(exposure);
        GST_INFO("Set %d: ExposureTime configured to %.2f μs", i, exposure);
      } else {
        GST_WARNING("Cannot set exposure time for set %d", i);
      }

      // Configure next set in sequence
      if (setNext.IsValid() && setNext.IsWritable()) {
        gint64 current_next = setNext.GetValue();
        GST_INFO("Set %d: SequencerSetNext was %ld, setting to %d",
                 i, current_next, next_set);
        setNext.SetValue(next_set);
        GST_INFO("Set %d: SequencerSetNext configured to %d", i, next_set);
      } else {
        GST_WARNING("Cannot configure next set for set %d", i);
      }

      // Save the configured set (ace 2 uses SequencerSetSave)
      GenApi::INode* saveNode = nodemap.GetNode("SequencerSetSave");
      if (saveNode) {
        GenApi::ICommand* saveCmd = dynamic_cast<GenApi::ICommand*>(saveNode);
        if (saveCmd && GenApi::IsWritable(saveCmd)) {
          GST_INFO("Executing SequencerSetSave for Set %d", i);
          saveCmd->Execute();
          GST_INFO("SequencerSetSave executed successfully for Set %d", i);
        } else {
          GST_DEBUG("SequencerSetSave command not writable for Set %d", i);
        }
      } else {
        GST_DEBUG("SequencerSetSave command not found - changes may apply immediately");
      }
    }

    // Exit configuration mode
    if (seqConfigMode.IsValid() && seqConfigMode.IsWritable()) {
      GST_INFO("Exiting sequencer configuration mode");
      seqConfigMode.SetValue("Off");
      Pylon::String_t config_mode = seqConfigMode.GetValue();
      GST_INFO("SequencerConfigurationMode set to: %s", config_mode.c_str());
    }

    // Enable sequencer mode
    if (sequencerMode.IsWritable()) {
      GST_INFO("Enabling sequencer mode");
      sequencerMode.SetValue("On");
      Pylon::String_t seq_mode = sequencerMode.GetValue();
      GST_INFO("SequencerMode set to: %s", seq_mode.c_str());
    } else {
      GST_WARNING("Cannot enable sequencer mode - not writable");
    }

    g_strfreev(steps);
    g_free(exposures);
    g_free(gains);
    GST_INFO("HDR sequence configuration completed successfully");

  } catch (const Pylon::GenericException &e) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                "Failed to configure HDR sequence: %s", e.GetDescription());
    return FALSE;
  }

  return TRUE;
}

gboolean gst_pylon_configure_dual_hdr_sequence(GstPylon *self,
                                               const gchar *hdr_sequence1,
                                               const gchar *hdr_sequence2,
                                               gint offset_x, gint offset_y,
                                               GError **err) {
  g_return_val_if_fail(self, FALSE);
  g_return_val_if_fail(err && *err == NULL, FALSE);

  if (!hdr_sequence1 || strlen(hdr_sequence1) == 0 ||
      !hdr_sequence2 || strlen(hdr_sequence2) == 0) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_SETTINGS,
                "Both HDR sequences must be specified for dual profile mode");
    return FALSE;
  }

  GST_INFO("Configuring dual HDR profiles with path branching (offset_x=%d, offset_y=%d):",
           offset_x, offset_y);
  GST_INFO("  Profile 0: %s", hdr_sequence1);
  GST_INFO("  Profile 1: %s", hdr_sequence2);

  try {
    GenApi::INodeMap &nodemap = self->camera->GetNodeMap();

    // Parse exposure:gain pairs from both sequences
    // Format: "exposure1:gain1,exposure2:gain2" or "exposure1,exposure2" (gain defaults to 0)
    gchar **steps1 = g_strsplit(hdr_sequence1, ",", -1);
    gchar **steps2 = g_strsplit(hdr_sequence2, ",", -1);
    guint num_steps1 = g_strv_length(steps1);
    guint num_steps2 = g_strv_length(steps2);

    // Validate sequence lengths
    if (num_steps1 == 0 || num_steps2 == 0) {
      g_strfreev(steps1);
      g_strfreev(steps2);
      g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_SETTINGS,
                  "HDR sequences must have at least 1 step");
      return FALSE;
    }

    // Parse Profile 0 steps into exposure and gain arrays
    gdouble *exposures1 = g_new0(gdouble, num_steps1);
    gdouble *gains1 = g_new0(gdouble, num_steps1);

    for (guint i = 0; i < num_steps1; i++) {
      gchar **parts = g_strsplit(steps1[i], ":", 2);

      if (parts[0]) {
        exposures1[i] = g_strtod(parts[0], NULL);
        gains1[i] = parts[1] ? g_strtod(parts[1], NULL) : 0.0;
        GST_DEBUG("Profile 0 Step %d: exposure=%.2f μs, gain=%.2f", i, exposures1[i], gains1[i]);
      } else {
        g_strfreev(parts);
        g_strfreev(steps1);
        g_strfreev(steps2);
        g_free(exposures1);
        g_free(gains1);
        g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_SETTINGS,
                    "Failed to parse Profile 0 step %d", i);
        return FALSE;
      }

      g_strfreev(parts);
    }

    // Parse Profile 1 steps into exposure and gain arrays
    gdouble *exposures2 = g_new0(gdouble, num_steps2);
    gdouble *gains2 = g_new0(gdouble, num_steps2);

    for (guint i = 0; i < num_steps2; i++) {
      gchar **parts = g_strsplit(steps2[i], ":", 2);

      if (parts[0]) {
        exposures2[i] = g_strtod(parts[0], NULL);
        gains2[i] = parts[1] ? g_strtod(parts[1], NULL) : 0.0;
        GST_DEBUG("Profile 1 Step %d: exposure=%.2f μs, gain=%.2f", i, exposures2[i], gains2[i]);
      } else {
        g_strfreev(parts);
        g_strfreev(steps1);
        g_strfreev(steps2);
        g_free(exposures1);
        g_free(gains1);
        g_free(exposures2);
        g_free(gains2);
        g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_SETTINGS,
                    "Failed to parse Profile 1 step %d", i);
        return FALSE;
      }

      g_strfreev(parts);
    }

    // Check maximum sets limit (most cameras support 16 sets)
    guint total_sets = num_steps1 + num_steps2;
    if (total_sets > 16) {
      g_strfreev(steps1);
      g_strfreev(steps2);
      g_free(exposures1);
      g_free(gains1);
      g_free(exposures2);
      g_free(gains2);
      g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_SETTINGS,
                  "Total number of sets (%d) exceeds typical camera limit of 16", total_sets);
      return FALSE;
    }

    GST_INFO("Profile 0: %d steps, Profile 1: %d steps, Total sets: %d",
              num_steps1, num_steps2, total_sets);

    // Check if sequencer features are available
    Pylon::CEnumParameter sequencerMode(nodemap, "SequencerMode");
    if (!sequencerMode.IsValid()) {
      g_strfreev(steps1);
      g_strfreev(steps2);
      g_free(exposures1);
      g_free(gains1);
      g_free(exposures2);
      g_free(gains2);
      g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_SETTINGS,
                  "Camera does not support sequencer mode");
      return FALSE;
    }

    // Get current camera settings BEFORE entering configuration mode
    Pylon::CIntegerParameter currentWidth(nodemap, "Width");
    Pylon::CIntegerParameter currentHeight(nodemap, "Height");
    Pylon::CEnumParameter currentPixelFormat(nodemap, "PixelFormat");

    gint64 width_val = currentWidth.GetValue();
    gint64 height_val = currentHeight.GetValue();
    Pylon::String_t pixelformat_val = currentPixelFormat.GetValue();

    GST_INFO("Current camera settings: Width=%ld, Height=%ld, PixelFormat=%s",
              width_val, height_val, pixelformat_val.c_str());

    // First make sure sequencer mode is OFF
    if (sequencerMode.IsWritable()) {
      sequencerMode.SetValue("Off");
      GST_INFO("Sequencer mode disabled for configuration");
    }

    // Enter sequencer configuration mode
    Pylon::CEnumParameter seqConfigMode(nodemap, "SequencerConfigurationMode");
    if (seqConfigMode.IsValid() && seqConfigMode.IsWritable()) {
      seqConfigMode.SetValue("On");
      GST_INFO("Entered sequencer configuration mode");
    }

    // Get sequencer parameters
    Pylon::CIntegerParameter setSelector(nodemap, "SequencerSetSelector");
    Pylon::CIntegerParameter pathSelector(nodemap, "SequencerPathSelector");
    Pylon::CIntegerParameter setNext(nodemap, "SequencerSetNext");
    Pylon::CEnumParameter seqTriggerSource(nodemap, "SequencerTriggerSource");
    Pylon::CIntegerParameter seqWidth(nodemap, "Width");
    Pylon::CIntegerParameter seqHeight(nodemap, "Height");
    Pylon::CIntegerParameter seqOffsetX(nodemap, "OffsetX");
    Pylon::CIntegerParameter seqOffsetY(nodemap, "OffsetY");
    Pylon::CEnumParameter seqPixelFormat(nodemap, "PixelFormat");
    Pylon::CFloatParameter exposureTime(nodemap, "ExposureTime");

    // Check for path selector support
    if (!pathSelector.IsValid()) {
      g_strfreev(steps1);
      g_strfreev(steps2);
      g_free(exposures1);
      g_free(gains1);
      g_free(exposures2);
      g_free(gains2);
      g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_SETTINGS,
                  "Camera does not support SequencerPathSelector - required for dual profiles");
      return FALSE;
    }

    // Configure sequencer start set to 0
    Pylon::CIntegerParameter seqSetStart(nodemap, "SequencerSetStart");
    if (seqSetStart.IsValid() && seqSetStart.IsWritable()) {
      seqSetStart.SetValue(0);
      GST_INFO("SequencerSetStart = 0");
    }

    // Get Gain parameter if available
    Pylon::CFloatParameter seqGain;
    Pylon::CIntegerParameter seqGainRaw;
    bool has_gain_float = false;
    bool has_gain_raw = false;

    if (nodemap.GetNode("Gain")) {
      seqGain.Attach(nodemap.GetNode("Gain"));
      if (seqGain.IsValid()) {
        has_gain_float = true;
        GST_INFO("Camera supports Gain parameter (float)");
      }
    }

    if (!has_gain_float && nodemap.GetNode("GainRaw")) {
      seqGainRaw.Attach(nodemap.GetNode("GainRaw"));
      if (seqGainRaw.IsValid()) {
        has_gain_raw = true;
        GST_INFO("Camera supports GainRaw parameter (integer)");
      }
    }

    // Check exposure time parameter
    if (!exposureTime.IsValid()) {
      // Try alternative name
      exposureTime.Attach(nodemap.GetNode("ExposureTimeAbs"));
      if (!exposureTime.IsValid()) {
        g_strfreev(steps1);
        g_strfreev(steps2);
        g_free(exposures1);
        g_free(gains1);
        g_free(exposures2);
        g_free(gains2);
        g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_SETTINGS,
                    "Camera does not have ExposureTime parameter");
        return FALSE;
      }
    }

    // Helper lambda to configure common settings for each set
    auto configureCommonSettings = [&](guint set_num, gdouble exposure, gdouble gain) {
      GST_INFO("=== Configuring Set %d ===", set_num);

      // Select the set
      setSelector.SetValue(set_num);

      // Load the current set configuration
      GenApi::INode* loadNode = nodemap.GetNode("SequencerSetLoad");
      if (loadNode) {
        GenApi::ICommand* loadCmd = dynamic_cast<GenApi::ICommand*>(loadNode);
        if (loadCmd && GenApi::IsWritable(loadCmd)) {
          loadCmd->Execute();
        }
      }

      // Set common parameters (width, height, pixel format)
      if (seqWidth.IsValid() && seqWidth.IsWritable()) {
        seqWidth.SetValue(width_val);
      }
      if (seqHeight.IsValid() && seqHeight.IsWritable()) {
        seqHeight.SetValue(height_val);
      }

      // Set sensor offsets (must be set after Width/Height)
      if (offset_x > 0 && seqOffsetX.IsValid() && seqOffsetX.IsWritable()) {
        seqOffsetX.SetValue(offset_x);
        GST_INFO("  OffsetX = %d", offset_x);
      }
      if (offset_y > 0 && seqOffsetY.IsValid() && seqOffsetY.IsWritable()) {
        seqOffsetY.SetValue(offset_y);
        GST_INFO("  OffsetY = %d", offset_y);
      }

      if (seqPixelFormat.IsValid() && seqPixelFormat.IsWritable()) {
        seqPixelFormat.SetValue(pixelformat_val);
      }

      // Set gain for this set
      if (has_gain_float && seqGain.IsWritable()) {
        seqGain.SetValue(gain);
        GST_INFO("  Gain = %.2f", gain);
      } else if (has_gain_raw && seqGainRaw.IsWritable()) {
        seqGainRaw.SetValue((gint64)gain);
        GST_INFO("  GainRaw = %.0f", gain);
      } else if (gain != 0.0) {
        GST_WARNING("  Gain parameter not available or not writable, cannot set gain=%.2f", gain);
      }

      // Set exposure time
      exposureTime.SetValue(exposure);
      GST_INFO("  ExposureTime = %.2f μs", exposure);
    };

    // Helper lambda to save current set configuration
    auto saveSet = [&]() {
      GenApi::INode* saveNode = nodemap.GetNode("SequencerSetSave");
      if (saveNode) {
        GenApi::ICommand* saveCmd = dynamic_cast<GenApi::ICommand*>(saveNode);
        if (saveCmd && GenApi::IsWritable(saveCmd)) {
          saveCmd->Execute();
          GST_DEBUG("  SequencerSetSave executed");
        }
      }
    };

    // Calculate set indices for profiles
    guint profile0_first = 0;
    guint profile0_last = num_steps1 - 1;
    guint profile1_first = num_steps1;
    guint profile1_last = num_steps1 + num_steps2 - 1;

    GST_INFO("Set allocation: Profile 0 [%d-%d], Profile 1 [%d-%d]",
             profile0_first, profile0_last, profile1_first, profile1_last);

    // ===== Configure Profile 0 sets =====
    for (guint i = 0; i < num_steps1; i++) {
      guint set_num = i;
      gdouble exposure = exposures1[i];
      gdouble gain = gains1[i];

      GST_INFO("=== Configuring Set %d (Profile 0, step %d/%d: %.2fμs, gain=%.2f) ===",
               set_num, i+1, num_steps1, exposure, gain);

      configureCommonSettings(set_num, exposure, gain);

      // ALL sets in Profile 0 get software signal on Path 0
      // Path 0: Switch to Profile 1 on SoftwareSignal1 (checked first)
      pathSelector.SetValue(0);
      setNext.SetValue(profile1_first);
      if (!seqTriggerSource.IsValid()) {
        g_strfreev(steps1);
        g_strfreev(steps2);
        g_free(exposures1);
        g_free(gains1);
        g_free(exposures2);
        g_free(gains2);
        throw Pylon::GenericException("SequencerTriggerSource parameter not available", __FILE__, __LINE__);
      }
      if (!seqTriggerSource.CanSetValue("SoftwareSignal1")) {
        g_strfreev(steps1);
        g_strfreev(steps2);
        g_free(exposures1);
        g_free(gains1);
        g_free(exposures2);
        g_free(gains2);
        throw Pylon::GenericException("Cannot set SequencerTriggerSource to SoftwareSignal1", __FILE__, __LINE__);
      }
      seqTriggerSource.SetValue("SoftwareSignal1");
      GST_INFO("  Path 0: Next = %d, Trigger = SoftwareSignal1 (switch to Profile 1)", profile1_first);

      // Path 1: Default path - normal progression with ExposureActive trigger
      pathSelector.SetValue(1);
      if (i == profile0_last) {
        // Last set: loop back to beginning
        setNext.SetValue(profile0_first);
        GST_INFO("  Path 1 (default): Next = %d, Trigger = %s (loop Profile 0)", profile0_first, HDR_SEQUENCER_TRIGGER);
      } else {
        // Other sets: go to next set
        setNext.SetValue(set_num + 1);
        GST_INFO("  Path 1 (default): Next = %d, Trigger = %s", set_num + 1, HDR_SEQUENCER_TRIGGER);
      }
      if (!seqTriggerSource.CanSetValue(HDR_SEQUENCER_TRIGGER)) {
        g_strfreev(steps1);
        g_strfreev(steps2);
        g_free(exposures1);
        g_free(gains1);
        g_free(exposures2);
        g_free(gains2);
        throw Pylon::GenericException(
          (std::string("Cannot set SequencerTriggerSource to ") + HDR_SEQUENCER_TRIGGER).c_str(),
          __FILE__, __LINE__);
      }
      seqTriggerSource.SetValue(HDR_SEQUENCER_TRIGGER);

      // Save the set ONCE with BOTH paths configured
      saveSet();
    }

    // ===== Configure Profile 1 sets =====
    for (guint i = 0; i < num_steps2; i++) {
      guint set_num = profile1_first + i;
      gdouble exposure = exposures2[i];
      gdouble gain = gains2[i];

      GST_INFO("=== Configuring Set %d (Profile 1, step %d/%d: %.2fμs, gain=%.2f) ===",
               set_num, i+1, num_steps2, exposure, gain);

      configureCommonSettings(set_num, exposure, gain);

      // ALL sets in Profile 1 get software signal on Path 0
      // Path 0: Switch to Profile 0 on SoftwareSignal2 (checked first)
      pathSelector.SetValue(0);
      setNext.SetValue(profile0_first);
      if (!seqTriggerSource.IsValid()) {
        g_strfreev(steps1);
        g_strfreev(steps2);
        g_free(exposures1);
        g_free(gains1);
        g_free(exposures2);
        g_free(gains2);
        throw Pylon::GenericException("SequencerTriggerSource parameter not available", __FILE__, __LINE__);
      }
      if (!seqTriggerSource.CanSetValue("SoftwareSignal2")) {
        g_strfreev(steps1);
        g_strfreev(steps2);
        g_free(exposures1);
        g_free(gains1);
        g_free(exposures2);
        g_free(gains2);
        throw Pylon::GenericException("Cannot set SequencerTriggerSource to SoftwareSignal2", __FILE__, __LINE__);
      }
      seqTriggerSource.SetValue("SoftwareSignal2");
      GST_INFO("  Path 0: Next = %d, Trigger = SoftwareSignal2 (switch to Profile 0)", profile0_first);

      // Path 1: Default path - normal progression with ExposureActive trigger
      pathSelector.SetValue(1);
      if (set_num == profile1_last) {
        // Last set: loop back to beginning
        setNext.SetValue(profile1_first);
        GST_INFO("  Path 1 (default): Next = %d, Trigger = %s (loop Profile 1)", profile1_first, HDR_SEQUENCER_TRIGGER);
      } else {
        // Other sets: go to next set
        setNext.SetValue(set_num + 1);
        GST_INFO("  Path 1 (default): Next = %d, Trigger = %s", set_num + 1, HDR_SEQUENCER_TRIGGER);
      }
      if (!seqTriggerSource.CanSetValue(HDR_SEQUENCER_TRIGGER)) {
        g_strfreev(steps1);
        g_strfreev(steps2);
        g_free(exposures1);
        g_free(gains1);
        g_free(exposures2);
        g_free(gains2);
        throw Pylon::GenericException(
          (std::string("Cannot set SequencerTriggerSource to ") + HDR_SEQUENCER_TRIGGER).c_str(),
          __FILE__, __LINE__);
      }
      seqTriggerSource.SetValue(HDR_SEQUENCER_TRIGGER);

      // Save the set ONCE with BOTH paths configured
      saveSet();
    }

    // Exit configuration mode
    if (seqConfigMode.IsValid() && seqConfigMode.IsWritable()) {
      seqConfigMode.SetValue("Off");
      GST_INFO("Exited sequencer configuration mode");
    }

    // Enable sequencer mode
    if (sequencerMode.IsWritable()) {
      sequencerMode.SetValue("On");
      GST_INFO("Sequencer mode enabled");
    }

    g_strfreev(steps1);
    g_strfreev(steps2);
    g_free(exposures1);
    g_free(gains1);
    g_free(exposures2);
    g_free(gains2);
    GST_INFO("Dual HDR profile configuration with path branching completed successfully");

  } catch (const Pylon::GenericException &e) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                "Failed to configure dual HDR profiles: %s", e.GetDescription());
    return FALSE;
  }

  return TRUE;
}

gboolean gst_pylon_switch_hdr_profile(GstPylon *self, gint profile, GError **err) {
  g_return_val_if_fail(self, FALSE);
  g_return_val_if_fail(err && *err == NULL, FALSE);
  g_return_val_if_fail(profile == 0 || profile == 1, FALSE);

  try {
    GenApi::INodeMap &nodemap = self->camera->GetNodeMap();

    // Use SoftwareSignalSelector and SoftwareSignalPulse to trigger the profile switch
    Pylon::CEnumParameter signalSelector(nodemap, "SoftwareSignalSelector");
    Pylon::CCommandParameter signalPulse(nodemap, "SoftwareSignalPulse");

    if (signalSelector.IsValid() && signalPulse.IsValid()) {
      // Select the appropriate software signal
      // To switch TO Profile 1: trigger SoftwareSignal1 (Set 1 Path 1 listens for this)
      // To switch TO Profile 0: trigger SoftwareSignal2 (Set 4 Path 1 listens for this)
      const char* signal_value = (profile == 1) ? "SoftwareSignal1" : "SoftwareSignal2";

      GST_DEBUG("Attempting to switch to profile %d using signal %s", profile, signal_value);

      if (signalSelector.CanSetValue(signal_value)) {
        signalSelector.SetValue(signal_value);
        GST_DEBUG("SoftwareSignalSelector set to %s", signal_value);

        // Execute the pulse command
        if (GenApi::IsWritable(signalPulse.GetNode())) {
          signalPulse.Execute();
          GST_INFO("Executed %s pulse to switch to HDR Profile %d", signal_value, profile);

          // Verify the signal was set correctly
          try {
            Pylon::String_t current_signal = signalSelector.GetValue();
            GST_DEBUG("After pulse, SoftwareSignalSelector reads: %s", current_signal.c_str());
          } catch (...) {
            GST_DEBUG("Could not read back SoftwareSignalSelector value");
          }

          return TRUE;
        } else {
          GST_WARNING("SoftwareSignalPulse command not writable after selecting %s", signal_value);
        }
      } else {
        GST_WARNING("Cannot set SoftwareSignalSelector to %s", signal_value);
      }
    } else {
      GST_WARNING("SoftwareSignalSelector=%d, SoftwareSignalPulse=%d",
                  signalSelector.IsValid(), signalPulse.IsValid());
    }

    // If the primary method didn't work, log more details for debugging
    GST_WARNING("Software signal switching failed - debugging info:");

    // Check what signals are available
    if (signalSelector.IsValid()) {
      GenApi::StringList_t available_signals;
      signalSelector.GetSettableValues(available_signals);
      GST_WARNING("Available software signals:");
      for (const auto& sig : available_signals) {
        GST_WARNING("  - %s", sig.c_str());
      }

      // Try to read current value
      try {
        Pylon::String_t current = signalSelector.GetValue();
        GST_WARNING("Current SoftwareSignalSelector value: %s", current.c_str());
      } catch (...) {
        GST_WARNING("Could not read current SoftwareSignalSelector value");
      }
    }

    GST_ERROR("Could not trigger software signal for profile switching");
    GST_ERROR("Attempting to switch to profile %d failed", profile);
    GST_ERROR("The sequencer will continue with the current profile");

    // Don't return FALSE to avoid failing the pipeline, but the switch didn't work
    return TRUE;

  } catch (const Pylon::GenericException &e) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                "Failed to switch HDR profile: %s", e.GetDescription());
    return FALSE;
  }
}

void gst_pylon_free(GstPylon *self) {
  g_return_if_fail(self);

  self->camera->DeregisterImageEventHandler(&self->image_handler);
  self->camera->DeregisterConfiguration(&self->disconnect_handler);
  self->camera->Close();
  g_object_unref(self->gcamera);

  delete self;
}

gboolean gst_pylon_start(GstPylon *self, GError **err) {
  gboolean ret = TRUE;

  g_return_val_if_fail(self, FALSE);
  g_return_val_if_fail(err && *err == NULL, FALSE);

  try {
    self->camera->StartGrabbing(Pylon::GrabStrategy_LatestImageOnly,
                                Pylon::GrabLoop_ProvidedByInstantCamera);
  } catch (const Pylon::GenericException &e) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
                e.GetDescription());
    ret = FALSE;
  }

  return ret;
}

gboolean gst_pylon_stop(GstPylon *self, GError **err) {
  gboolean ret = TRUE;

  g_return_val_if_fail(self, FALSE);
  g_return_val_if_fail(err && *err == NULL, FALSE);

  try {
    self->camera->StopGrabbing();
  } catch (const Pylon::GenericException &e) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
                e.GetDescription());
    ret = FALSE;
  }

  return ret;
}

void gst_pylon_interrupt_capture(GstPylon *self) {
  g_return_if_fail(self);
  self->image_handler.InterruptWaitForImage();
}

static void gst_pylon_add_result_meta(
    GstPylon *self, GstBuffer *buf,
    Pylon::CBaslerUniversalGrabResultPtr &grab_result_ptr) {
  g_return_if_fail(self);
  g_return_if_fail(buf);

  gst_buffer_add_pylon_meta(buf, grab_result_ptr);
}

static void free_ptr_grab_result(gpointer data) {
  g_return_if_fail(data);

  auto wrapped_data = static_cast<GrabResultPair *>(data);

  Pylon::CBaslerUniversalGrabResultPtr *ptr_grab_result = wrapped_data->second;
  delete ptr_grab_result;

  delete wrapped_data;
}

gboolean gst_pylon_capture(GstPylon *self, GstBuffer **buf,
                           GstPylonCaptureErrorEnum capture_error,
                           std::atomic<guint64> *error_count, GError **err) {
  g_return_val_if_fail(self, FALSE);
  g_return_val_if_fail(buf, FALSE);
  g_return_val_if_fail(err && *err == NULL, FALSE);

  bool retry_grab = true;
  bool buffer_error = false;
  gint retry_frame_counter = 0;
  static const gint max_frames_to_skip = G_MAXINT - 16;
  Pylon::CBaslerUniversalGrabResultPtr *grab_result_ptr = NULL;

  while (retry_grab) {
    grab_result_ptr = self->image_handler.WaitForImage();

    /* Return if user requests to interrupt the grabbing thread */
    if (!grab_result_ptr) {
      return FALSE;
    }

    if ((*grab_result_ptr)->GrabSucceeded()) {
      break;
    }

    std::string error_message =
        std::string((*grab_result_ptr)->GetErrorDescription());
    switch (capture_error) {
      case ENUM_KEEP:
        /* Deliver the buffer into pipeline even if pylon reports an error */
        GST_ELEMENT_WARNING(self->gstpylonsrc, LIBRARY, FAILED,
                            ("Capture failed. Keeping buffer."),
                            ("%s", error_message.c_str()));
        error_count->fetch_add(1, std::memory_order_relaxed);
        retry_grab = false;
        break;
      case ENUM_ABORT:
        /* Signal an error to abort pipeline */
        buffer_error = true;
        break;
      case ENUM_SKIP:
        /* Fail if max number of skipped frames is reached */
        if (retry_frame_counter == max_frames_to_skip) {
          error_message = "Max number of allowed buffer skips reached (" +
                          std::to_string(max_frames_to_skip) +
                          "): " + error_message;
          buffer_error = true;
        } else {
          /* Retry to capture next buffer and release current pylon buffer */
          GST_ELEMENT_WARNING(self->gstpylonsrc, LIBRARY, FAILED,
                              ("Capture failed. Skipping buffer."),
                              ("%s", error_message.c_str()));
          error_count->fetch_add(1, std::memory_order_relaxed);
          delete grab_result_ptr;
          grab_result_ptr = NULL;
          retry_grab = true;
          retry_frame_counter += 1;
        }
        break;
    };

    if (buffer_error) {
      g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
                  error_message.c_str());
      delete grab_result_ptr;
      grab_result_ptr = NULL;
      return FALSE;
    }
  };

#ifdef NVMM_ENABLED
  if (MEM_NVMM == self->mem_type) {
    NvBufSurface *surf = reinterpret_cast<NvBufSurface *>(
        (*grab_result_ptr)->GetBufferContext());

    size_t src_stride;
    (*grab_result_ptr)->GetStride(src_stride);

    /* calc src width in byte from pixel type info */
    const auto src_width_pix = (*grab_result_ptr)->GetWidth();
    const auto src_bit_per_pix =
        Pylon::BitPerPixel((*grab_result_ptr)->GetPixelType());

    g_assert(0 == (src_width_pix * src_bit_per_pix) % 8);
    const size_t src_width = (src_width_pix * src_bit_per_pix) >> 3;

    cudaError_t cuda_err = cudaMemcpy2D(
        surf->surfaceList[0].mappedAddr.addr[0], surf->surfaceList[0].pitch,
        (*grab_result_ptr)->GetBuffer(), src_stride, src_width,
        (*grab_result_ptr)->GetHeight(), cudaMemcpyDefault);
    if (cuda_err != cudaSuccess) {
      g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                  "Error copying memory to device");
      return FALSE;
    }

    auto buffer_ref = new GrabResultPair(self->buffer_factory, grab_result_ptr);
    *buf = gst_buffer_new_wrapped_full(
        GST_MEMORY_FLAG_READONLY, surf, sizeof(*surf), 0, sizeof(*surf),
        buffer_ref, static_cast<GDestroyNotify>(free_ptr_grab_result));
  } else {
#endif
    gsize buffer_size = (*grab_result_ptr)->GetImageSize();
    auto buffer_ref = new GrabResultPair(self->buffer_factory, grab_result_ptr);
    *buf = gst_buffer_new_wrapped_full(
        static_cast<GstMemoryFlags>(0), (*grab_result_ptr)->GetBuffer(),
        buffer_size, 0, buffer_size, buffer_ref,
        static_cast<GDestroyNotify>(free_ptr_grab_result));
#ifdef NVMM_ENABLED
  }
#endif

  gst_pylon_add_result_meta(self, *buf, *grab_result_ptr);

  // Debug output for HDR sequences - show actual exposure time of captured frame
  try {
    static gint frame_counter = 0;
    frame_counter++;

    // IMPORTANT: Read exposure from chunk data in the grab result
    // This gives us the actual exposure used for THIS specific frame
    if ((*grab_result_ptr)->IsChunkDataAvailable()) {

      // Try to access ChunkExposureTime from the grab result
      try {
        // The chunk parser needs to be initialized
        (*grab_result_ptr)->GetChunkDataNodeMap();

        // Now try to get the exposure time chunk
        if ((*grab_result_ptr)->ChunkExposureTime.IsValid()) {
          gdouble chunk_exposure = (*grab_result_ptr)->ChunkExposureTime.GetValue();
          GST_DEBUG("Frame %d captured with exposure: %.2fμs (from chunk)",
                   frame_counter, chunk_exposure);
        } else {
          GST_DEBUG("HDR Frame %d - ChunkExposureTime not valid in grab result", frame_counter);

          // Try alternative chunk name
          GenApi::INodeMap &chunkNodeMap = (*grab_result_ptr)->GetChunkDataNodeMap();
          GenApi::CFloatPtr exposureChunk = chunkNodeMap.GetNode("ChunkExposureTime");
          if (!exposureChunk.IsValid()) {
            exposureChunk = chunkNodeMap.GetNode("ChunkExposureTimeAbs");
          }

          if (exposureChunk.IsValid()) {
            gdouble chunk_exp = exposureChunk->GetValue();
            GST_DEBUG("Frame %d captured with exposure: %.2fμs (from alt chunk)",
                     frame_counter, chunk_exp);
          } else {
            GST_DEBUG("No exposure time found in chunk data for frame %d", frame_counter);
          }
        }
      } catch (const Pylon::GenericException &chunk_err) {
        GST_DEBUG("Error reading chunk exposure: %s", chunk_err.GetDescription());
      }

    } else {
      GST_DEBUG("HDR Frame %d - No chunk data available", frame_counter);
      GST_WARNING("Chunks not available - enable with cam::ChunkModeActive=True");

      // Fallback: Try to estimate from sequencer if we know the pattern
      // This is less reliable but better than nothing
      GenApi::INodeMap &nodemap = self->camera->GetNodeMap();
      Pylon::CIntegerParameter seqSetIndex(nodemap, "SequencerSetActive");
      if (seqSetIndex.IsValid() && seqSetIndex.IsReadable()) {
        gint64 active_set = seqSetIndex.GetValue();
        GST_DEBUG("HDR Frame %d - Active sequencer set: %ld", frame_counter, active_set);
      }
    }

  } catch (const Pylon::GenericException &e) {
    // Don't fail on debug output errors
    GST_DEBUG("Could not read exposure time for debug: %s", e.GetDescription());
  }

  return TRUE;
}

static std::vector<std::string> gst_pylon_gst_to_pfnc(
    const std::string &gst_format,
    const std::vector<PixelFormatMappingType> &pixel_format_mapping) {
  std::vector<std::string> out_formats;
  for (auto &entry : pixel_format_mapping) {
    if (entry.gst_name == gst_format) {
      out_formats.push_back(entry.pfnc_name);
    }
  }
  return out_formats;
}

static std::vector<std::string> gst_pylon_pfnc_to_gst(
    const std::string &genapi_format,
    const std::vector<PixelFormatMappingType> &pixel_format_mapping) {
  std::vector<std::string> out_formats;
  for (auto &entry : pixel_format_mapping) {
    if (entry.pfnc_name == genapi_format) {
      out_formats.push_back(entry.gst_name);
    }
  }
  return out_formats;
}

static std::vector<std::string> gst_pylon_pfnc_list_to_gst(
    const GenApi::StringList_t &genapi_formats,
    const std::vector<PixelFormatMappingType> &pixel_format_mapping) {
  std::vector<std::string> formats_list;

  for (const auto &genapi_fmt : genapi_formats) {
    std::vector<std::string> gst_fmts =
        gst_pylon_pfnc_to_gst(std::string(genapi_fmt), pixel_format_mapping);

    /* Insert every matching gst format */
    formats_list.insert(formats_list.end(), gst_fmts.begin(), gst_fmts.end());
  }

  return formats_list;
}

typedef void (*GstPylonQuery)(GstPylon *, GValue *);

static void gst_pylon_query_format(
    GstPylon *self, GValue *outvalue,
    const std::vector<PixelFormatMappingType> &pixel_format_mapping) {
  g_return_if_fail(self);
  g_return_if_fail(outvalue);

  GenApi::INodeMap &nodemap = self->camera->GetNodeMap();
  Pylon::CEnumParameter pixelformat(nodemap, "PixelFormat");

  GenApi::StringList_t genapi_formats;
  pixelformat.GetSettableValues(genapi_formats);

  /* Convert GenApi formats to Gst formats */
  std::vector<std::string> gst_formats =
      gst_pylon_pfnc_list_to_gst(genapi_formats, pixel_format_mapping);

  /* Fill format field */
  g_value_init(outvalue, GST_TYPE_LIST);

  GValue value = G_VALUE_INIT;
  g_value_init(&value, G_TYPE_STRING);

  for (const auto &fmt : gst_formats) {
    g_value_set_string(&value, fmt.c_str());
    gst_value_list_append_value(outvalue, &value);
  }

  g_value_unset(&value);
}

static void gst_pylon_query_integer(GstPylon *self, GValue *outvalue,
                                    const std::string &name) {
  g_return_if_fail(self);
  g_return_if_fail(outvalue);

  GenApi::INodeMap &nodemap = self->camera->GetNodeMap();
  Pylon::CIntegerParameter param(nodemap, name.c_str());

  gint min = param.GetMin();
  gint max = param.GetMax();

  g_value_init(outvalue, GST_TYPE_INT_RANGE);
  gst_value_set_int_range(outvalue, min, max);
}

static void gst_pylon_query_width(GstPylon *self, GValue *outvalue) {
  g_return_if_fail(self);
  g_return_if_fail(outvalue);

  gst_pylon_query_integer(self, outvalue, "Width");
}

static void gst_pylon_query_height(GstPylon *self, GValue *outvalue) {
  g_return_if_fail(self);
  g_return_if_fail(outvalue);

  gst_pylon_query_integer(self, outvalue, "Height");
}

static void gst_pylon_query_framerate(GstPylon *self, GValue *outvalue) {
  g_return_if_fail(self);
  g_return_if_fail(outvalue);

  gdouble min_fps = 1;
  gdouble max_fps = 1;
  Pylon::CFloatParameter framerate;

  GenApi::INodeMap &nodemap = self->camera->GetNodeMap();

  if (self->camera->GetSfncVersion() >= Pylon::Sfnc_2_0_0) {
    framerate.Attach(nodemap, "AcquisitionFrameRate");
  } else {
    framerate.Attach(nodemap, "AcquisitionFrameRateAbs");
  }

  if (framerate.IsReadable()) {
    min_fps = framerate.GetMin();
    max_fps = framerate.GetMax();

    gint min_fps_num = 0;
    gint min_fps_den = 0;
    gst_util_double_to_fraction(min_fps, &min_fps_num, &min_fps_den);

    gint max_fps_num = 0;
    gint max_fps_den = 0;
    gst_util_double_to_fraction(max_fps, &max_fps_num, &max_fps_den);

    g_value_init(outvalue, GST_TYPE_FRACTION_RANGE);
    gst_value_set_fraction_range_full(outvalue, min_fps_num, min_fps_den,
                                      max_fps_num, max_fps_den);
  } else {
    /* Fallback framerate 0, if camera does not supply any value */
    g_value_init(outvalue, GST_TYPE_FRACTION);
    gst_value_set_fraction(outvalue, 0, 1);
    GST_INFO(
        "AcquisitionFramerate feature not available"
        " camera will report 0/1 as supported framerate");
  }
}

static void gst_pylon_query_caps(
    GstPylon *self, GstStructure *st,
    const std::vector<PixelFormatMappingType> &pixel_format_mapping) {
  g_return_if_fail(self);
  g_return_if_fail(st);

  GValue value = G_VALUE_INIT;

  /* Save offset to later reset values after querying */
  gint64 orig_offset_x = self->camera->OffsetX.GetValue();
  gint64 orig_offset_y = self->camera->OffsetY.GetValue();

  const std::vector<std::pair<GstPylonQuery, const std::string>> queries = {
      {gst_pylon_query_width, "width"},
      {gst_pylon_query_height, "height"},
      {gst_pylon_query_framerate, "framerate"}};

  /* Offsets are set to 0 to get the true image geometry */
  self->camera->OffsetX.TrySetToMinimum();
  self->camera->OffsetY.TrySetToMinimum();

  /* Pixel format is queried separately to support querying different pixel
   * format mappings */
  gst_pylon_query_format(self, &value, pixel_format_mapping);
  gst_structure_set_value(st, "format", &value);
  g_value_unset(&value);

  for (const auto &query : queries) {
    GstPylonQuery func = query.first;
    const gchar *name = query.second.c_str();

    func(self, &value);
    gst_structure_set_value(st, name, &value);
    g_value_unset(&value);
  }

  /* Reset offset after querying */
  self->camera->OffsetX.TrySetValue(orig_offset_x);
  self->camera->OffsetY.TrySetValue(orig_offset_y);
}

GstCaps *gst_pylon_query_configuration(GstPylon *self, GError **err) {
  g_return_val_if_fail(self, NULL);
  g_return_val_if_fail(err && *err == NULL, NULL);

  /* Build gst caps */
  GstCaps *caps = gst_caps_new_empty();

  for (const auto &gst_structure_format : gst_structure_formats) {
    GstStructure *st =
        gst_structure_new_empty(gst_structure_format.st_name.c_str());
    try {
      gst_pylon_query_caps(self, st, gst_structure_format.format_map);
      gst_caps_append_structure(caps, st);

#ifdef NVMM_ENABLED
      /* We need the copy since the append has taken ownership of the "old" st
       */
      gst_caps_append_structure_full(
          caps, gst_structure_copy(st),
          gst_caps_features_new("memory:NVMM", NULL));
#endif

    } catch (const Pylon::GenericException &e) {
      gst_structure_free(st);
      gst_caps_unref(caps);

      g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
                  e.GetDescription());
      return NULL;
    }
  }

  return caps;
}

gboolean gst_pylon_set_configuration(GstPylon *self, const GstCaps *conf,
                                     GError **err) {
  g_return_val_if_fail(self, FALSE);
  g_return_val_if_fail(conf, FALSE);
  g_return_val_if_fail(err && *err == NULL, FALSE);

  GstStructure *st = gst_caps_get_structure(conf, 0);

  GenApi::INodeMap &nodemap = self->camera->GetNodeMap();
  Pylon::CEnumParameter pixelformat(nodemap, "PixelFormat");

  try {
    const std::string gst_format = gst_structure_get_string(st, "format");
    if (gst_format.empty()) {
      throw Pylon::GenericException(
          "Unable to find the format in the configuration", __FILE__, __LINE__);
    }

    gint gst_width = 0;
    if (!gst_structure_get_int(st, "width", &gst_width)) {
      throw Pylon::GenericException(
          "Unable to find the width in the configuration", __FILE__, __LINE__);
    }

    gint gst_height = 0;
    if (!gst_structure_get_int(st, "height", &gst_height)) {
      throw Pylon::GenericException(
          "Unable to find the height in the configuration", __FILE__, __LINE__);
    }

    gint gst_numerator = 0;
    gint gst_denominator = 0;
    if (!gst_structure_get_fraction(st, "framerate", &gst_numerator,
                                    &gst_denominator)) {
      throw Pylon::GenericException(
          "Unable to find the framerate in the configuration", __FILE__,
          __LINE__);
    }

    bool fmt_valid = false;
    for (const auto &gst_structure_format : gst_structure_formats) {
      const std::vector<std::string> pfnc_formats =
          gst_pylon_gst_to_pfnc(gst_format, gst_structure_format.format_map);

      /* In case of ambiguous format mapping choose first */
      for (auto &fmt : pfnc_formats) {
        fmt_valid = pixelformat.TrySetValue(fmt.c_str());
        if (fmt_valid) break;
      }
    }

    if (!fmt_valid) {
      throw Pylon::GenericException(
          std::string("Unsupported GStreamer format: " + gst_format).c_str(),
          __FILE__, __LINE__);
    }

    Pylon::CIntegerParameter width(nodemap, "Width");
    width.SetValue(gst_width, Pylon::IntegerValueCorrection_None);
    GST_INFO("Set Feature Width: %d", gst_width);

    Pylon::CIntegerParameter height(nodemap, "Height");
    height.SetValue(gst_height, Pylon::IntegerValueCorrection_None);
    GST_INFO("Set Feature Height: %d", gst_height);

    /* set the cached offsetx/y values
     * respect the rounding value adjustment rules
     * -> offset will be adjusted to keep dimensions
     */

    GstPylonObjectPrivate *cam_properties =
        (GstPylonObjectPrivate *)gst_pylon_object_get_instance_private(
            reinterpret_cast<GstPylonObject *>(self->gcamera));

    auto &offsetx_cache = cam_properties->dimension_cache.offsetx;
    auto &offsety_cache = cam_properties->dimension_cache.offsety;
    auto enable_correction = cam_properties->enable_correction;

    bool value_corrected = false;
    if (offsetx_cache >= 0) {
      Pylon::CIntegerParameter offsetx(nodemap, "OffsetX");
      if (enable_correction) {
        try {
          offsetx.SetValue(
              offsetx_cache,
              Pylon::EIntegerValueCorrection::IntegerValueCorrection_None);
        } catch (GenICam::OutOfRangeException &) {
          offsetx.SetValue(
              offsetx_cache,
              Pylon::EIntegerValueCorrection::IntegerValueCorrection_Nearest);
          value_corrected = true;
        }
      } else {
        offsetx.SetValue(offsetx_cache);
      }
      GST_INFO("Set Feature OffsetX: %d %s",
               static_cast<gint>(offsetx.GetValue()),
               value_corrected ? " [corrected]" : "");
      offsetx_cache = -1;
    }

    value_corrected = false;
    if (offsety_cache >= 0) {
      Pylon::CIntegerParameter offsety(nodemap, "OffsetY");
      if (enable_correction) {
        try {
          offsety.SetValue(
              offsety_cache,
              Pylon::EIntegerValueCorrection::IntegerValueCorrection_None);
        } catch (GenICam::OutOfRangeException &) {
          offsety.SetValue(
              offsety_cache,
              Pylon::EIntegerValueCorrection::IntegerValueCorrection_Nearest);
          value_corrected = true;
        }
      } else {
        offsety.SetValue(offsety_cache);
      }
      GST_INFO("Set Feature Offsety: %d %s",
               static_cast<gint>(offsety.GetValue()),
               value_corrected ? " [corrected]" : "");
      offsety_cache = -1;
    }

    Pylon::CBooleanParameter framerate_enable(nodemap,
                                              "AcquisitionFrameRateEnable");

    /* Basler dart gen1 models have no framerate_enable feature */
    framerate_enable.TrySetValue(true);

    gdouble div = 1.0 * gst_numerator / gst_denominator;
    if (self->camera->GetSfncVersion() >= Pylon::Sfnc_2_0_0) {
      Pylon::CFloatParameter framerate(nodemap, "AcquisitionFrameRate");
      framerate.TrySetValue(div, Pylon::FloatValueCorrection_None);
      GST_INFO("Set Feature AcquisitionFrameRate: %f", div);
    } else {
      Pylon::CFloatParameter framerate(nodemap, "AcquisitionFrameRateAbs");
      framerate.TrySetValue(div, Pylon::FloatValueCorrection_None);
      GST_INFO("Set Feature AcquisitionFrameRateAbs: %f", div);
    }
  } catch (const Pylon::GenericException &e) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED, "%s",
                e.GetDescription());
    return FALSE;
  }

  guint64 maxnumbuffers = 0;
  g_object_get(self->gstream_grabber, "MaxNumBuffer", &maxnumbuffers, nullptr);
  self->camera->MaxNumBuffer.TrySetValue(maxnumbuffers);

#ifdef NVMM_ENABLED
  GstCapsFeatures *features = gst_caps_get_features(conf, 0);
  if (gst_caps_features_contains(features, "memory:NVMM")) {
    self->buffer_factory = std::make_shared<GstPylonDsNvmmBufferFactory>(
        self->nvsurface_layout, self->gpu_id);

    self->buffer_factory->SetConfig(conf);
    self->mem_type = MEM_NVMM;
  } else {
#endif
    self->buffer_factory = std::make_shared<GstPylonSysMemBufferFactory>();
    self->mem_type = MEM_SYSMEM;
#ifdef NVMM_ENABLED
  }
#endif

  self->camera->SetBufferFactory(self->buffer_factory.get(),
                                 Pylon::Cleanup_None);

  return TRUE;
}

static void gst_pylon_append_properties(
    Pylon::CBaslerUniversalInstantCamera *camera,
    const std::string &device_full_name, const std::string &device_type_str,
    GstPylonCache &feature_cache, GenApi::INodeMap &nodemap,
    gchar **device_properties, guint alignment) {
  g_return_if_fail(camera);
  g_return_if_fail(device_properties);

  GType device_type =
      gst_pylon_object_register(device_full_name, feature_cache, nodemap);
  GObject *device_obj = G_OBJECT(g_object_new(device_type, NULL));

  gchar *device_name = g_strdup_printf(
      "%*s %s:\n", alignment, camera->GetDeviceInfo().GetFriendlyName().c_str(),
      device_type_str.c_str());

  gchar *properties = gst_child_inspector_properties_to_string(
      device_obj, alignment, device_name);

  if (NULL == *device_properties) {
    *device_properties = g_strdup(properties);
  } else {
    *device_properties =
        g_strconcat(*device_properties, "\n", properties, NULL);
  }

  g_free(device_name);
  g_free(properties);

  g_object_unref(device_obj);
}

static void gst_pylon_append_camera_properties(
    Pylon::CBaslerUniversalInstantCamera *camera, gchar **camera_properties,
    guint alignment) {
  g_return_if_fail(camera);
  g_return_if_fail(camera_properties);

  GenApi::INodeMap &nodemap = camera->GetNodeMap();
  std::string camera_name = gst_pylon_get_camera_fullname(*camera);
  std::string device_type = "Camera";
  std::string cache_filename =
      std::string(camera->DeviceModelName.GetValue() + "_" +
                  camera->DeviceFirmwareVersion.GetValue() + "_" + VERSION);

  GstPylonCache feature_cache(cache_filename);

  gst_pylon_append_properties(camera, camera_name, device_type, feature_cache,
                              nodemap, camera_properties, alignment);
}

static void gst_pylon_append_stream_grabber_properties(
    Pylon::CBaslerUniversalInstantCamera *camera, gchar **sgrabber_properties,
    guint alignment) {
  g_return_if_fail(camera);
  g_return_if_fail(sgrabber_properties);

  GenApi::INodeMap &nodemap = camera->GetStreamGrabberNodeMap();
  std::string sgrabber_name = gst_pylon_get_sgrabber_name(*camera);
  std::string device_type = "Stream Grabber";
  std::string cache_filename =
      std::string(camera->GetDeviceInfo().GetModelName() + "_" +
                  Pylon::GetPylonVersionString() + "_" + VERSION);

  GstPylonCache feature_cache(cache_filename);

  gst_pylon_append_properties(camera, sgrabber_name, device_type, feature_cache,
                              nodemap, sgrabber_properties, alignment);
}

static gchar *gst_pylon_get_string_properties(
    GetStringProperties get_device_string_properties) {
  gchar *camera_properties = NULL;

  Pylon::CTlFactory &factory = Pylon::CTlFactory::GetInstance();
  Pylon::DeviceInfoList_t device_list;

  factory.EnumerateDevices(device_list);

  for (const auto &device : device_list) {
    try {
      Pylon::CBaslerUniversalInstantCamera camera(factory.CreateDevice(device),
                                                  Pylon::Cleanup_Delete);
      camera.Open();

      /* Set the camera to a valid state
       * close left open transactions on the device
       */
      camera.DeviceFeaturePersistenceEnd.TryExecute();
      camera.DeviceRegistersStreamingEnd.TryExecute();

      /* Set the camera to a valid state
       * load the factory default set
       */
      if (camera.UserSetSelector.IsWritable()) {
        camera.UserSetSelector.SetValue("Default");
        camera.UserSetLoad.Execute();
      }

      get_device_string_properties(&camera, &camera_properties,
                                   DEFAULT_ALIGNMENT);
      camera.Close();
    } catch (const Pylon::GenericException &) {
      continue;
    }
  }

  return camera_properties;
}

gchar *gst_pylon_camera_get_string_properties() {
  return gst_pylon_get_string_properties(gst_pylon_append_camera_properties);
}

gchar *gst_pylon_stream_grabber_get_string_properties() {
  return gst_pylon_get_string_properties(
      gst_pylon_append_stream_grabber_properties);
}

GObject *gst_pylon_get_camera(GstPylon *self) {
  g_return_val_if_fail(self, NULL);

  return G_OBJECT(g_object_ref(self->gcamera));
}

GObject *gst_pylon_get_stream_grabber(GstPylon *self) {
  g_return_val_if_fail(self, NULL);

  return G_OBJECT(g_object_ref(self->gstream_grabber));
}

gboolean gst_pylon_is_same_device(GstPylon *self, const gint device_index,
                                  const gchar *device_user_name,
                                  const gchar *device_serial_number) {
  g_return_val_if_fail(self, FALSE);

  std::string user_name = device_user_name ? device_user_name : "";
  std::string serial_number = device_serial_number ? device_serial_number : "";

  return self->requested_device_index == device_index &&
         self->requested_device_user_name == user_name &&
         self->requested_device_serial_number == serial_number;
}

gboolean gst_pylon_configure_line2(GstPylon *self, gboolean illumination,
                                   GError **err) {
  g_return_val_if_fail(self, FALSE);
  g_return_val_if_fail(err && *err == NULL, FALSE);

  try {
    auto &camera = self->camera;
    if (!camera || !camera->IsOpen()) {
      g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                  "Camera is not open");
      return FALSE;
    }

    // Select Line2
    if (GenApi::IsWritable(camera->LineSelector)) {
      camera->LineSelector.SetValue(Basler_UniversalCameraParams::LineSelector_Line2);
      GST_INFO("Line2 selected");
    } else {
      g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                  "LineSelector is not writable");
      return FALSE;
    }

    if (illumination) {
      // Configure for illumination: Output mode with ExposureActive
      if (GenApi::IsWritable(camera->LineMode)) {
        camera->LineMode.SetValue(Basler_UniversalCameraParams::LineMode_Output);
        GST_INFO("Line2 configured as Output");
      } else {
        g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                    "LineMode is not writable");
        return FALSE;
      }

      if (GenApi::IsWritable(camera->LineSource)) {
        camera->LineSource.SetValue(Basler_UniversalCameraParams::LineSource_ExposureActive);
        GST_INFO("Line2 source set to ExposureActive");
      } else {
        g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                    "LineSource is not writable");
        return FALSE;
      }
    } else {
      // Configure as input with LineSource Off
      if (GenApi::IsWritable(camera->LineMode)) {
        camera->LineMode.SetValue(Basler_UniversalCameraParams::LineMode_Input);
        GST_INFO("Line2 configured as Input");
      } else {
        g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                    "LineMode is not writable");
        return FALSE;
      }

      if (GenApi::IsWritable(camera->LineSource)) {
        camera->LineSource.SetValue(Basler_UniversalCameraParams::LineSource_Off);
        GST_INFO("Line2 source set to Off");
      } else {
        g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                    "LineSource is not writable");
        return FALSE;
      }
    }

    // Now configure Line3
    if (GenApi::IsWritable(camera->LineSelector)) {
      camera->LineSelector.SetValue(Basler_UniversalCameraParams::LineSelector_Line3);
      GST_INFO("Line3 selected");
    } else {
      g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                  "LineSelector is not writable for Line3");
      return FALSE;
    }

    // Line3 is always Output mode with Counter1Active source
    if (GenApi::IsWritable(camera->LineMode)) {
      camera->LineMode.SetValue(Basler_UniversalCameraParams::LineMode_Output);
      GST_INFO("Line3 configured as Output");
    } else {
      g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                  "LineMode is not writable for Line3");
      return FALSE;
    }

    if (GenApi::IsWritable(camera->LineSource)) {
      camera->LineSource.SetValue(Basler_UniversalCameraParams::LineSource_Counter1Active);
      GST_INFO("Line3 source set to Counter1Active");
    } else {
      g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                  "LineSource is not writable for Line3");
      return FALSE;
    }

    // Configure Line3 inverter based on illumination
    if (GenApi::IsWritable(camera->LineInverter)) {
      camera->LineInverter.SetValue(illumination);  // true when illumination=true, false when illumination=false
      GST_INFO("Line3 inverter set to %s", illumination ? "true" : "false");
    } else {
      g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                  "LineInverter is not writable for Line3");
      return FALSE;
    }

    return TRUE;
  } catch (const Pylon::GenericException &e) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                "Pylon error configuring illumination lines: %s", e.GetDescription());
    return FALSE;
  }
}

gdouble gst_pylon_get_device_temperature(GstPylon *self, GError **err) {
  g_return_val_if_fail(self, -273.15);
  g_return_val_if_fail(err && *err == NULL, -273.15);

  try {
    auto &camera = self->camera;
    if (!camera || !camera->IsOpen()) {
      g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                  "Camera is not open");
      return -273.15;
    }

    if (GenApi::IsReadable(camera->DeviceTemperature)) {
      gdouble temperature = camera->DeviceTemperature.GetValue();
      GST_DEBUG("Device temperature: %.2f C", temperature);
      return temperature;
    } else {
      g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                  "DeviceTemperature is not readable");
      return -273.15;
    }
  } catch (const Pylon::GenericException &e) {
    g_set_error(err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
                "Pylon error reading device temperature: %s", e.GetDescription());
    return -273.15;
  }
}

#ifdef NVMM_ENABLED
void gst_pylon_set_nvsurface_layout(
    GstPylon *self, const GstPylonNvsurfaceLayoutEnum nvsurface_layout) {
  g_return_if_fail(self);

  self->nvsurface_layout = nvsurface_layout;
}

GstPylonNvsurfaceLayoutEnum gst_pylon_get_nvsurface_layout(GstPylon *self) {
  g_return_val_if_fail(self, PROP_NVSURFACE_LAYOUT_DEFAULT);

  return self->nvsurface_layout;
}

void gst_pylon_set_gpu_id(GstPylon *self, const gint gpu_id) {
  g_return_if_fail(self);

  self->gpu_id = gpu_id;
}

guint gst_pylon_get_gpu_id(GstPylon *self) {
  g_return_val_if_fail(self, PROP_GPU_ID_DEFAULT);

  return self->gpu_id;
}
#endif
