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

/**
 * SECTION:element-gstpylonsrc
 *
 * The pylonsrc element captures images from Basler cameras.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 -v pylonsrc ! videoconvert ! autovideosink
 * ]|
 * Capture images from a Basler camera and display them.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gst/pylon/gstpylondebug.h"
#include "gst/pylon/gstpylonmeta.h"
#include "gstpylon.h"
#include "gstpylonsrc.h"
#include "HdrMetadataPlugin.h"
#include "HdrProfileSwitcher.h"
#include "gsthdrmeta.h"

#include <gst/pylon/gstpylonincludes.h>
#include <gst/video/video.h>

struct _GstPylonSrc {
  GstPushSrc base_pylonsrc;
  GstPylon *pylon;
  GstClockTime duration;
  GstVideoInfo video_info;

  gchar *device_user_name;
  gchar *device_serial_number;
  gint device_index;
  gchar *user_set;
  gchar *pfs_location;
  gboolean enable_correction;
  GstPylonCaptureErrorEnum capture_error;
  gchar *hdr_sequence;
  gchar *hdr_sequence2;
  gint hdr_profile;
  HdrMetadataPlugin *hdr_plugin;
  HdrProfileSwitcher *hdr_switcher;
  GObject *cam;
  GObject *stream;
  gboolean illumination;
  gint sensor_offset_x;
  gint sensor_offset_y;

#ifdef NVMM_ENABLED
  GstPylonNvsurfaceLayoutEnum nvsurface_layout;
  guint gpu_id;
#endif
};

/* prototypes */

static void gst_pylon_src_set_property(GObject *object, guint property_id,
                                       const GValue *value, GParamSpec *pspec);
static void gst_pylon_src_get_property(GObject *object, guint property_id,
                                       GValue *value, GParamSpec *pspec);
static void gst_pylon_src_finalize(GObject *object);

static GstCaps *gst_pylon_src_get_caps(GstBaseSrc *src, GstCaps *filter);
static gboolean gst_pylon_src_is_bayer(GstStructure *st);
static GstCaps *gst_pylon_src_fixate(GstBaseSrc *src, GstCaps *caps);
static gboolean gst_pylon_src_set_caps(GstBaseSrc *src, GstCaps *caps);
static gboolean gst_pylon_src_decide_allocation(GstBaseSrc *src,
                                                GstQuery *query);
static gboolean gst_pylon_src_start(GstBaseSrc *src);
static gboolean gst_pylon_src_stop(GstBaseSrc *src);
static gboolean gst_pylon_src_unlock(GstBaseSrc *src);
static gboolean gst_pylon_src_query(GstBaseSrc *src, GstQuery *query);
static void gst_plyon_src_add_metadata(GstPylonSrc *self, GstBuffer *buf);
static GstFlowReturn gst_pylon_src_create(GstPushSrc *src, GstBuffer **buf);
static void gst_pylon_src_enable_hdr_chunks(GstPylonSrc *self);

static void gst_pylon_src_child_proxy_init(GstChildProxyInterface *iface);

enum {
  PROP_0,
  PROP_DEVICE_USER_NAME,
  PROP_DEVICE_SERIAL_NUMBER,
  PROP_DEVICE_INDEX,
  PROP_USER_SET,
  PROP_PFS_LOCATION,
  PROP_ENABLE_CORRECTION,
  PROP_CAPTURE_ERROR,
  PROP_HDR_SEQUENCE,
  PROP_HDR_SEQUENCE2,
  PROP_HDR_PROFILE,
  PROP_CAM,
  PROP_STREAM,
  PROP_ILLUMINATION,
  PROP_DEVICE_TEMPERATURE,
  PROP_SENSOR_OFFSET_X,
  PROP_SENSOR_OFFSET_Y,
#ifdef NVMM_ENABLED
  PROP_NVSURFACE_LAYOUT,
  PROP_GPU_ID,
#endif
};

#define PROP_DEVICE_USER_NAME_DEFAULT NULL
#define PROP_DEVICE_SERIAL_NUMBER_DEFAULT NULL
#define PROP_DEVICE_INDEX_DEFAULT -1
#define PROP_DEVICE_INDEX_MIN -1
#define PROP_DEVICE_INDEX_MAX G_MAXINT32
#define PROP_USER_SET_DEFAULT NULL
#define PROP_PFS_LOCATION_DEFAULT NULL
#define PROP_ENABLE_CORRECTION_DEFAULT TRUE
#define PROP_HDR_SEQUENCE_DEFAULT NULL
#define PROP_HDR_SEQUENCE2_DEFAULT NULL
#define PROP_HDR_PROFILE_DEFAULT 0
#define PROP_CAM_DEFAULT NULL
#define PROP_STREAM_DEFAULT NULL
#define PROP_CAPTURE_ERROR_DEFAULT ENUM_ABORT
#define PROP_ILLUMINATION_DEFAULT FALSE
#define PROP_SENSOR_OFFSET_X_DEFAULT 0
#define PROP_SENSOR_OFFSET_Y_DEFAULT 0
#ifdef NVMM_ENABLED
#  define PROP_GPU_ID_MIN 0
#  define PROP_GPU_ID_MAX G_MAXUINT32
#endif

/* Enum for cature_error */
#define GST_TYPE_CAPTURE_ERROR_ENUM (gst_pylon_capture_error_enum_get_type())

/* Child proxy interface names */
static const gchar *gst_pylon_src_child_proxy_names[] = {"cam", "stream"};

static GType gst_pylon_capture_error_enum_get_type(void) {
  static gsize gtype = 0;
  static const GEnumValue values[] = {
      {ENUM_KEEP, "keep", "Use partial or corrupt buffers"},
      {ENUM_SKIP, "skip",
       "Skip partial or corrupt buffers. A maximum of 100 buffers can be "
       "skipped before the pipeline aborts."},
      {ENUM_ABORT, "abort", "Stop pipeline in case of any capture error"},
      {0, NULL, NULL}};

  if (g_once_init_enter(&gtype)) {
    GType tmp = g_enum_register_static("GstPylonCaptureErrorEnum", values);
    g_once_init_leave(&gtype, tmp);
  }

  return (GType)gtype;
}

#ifdef NVMM_ENABLED
#  define GST_TYPE_NVSURFACE_LAYOUT_ENUM \
    (gst_pylon_nvsurface_layout_enum_get_type())

static GType gst_pylon_nvsurface_layout_enum_get_type(void) {
  static gsize gtype = 0;
  static const GEnumValue values[] = {
      {ENUM_BLOCK_LINEAR, "block-linear", "Specifies block linear layout."},
      {ENUM_PITCH, "pitch", "Specifies pitch layout."},
      {0, NULL, NULL}};

  if (g_once_init_enter(&gtype)) {
    GType tmp = g_enum_register_static("GstPylonNvsurfaceLayoutEnum", values);
    g_once_init_leave(&gtype, tmp);
  }

  return (GType)gtype;
}
#endif

/* pad templates */
// clang-format off
#ifdef NVMM_ENABLED
#  define NVMM_GST_VIDEO_CAPS                  ";"       \
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES("memory:NVMM", \
                                          " {GRAY8, RGB, BGR, YUY2, UYVY} ")
#else
#  define NVMM_GST_VIDEO_CAPS
#endif

 static GstStaticPadTemplate gst_pylon_src_src_template =
    GST_STATIC_PAD_TEMPLATE(
        "src", GST_PAD_SRC, GST_PAD_ALWAYS,
        GST_STATIC_CAPS(GST_VIDEO_CAPS_MAKE(
            " {GRAY8, RGB, BGR, YUY2, UYVY} ") ";"
                                               "video/"
                                               "x-bayer,format={rggb,bggr,gbgr,"
                                               "grgb},"
                                               "width=" GST_VIDEO_SIZE_RANGE
                                               ",height=" GST_VIDEO_SIZE_RANGE
                                               ",framerate"
                                               "=" GST_VIDEO_FPS_RANGE
                                               NVMM_GST_VIDEO_CAPS
         )
    );
// clang-format on

/* class initialization */
G_DEFINE_TYPE_WITH_CODE(GstPylonSrc, gst_pylon_src, GST_TYPE_PUSH_SRC,
                        gst_pylon_debug_init();
                        G_IMPLEMENT_INTERFACE(GST_TYPE_CHILD_PROXY,
                                              gst_pylon_src_child_proxy_init));

static void gst_pylon_src_class_init(GstPylonSrcClass *klass) {
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
  GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS(klass);
  GstPushSrcClass *push_src_class = GST_PUSH_SRC_CLASS(klass);
  gchar *cam_params = NULL;
  gchar *cam_blurb = NULL;
  gchar *stream_params = NULL;
  gchar *stream_blurb = NULL;
  const gchar *cam_prolog = NULL;
  const gchar *stream_prolog = NULL;

  Pylon::PylonAutoInitTerm init_pylon;

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_static_pad_template(GST_ELEMENT_CLASS(klass),
                                            &gst_pylon_src_src_template);

  gst_element_class_set_static_metadata(
      GST_ELEMENT_CLASS(klass), "Basler/Pylon source element",
      "Source/Video/Hardware", "Source element for Basler cameras",
      "Basler AG <support.europe@baslerweb.com>");

  gobject_class->set_property = gst_pylon_src_set_property;
  gobject_class->get_property = gst_pylon_src_get_property;
  gobject_class->finalize = gst_pylon_src_finalize;

  g_object_class_install_property(
      gobject_class, PROP_DEVICE_USER_NAME,
      g_param_spec_string(
          "device-user-name", "Device user defined name",
          "The user-defined name of the device to use. May be combined"
          "with other device selection properties to reduce the search.",
          PROP_DEVICE_USER_NAME_DEFAULT,
          static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                                   GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property(
      gobject_class, PROP_DEVICE_SERIAL_NUMBER,
      g_param_spec_string(
          "device-serial-number", "Device serial number",
          "The serial number of the device to use. May be combined with "
          "other device selection properties to reduce the search.",
          PROP_DEVICE_SERIAL_NUMBER_DEFAULT,
          static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                                   GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property(
      gobject_class, PROP_DEVICE_INDEX,
      g_param_spec_int(
          "device-index", "Device index",
          "The index of the device to use.This index applies to the "
          "resulting device list after applying the other device selection "
          "properties. The index is mandatory if multiple devices match "
          "the given search criteria.",
          PROP_DEVICE_INDEX_MIN, PROP_DEVICE_INDEX_MAX,
          PROP_DEVICE_INDEX_DEFAULT,
          static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                                   GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property(
      gobject_class, PROP_USER_SET,
      g_param_spec_string(
          "user-set", "Device user configuration set",
          "The user-defined configuration set to use. Leaving this property "
          "unset, or using 'Auto' result in selecting the "
          "power-on default camera configuration.",
          PROP_USER_SET_DEFAULT,
          static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                                   GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property(
      gobject_class, PROP_PFS_LOCATION,
      g_param_spec_string(
          "pfs-location", "PFS file location",
          "The filepath to the PFS file from which to load the device "
          "configuration. "
          "Setting this property will override the user set property if also "
          "set.",
          PROP_PFS_LOCATION_DEFAULT,
          static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                                   GST_PARAM_MUTABLE_READY)));

  g_object_class_install_property(
      gobject_class, PROP_ENABLE_CORRECTION,
      g_param_spec_boolean(
          "enable-correction", "Enable correction",
          "If enabled, the values from other parameters will be automatically "
          "corrected. "
          " If any of the properties holds an incorrect value given an "
          "specific configuration "
          "it will be corrected",
          PROP_ENABLE_CORRECTION_DEFAULT,
          static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                                   GST_PARAM_MUTABLE_READY)));

  g_object_class_install_property(
      gobject_class, PROP_CAPTURE_ERROR,
      g_param_spec_enum(
          "capture-error", "Capture error strategy",
          "The strategy to use in case of a camera capture error.",
          GST_TYPE_CAPTURE_ERROR_ENUM, PROP_CAPTURE_ERROR_DEFAULT,
          static_cast<GParamFlags>(G_PARAM_READWRITE |
                                   GST_PARAM_CONTROLLABLE)));

  g_object_class_install_property(
      gobject_class, PROP_HDR_SEQUENCE,
      g_param_spec_string(
          "hdr-sequence", "HDR Exposure Sequence (Profile 0)",
          "Comma-separated list of exposure:gain pairs for HDR sequence mode Profile 0. "
          "Format: 'exposure1:gain1,exposure2:gain2' where exposure is in microseconds and gain is a float value. "
          "Gain is optional and defaults to 0 if not specified. "
          "Examples: '19:1.2,150:2.5' (with gains), '19,150' (gains default to 0), '19:1.2,150' (mixed). "
          "Setting this property will automatically configure the camera's sequencer mode. "
          "Each exposure:gain pair will be assigned to a sequencer set, cycling through them continuously. "
          "Leave empty to disable sequencer mode.",
          PROP_HDR_SEQUENCE_DEFAULT,
          static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                                   GST_PARAM_MUTABLE_READY)));

  g_object_class_install_property(
      gobject_class, PROP_HDR_SEQUENCE2,
      g_param_spec_string(
          "hdr-sequence2", "HDR Exposure Sequence (Profile 1)",
          "Comma-separated list of exposure:gain pairs for HDR sequence mode Profile 1. "
          "Format: 'exposure1:gain1,exposure2:gain2' where exposure is in microseconds and gain is a float value. "
          "Gain is optional and defaults to 0 if not specified. "
          "Examples: '5000:2.5,10000:3.0' (with gains), '5000,10000' (gains default to 0). "
          "When both hdr-sequence and hdr-sequence2 are set, dual profile mode is enabled "
          "allowing runtime switching between profiles via the hdr-profile property. "
          "Leave empty to use single profile mode.",
          PROP_HDR_SEQUENCE2_DEFAULT,
          static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                                   GST_PARAM_MUTABLE_READY)));

  g_object_class_install_property(
      gobject_class, PROP_HDR_PROFILE,
      g_param_spec_int(
          "hdr-profile", "Active HDR Profile",
          "HDR profile to switch to (0 or 1). Set to trigger a profile switch via software signal. "
          "Get returns the currently active profile based on actual frames (-1 if not configured).",
          -1, 1, PROP_HDR_PROFILE_DEFAULT,
          static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property(
      gobject_class, PROP_ILLUMINATION,
      g_param_spec_boolean(
          "illumination", "Illumination Control",
          "Enable external illumination control via Line2 and Line3. "
          "When true: Line2=Output+ExposureActive, Line3=Output+Counter1Active+Inverted. "
          "When false: Line2=Input+Off, Line3=Output+Counter1Active+NotInverted.",
          PROP_ILLUMINATION_DEFAULT,
          static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                                   GST_PARAM_MUTABLE_READY)));

  g_object_class_install_property(
      gobject_class, PROP_DEVICE_TEMPERATURE,
      g_param_spec_double(
          "device-temperature", "Device Temperature",
          "Current camera device temperature in degrees Celsius (read-only). "
          "Returns -273.15 if the camera is not open or temperature is not readable.",
          -273.15, 200.0, 0.0,
          static_cast<GParamFlags>(G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property(
      gobject_class, PROP_SENSOR_OFFSET_X,
      g_param_spec_int(
          "sensor-offset-x", "Sensor Offset X",
          "X offset from sensor origin in pixels. Applied to all HDR sequencer sets. "
          "Use this to grab image from a different horizontal position on the sensor.",
          0, G_MAXINT32, PROP_SENSOR_OFFSET_X_DEFAULT,
          static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                                   GST_PARAM_MUTABLE_READY)));

  g_object_class_install_property(
      gobject_class, PROP_SENSOR_OFFSET_Y,
      g_param_spec_int(
          "sensor-offset-y", "Sensor Offset Y",
          "Y offset from sensor origin in pixels. Applied to all HDR sequencer sets. "
          "Use this to grab image from a different vertical position on the sensor.",
          0, G_MAXINT32, PROP_SENSOR_OFFSET_Y_DEFAULT,
          static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                                   GST_PARAM_MUTABLE_READY)));

#ifdef NVMM_ENABLED
  g_object_class_install_property(
      gobject_class, PROP_NVSURFACE_LAYOUT,
      g_param_spec_enum("nvsurface-layout", "Surface layout",
                        "Surface layout. May be block-linear or pitch-linear. "
                        "For a dGPU, only pitch-linear is valid.",
                        GST_TYPE_NVSURFACE_LAYOUT_ENUM,
                        PROP_NVSURFACE_LAYOUT_DEFAULT,
                        static_cast<GParamFlags>(G_PARAM_READWRITE |
                                                 GST_PARAM_CONTROLLABLE)));

  g_object_class_install_property(
      gobject_class, PROP_GPU_ID,
      g_param_spec_uint(
          "gpu-id", "GPU ID",
          "Holds the GPU ID. Valid only for a multi-GPU system.",
          PROP_GPU_ID_MIN, PROP_GPU_ID_MAX, PROP_GPU_ID_DEFAULT,
          static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                                   GST_PARAM_MUTABLE_READY)));
#endif

  cam_params = gst_pylon_camera_get_string_properties();
  stream_params = gst_pylon_stream_grabber_get_string_properties();

  if (NULL == cam_params) {
    cam_prolog = "No valid cameras where found connected to the system.";
    stream_prolog = cam_prolog;
    cam_params = g_strdup("");
    stream_params = g_strdup("");
  } else {
    cam_prolog = "The following list details the properties for each camera.\n";
    stream_prolog =
        "The following list details the properties for each stream grabber.\n";
  }

  cam_blurb = g_strdup_printf(
      "The camera to use.\n"
      "\t\t\tAccording to the selected camera "
      "different properties will be available.\n "
      "\t\t\tThese properties can be accessed using the "
      "\"cam::<property>\" syntax.\n"
      "\t\t\t%s%s",
      cam_prolog, cam_params);

  g_object_class_install_property(
      gobject_class, PROP_CAM,
      g_param_spec_object("cam", "Camera", cam_blurb, G_TYPE_OBJECT,
                          G_PARAM_READABLE));

  stream_blurb = g_strdup_printf(
      "The stream grabber to use.\n"
      "\t\t\tAccording to the selected stream grabber "
      "different properties will be available.\n "
      "\t\t\tThese properties can be accessed using the "
      "\"stream::<property>\" syntax.\n"
      "\t\t\t%s%s",
      stream_prolog, stream_params);

  g_object_class_install_property(
      gobject_class, PROP_STREAM,
      g_param_spec_object("stream", "Stream Grabber", stream_blurb,
                          G_TYPE_OBJECT, G_PARAM_READABLE));

  g_free(cam_params);
  g_free(stream_params);

  base_src_class->get_caps = GST_DEBUG_FUNCPTR(gst_pylon_src_get_caps);
  base_src_class->fixate = GST_DEBUG_FUNCPTR(gst_pylon_src_fixate);
  base_src_class->set_caps = GST_DEBUG_FUNCPTR(gst_pylon_src_set_caps);
  base_src_class->decide_allocation =
      GST_DEBUG_FUNCPTR(gst_pylon_src_decide_allocation);
  base_src_class->start = GST_DEBUG_FUNCPTR(gst_pylon_src_start);
  base_src_class->stop = GST_DEBUG_FUNCPTR(gst_pylon_src_stop);
  base_src_class->unlock = GST_DEBUG_FUNCPTR(gst_pylon_src_unlock);
  base_src_class->query = GST_DEBUG_FUNCPTR(gst_pylon_src_query);
  push_src_class->create = GST_DEBUG_FUNCPTR(gst_pylon_src_create);
}

static void gst_pylon_src_init(GstPylonSrc *self) {
  GstBaseSrc *base = GST_BASE_SRC(self);

  self->pylon = NULL;
  self->duration = GST_CLOCK_TIME_NONE;
  self->device_user_name = PROP_DEVICE_USER_NAME_DEFAULT;
  self->device_serial_number = PROP_DEVICE_SERIAL_NUMBER_DEFAULT;
  self->device_index = PROP_DEVICE_INDEX_DEFAULT;
  self->user_set = PROP_USER_SET_DEFAULT;
  self->pfs_location = PROP_PFS_LOCATION_DEFAULT;
  self->enable_correction = PROP_ENABLE_CORRECTION_DEFAULT;
  self->capture_error = PROP_CAPTURE_ERROR_DEFAULT;
  self->hdr_sequence = PROP_HDR_SEQUENCE_DEFAULT;
  self->hdr_sequence2 = PROP_HDR_SEQUENCE2_DEFAULT;
  self->hdr_profile = PROP_HDR_PROFILE_DEFAULT;
  self->hdr_plugin = new HdrMetadataPlugin();
  self->hdr_switcher = new HdrProfileSwitcher();
  self->cam = PROP_CAM_DEFAULT;
  self->stream = PROP_STREAM_DEFAULT;
  self->illumination = PROP_ILLUMINATION_DEFAULT;
  self->sensor_offset_x = PROP_SENSOR_OFFSET_X_DEFAULT;
  self->sensor_offset_y = PROP_SENSOR_OFFSET_Y_DEFAULT;
  gst_video_info_init(&self->video_info);
#ifdef NVMM_ENABLED
  self->nvsurface_layout = PROP_NVSURFACE_LAYOUT_DEFAULT;
  self->gpu_id = PROP_GPU_ID_DEFAULT;
#endif

  gst_base_src_set_live(base, TRUE);
  gst_base_src_set_format(base, GST_FORMAT_TIME);
}

static void gst_pylon_src_set_property(GObject *object, guint property_id,
                                       const GValue *value, GParamSpec *pspec) {
  GstPylonSrc *self = GST_PYLON_SRC(object);

  GST_LOG_OBJECT(self, "set_property");

  GST_OBJECT_LOCK(self);

  switch (property_id) {
    case PROP_DEVICE_USER_NAME:
      g_free(self->device_user_name);
      self->device_user_name = g_value_dup_string(value);
      break;
    case PROP_DEVICE_SERIAL_NUMBER:
      g_free(self->device_serial_number);
      self->device_serial_number = g_value_dup_string(value);
      break;
    case PROP_DEVICE_INDEX:
      self->device_index = g_value_get_int(value);
      break;
    case PROP_USER_SET:
      g_free(self->user_set);
      self->user_set = g_value_dup_string(value);
      break;
    case PROP_PFS_LOCATION:
      g_free(self->pfs_location);
      self->pfs_location = g_value_dup_string(value);
      break;
    case PROP_ENABLE_CORRECTION:
      self->enable_correction = g_value_get_boolean(value);
      break;
    case PROP_CAPTURE_ERROR:
      self->capture_error =
          static_cast<GstPylonCaptureErrorEnum>(g_value_get_enum(value));
      break;
    case PROP_HDR_SEQUENCE:
      g_free(self->hdr_sequence);
      self->hdr_sequence = g_value_dup_string(value);
      break;
    case PROP_HDR_SEQUENCE2:
      g_free(self->hdr_sequence2);
      self->hdr_sequence2 = g_value_dup_string(value);
      break;
    case PROP_HDR_PROFILE:
      {
        gint new_profile = g_value_get_int(value);
        if (new_profile >= 0 && new_profile <= 1) {
          if (self->hdr_plugin && self->hdr_plugin->IsConfigured()) {
            // With software signals on all states, only need 1 retry
            gint retry_count = 1;
            self->hdr_switcher->RequestSwitch(new_profile, retry_count);
            GST_INFO_OBJECT(self, "Profile switch requested to %d, will retry %d times",
                           new_profile, retry_count);
          } else {
            GST_WARNING_OBJECT(self, "Cannot switch profile - HDR not configured");
          }
        } else if (new_profile != -1) {
          GST_WARNING_OBJECT(self, "Invalid profile value %d (must be 0 or 1)", new_profile);
        }
      }
      break;
#ifdef NVMM_ENABLED
    case PROP_NVSURFACE_LAYOUT:
      self->nvsurface_layout =
          static_cast<GstPylonNvsurfaceLayoutEnum>(g_value_get_enum(value));
      break;
    case PROP_GPU_ID:
      self->gpu_id = g_value_get_int(value);
      break;
#endif
    case PROP_ILLUMINATION:
      self->illumination = g_value_get_boolean(value);
      break;
    case PROP_SENSOR_OFFSET_X:
      self->sensor_offset_x = g_value_get_int(value);
      break;
    case PROP_SENSOR_OFFSET_Y:
      self->sensor_offset_y = g_value_get_int(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }

  GST_OBJECT_UNLOCK(self);
}

static void gst_pylon_src_get_property(GObject *object, guint property_id,
                                       GValue *value, GParamSpec *pspec) {
  GstPylonSrc *self = GST_PYLON_SRC(object);

  GST_LOG_OBJECT(self, "get_property");

  GST_OBJECT_LOCK(self);

  switch (property_id) {
    case PROP_DEVICE_USER_NAME:
      g_value_set_string(value, self->device_user_name);
      break;
    case PROP_DEVICE_SERIAL_NUMBER:
      g_value_set_string(value, self->device_serial_number);
      break;
    case PROP_DEVICE_INDEX:
      g_value_set_int(value, self->device_index);
      break;
    case PROP_USER_SET:
      g_value_set_string(value, self->user_set);
      break;
    case PROP_PFS_LOCATION:
      g_value_set_string(value, self->pfs_location);
      break;
    case PROP_ENABLE_CORRECTION:
      g_value_set_boolean(value, self->enable_correction);
      break;
    case PROP_CAPTURE_ERROR:
      g_value_set_enum(value, self->capture_error);
      break;
    case PROP_HDR_SEQUENCE:
      g_value_set_string(value, self->hdr_sequence);
      break;
    case PROP_HDR_SEQUENCE2:
      g_value_set_string(value, self->hdr_sequence2);
      break;
    case PROP_HDR_PROFILE:
      g_value_set_int(value,
        self->hdr_plugin ? self->hdr_plugin->GetCurrentProfile() : -1);
      break;
#ifdef NVMM_ENABLED
    case PROP_NVSURFACE_LAYOUT:
      g_value_set_enum(value, self->nvsurface_layout);
      break;
    case PROP_GPU_ID:
      g_value_set_uint(value, self->gpu_id);
      break;
#endif
    case PROP_ILLUMINATION:
      g_value_set_boolean(value, self->illumination);
      break;
    case PROP_DEVICE_TEMPERATURE: {
      GError *error = NULL;
      gdouble temperature = -273.15;
      if (self->pylon) {
        temperature = gst_pylon_get_device_temperature(self->pylon, &error);
        if (error) {
          GST_WARNING_OBJECT(self, "Failed to get device temperature: %s",
                             error->message);
          g_error_free(error);
        }
      }
      g_value_set_double(value, temperature);
      break;
    }
    case PROP_SENSOR_OFFSET_X:
      g_value_set_int(value, self->sensor_offset_x);
      break;
    case PROP_SENSOR_OFFSET_Y:
      g_value_set_int(value, self->sensor_offset_y);
      break;
    case PROP_CAM:
      g_value_set_object(value, self->cam);
      break;
    case PROP_STREAM:
      g_value_set_object(value, self->stream);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }

  GST_OBJECT_UNLOCK(self);
}

static void gst_pylon_src_finalize(GObject *object) {
  GstPylonSrc *self = GST_PYLON_SRC(object);

  GST_LOG_OBJECT(self, "finalize");

  g_free(self->device_user_name);
  self->device_user_name = NULL;

  g_free(self->device_serial_number);
  self->device_serial_number = NULL;

  g_free(self->user_set);
  self->user_set = NULL;

  g_free(self->hdr_sequence);
  self->hdr_sequence = NULL;

  g_free(self->hdr_sequence2);
  self->hdr_sequence2 = NULL;

  if (self->hdr_plugin) {
    delete self->hdr_plugin;
    self->hdr_plugin = NULL;
  }

  if (self->hdr_switcher) {
    delete self->hdr_switcher;
    self->hdr_switcher = NULL;
  }

  if (self->cam) {
    g_object_unref(self->cam);
    self->cam = NULL;
  }

  if (self->stream) {
    g_object_unref(self->stream);
    self->stream = NULL;
  }

  G_OBJECT_CLASS(gst_pylon_src_parent_class)->finalize(object);
}

/* get caps from subclass */
static GstCaps *gst_pylon_src_get_caps(GstBaseSrc *src, GstCaps *filter) {
  GstPylonSrc *self = GST_PYLON_SRC(src);
  GstCaps *outcaps = NULL;
  GError *error = NULL;

  if (!self->pylon) {
    outcaps = gst_pad_get_pad_template_caps(GST_BASE_SRC_PAD(self));
    GST_INFO_OBJECT(
        self,
        "Camera not open yet, returning src template caps %" GST_PTR_FORMAT,
        outcaps);
    goto out;
  }

  outcaps = gst_pylon_query_configuration(self->pylon, &error);

  if (outcaps == NULL && error) {
    goto log_gst_error;
  }

  GST_DEBUG_OBJECT(self, "Camera returned caps %" GST_PTR_FORMAT, outcaps);

  if (filter) {
    GstCaps *tmp = outcaps;

    GST_DEBUG_OBJECT(self, "Filtering with %" GST_PTR_FORMAT, filter);

    outcaps = gst_caps_intersect(outcaps, filter);
    gst_caps_unref(tmp);
  }

  GST_INFO_OBJECT(self, "Returning caps %" GST_PTR_FORMAT, outcaps);

  goto out;

log_gst_error:
  GST_ELEMENT_ERROR(self, LIBRARY, FAILED, ("Failed to get caps."),
                    ("%s", error->message));
  g_error_free(error);

out:
  return outcaps;
}

static gboolean gst_pylon_src_is_bayer(GstStructure *st) {
  gboolean is_bayer = FALSE;

  g_return_val_if_fail(st, FALSE);

  if (0 == g_strcmp0(gst_structure_get_name(st), "video/x-bayer")) {
    is_bayer = TRUE;
  }
  return is_bayer;
}

/* called if, in negotiation, caps need fixating */
static GstCaps *gst_pylon_src_fixate(GstBaseSrc *src, GstCaps *caps) {
  GstPylonSrc *self = GST_PYLON_SRC(src);
  GstCaps *outcaps = NULL;
  GstStructure *st = NULL;
  GstCapsFeatures *features = NULL;
  const GValue *width_field = NULL;
  static const gint width_1080p = 1920;
  static const gint height_1080p = 1080;
  static const gint preferred_framerate_num = 30;
  static const gint preferred_framerate_den = 1;
  gint preferred_width_adjusted = 0;

  /* get the configured width/height after applying userset and pfs */
  gint preferred_width = width_1080p;
  gint preferred_height = height_1080p;
  gst_pylon_get_startup_geometry(self->pylon, &preferred_width,
                                 &preferred_height);

  GST_DEBUG_OBJECT(self, "Fixating caps %" GST_PTR_FORMAT, caps);

  if (gst_caps_is_fixed(caps)) {
    GST_DEBUG_OBJECT(self, "Caps are already fixed");
    return caps;
  }

  outcaps = gst_caps_new_empty();
  st = gst_structure_copy(gst_caps_get_structure(caps, 0));
  features = gst_caps_features_copy(gst_caps_get_features(caps, 0));
  width_field = gst_structure_get_value(st, "width");
  gst_caps_unref(caps);

  if (gst_pylon_src_is_bayer(st) && GST_VALUE_HOLDS_INT_RANGE(width_field)) {
    preferred_width_adjusted =
        GST_ROUND_DOWN_4(gst_value_get_int_range_max(width_field));
  } else {
    preferred_width_adjusted = preferred_width;
  }

  gst_structure_fixate_field_nearest_int(st, "width", preferred_width_adjusted);
  gst_structure_fixate_field_nearest_int(st, "height", preferred_height);
  gst_structure_fixate_field_nearest_fraction(
      st, "framerate", preferred_framerate_num, preferred_framerate_den);

  gst_caps_append_structure_full(outcaps, st, features);

  /* fixate the remainder of the fields */
  outcaps = gst_caps_fixate(outcaps);

  GST_INFO_OBJECT(self, "Fixated caps to %" GST_PTR_FORMAT, outcaps);

  return outcaps;
}

/* enable chunks for HDR sequence metadata */
static void gst_pylon_src_enable_hdr_chunks(GstPylonSrc *self) {
  g_return_if_fail(GST_IS_PYLON_SRC(self));

  // IMPORTANT: Enable chunks BEFORE configuring HDR sequence
  // Once sequencer mode is configured, chunk settings become read-only
  GST_INFO_OBJECT(self, "Enabling chunks for HDR sequence metadata");

  // Enable chunks using the child proxy interface (same as cam:: on command line)
  GValue val = G_VALUE_INIT;
  g_value_init(&val, G_TYPE_BOOLEAN);
  g_value_set_boolean(&val, TRUE);

  // Set ChunkModeActive=TRUE first
  gst_child_proxy_set_property(GST_CHILD_PROXY(self), "cam::ChunkModeActive", &val);
  GST_INFO_OBJECT(self, "Set cam::ChunkModeActive=TRUE");

  // Enable ExposureTime chunk
  gst_child_proxy_set_property(GST_CHILD_PROXY(self), "cam::ChunkEnable-ExposureTime", &val);
  GST_INFO_OBJECT(self, "Set cam::ChunkEnable-ExposureTime=TRUE");

  // Also enable Timestamp chunk for better debugging
  gst_child_proxy_set_property(GST_CHILD_PROXY(self), "cam::ChunkEnable-Timestamp", &val);
  GST_INFO_OBJECT(self, "Set cam::ChunkEnable-Timestamp=TRUE");

  g_value_unset(&val);
  GST_INFO_OBJECT(self, "Chunk configuration completed");
}

/* notify the subclass of new caps */
static gboolean gst_pylon_src_set_caps(GstBaseSrc *src, GstCaps *caps) {
  GstPylonSrc *self = GST_PYLON_SRC(src);
  GstStructure *st = NULL;
  gint numerator = 0;
  gint denominator = 0;
  gint width = 0;
  static const gint byte_alignment = 4;
  gchar *error_msg = NULL;
  GError *error = NULL;
  gboolean ret = FALSE;
  const gchar *action = NULL;

  GST_INFO_OBJECT(self, "Setting new caps: %" GST_PTR_FORMAT, caps);

  st = gst_caps_get_structure(caps, 0);
  gst_structure_get_int(st, "width", &width);

  if (gst_pylon_src_is_bayer(st) && 0 != width % byte_alignment) {
    action = "configure";
    error_msg = g_strdup(
        "Bayer formats require the width to be word aligned (4 bytes).");
    goto error;
  }

  gst_structure_get_fraction(st, "framerate", &numerator, &denominator);

  GST_OBJECT_LOCK(self);
  if (numerator != 0) {
    self->duration = gst_util_uint64_scale(GST_SECOND, denominator, numerator);
  } else {
    self->duration = GST_CLOCK_TIME_NONE;
  }
  GST_OBJECT_UNLOCK(self);
  gst_element_post_message(GST_ELEMENT_CAST(self),
                           gst_message_new_latency(GST_OBJECT_CAST(self)));

  ret = gst_pylon_stop(self->pylon, &error);
  if (FALSE == ret && error) {
    action = "stop";
    goto log_error;
  }

  ret = gst_pylon_set_configuration(self->pylon, caps, &error);
  if (FALSE == ret && error) {
    action = "configure";
    goto log_error;
  }

  /* Configure HDR sequence if specified */
  if (self->hdr_sequence && strlen(self->hdr_sequence) > 0) {
    // Parse sequences
    std::vector<guint32> profile0_exposures;
    std::vector<guint32> profile1_exposures;
    std::vector<guint32> adjusted0, adjusted1;

    // Parse profile 0
    if (self->hdr_sequence) {
      gchar **exposures = g_strsplit(self->hdr_sequence, ",", -1);
      for (int i = 0; exposures[i]; i++) {
        profile0_exposures.push_back(g_ascii_strtoull(exposures[i], NULL, 10));
      }
      g_strfreev(exposures);
    }

    // Parse profile 1
    if (self->hdr_sequence2) {
      gchar **exposures = g_strsplit(self->hdr_sequence2, ",", -1);
      for (int i = 0; exposures[i]; i++) {
        profile1_exposures.push_back(g_ascii_strtoull(exposures[i], NULL, 10));
      }
      g_strfreev(exposures);
    }

    // Configure HDR plugin - it may adjust sequences for duplicates
    if (self->hdr_plugin->Configure(profile0_exposures, profile1_exposures,
                                    adjusted0, adjusted1)) {

      // Update sequences if they were adjusted
      if (!adjusted0.empty() && adjusted0 != profile0_exposures) {
        GString *str = g_string_new(NULL);
        for (size_t i = 0; i < adjusted0.size(); i++) {
          if (i > 0) g_string_append_c(str, ',');
          g_string_append_printf(str, "%u", adjusted0[i]);
        }
        GST_INFO_OBJECT(self, "Profile 0 sequence adjusted: %s -> %s",
                       self->hdr_sequence, str->str);
        g_free(self->hdr_sequence);
        self->hdr_sequence = g_string_free(str, FALSE);
      }

      if (!adjusted1.empty() && adjusted1 != profile1_exposures) {
        GString *str = g_string_new(NULL);
        for (size_t i = 0; i < adjusted1.size(); i++) {
          if (i > 0) g_string_append_c(str, ',');
          g_string_append_printf(str, "%u", adjusted1[i]);
        }
        GST_INFO_OBJECT(self, "Profile 1 sequence adjusted: %s -> %s",
                       self->hdr_sequence2, str->str);
        g_free(self->hdr_sequence2);
        self->hdr_sequence2 = g_string_free(str, FALSE);
      }

      // Enable chunks for HDR metadata
      gst_pylon_src_enable_hdr_chunks(self);

      // Configure camera with (possibly adjusted) sequences
      if (self->hdr_sequence2) {
        ret = gst_pylon_configure_dual_hdr_sequence(self->pylon,
                                                    self->hdr_sequence,
                                                    self->hdr_sequence2,
                                                    self->sensor_offset_x,
                                                    self->sensor_offset_y,
                                                    &error);
      } else {
        ret = gst_pylon_configure_hdr_sequence(self->pylon,
                                               self->hdr_sequence,
                                               self->sensor_offset_x,
                                               self->sensor_offset_y,
                                               &error);
      }

      if (ret) {
        GST_INFO_OBJECT(self, "HDR sequences configured successfully");
      } else {
        GST_ERROR_OBJECT(self, "Failed to configure camera: %s",
                        error ? error->message : "Unknown error");
        action = "configure HDR sequence";
        goto log_error;
      }
    } else {
      GST_ERROR_OBJECT(self, "Failed to configure HDR plugin");
      ret = FALSE;
      action = "configure HDR plugin";
      goto log_error;
    }
  }

  ret = gst_pylon_start(self->pylon, &error);
  if (FALSE == ret && error) {
    action = "start";
    goto log_error;
  }

  ret = gst_video_info_from_caps(&self->video_info, caps);

  goto out;

log_error:
  error_msg = g_strdup(error->message);
  g_error_free(error);

error:
  GST_ELEMENT_ERROR(self, LIBRARY, FAILED, ("Failed to %s camera.", action),
                    ("%s", error_msg));
  g_free(error_msg);

out:
  return ret;
}

/* setup allocation query */
static gboolean gst_pylon_src_decide_allocation(GstBaseSrc *src,
                                                GstQuery *query) {
  GstPylonSrc *self = GST_PYLON_SRC(src);

  GST_LOG_OBJECT(self, "decide_allocation");

  return TRUE;
}

/* start and stop processing, ideal for opening/closing the resource */
static gboolean gst_pylon_src_start(GstBaseSrc *src) {
  GstPylonSrc *self = GST_PYLON_SRC(src);
  GError *error = NULL;
  gboolean ret = TRUE;
  gboolean using_pfs = FALSE;
  gboolean same_device = TRUE;

  GST_OBJECT_LOCK(self);
  same_device =
      self->pylon && gst_pylon_is_same_device(self->pylon, self->device_index,
                                              self->device_user_name,
                                              self->device_serial_number);
  GST_OBJECT_UNLOCK(self);

  if (same_device) {
    goto out;
  }

  if (self->pylon) {
    gst_pylon_stop(self->pylon, &error);
    gst_pylon_free(self->pylon);
    self->pylon = NULL;

    if (error) {
      ret = FALSE;
      goto log_gst_error;
    }
  }

  GST_OBJECT_LOCK(self);

  Pylon::PylonInitialize();

  GST_INFO_OBJECT(
      self,
      "Attempting to create camera device with the following configuration:"
      "\n\tname: %s\n\tserial number: %s\n\tindex: %d\n\tuser set: %s \n\tPFS "
      "filepath: %s \n\tEnable correction: %s.\n"
      "If defined, the PFS file will override the user set configuration.",
      self->device_user_name, self->device_serial_number, self->device_index,
      self->user_set, self->pfs_location,
      ((self->enable_correction) ? "True" : "False"));

  self->pylon = gst_pylon_new(GST_ELEMENT_CAST(self), self->device_user_name,
                              self->device_serial_number, self->device_index,
                              self->enable_correction, &error);
#ifdef NVMM_ENABLED
  /* setup nvbufsurface if a new device has been created */
  if (self->pylon) {
    gst_pylon_set_nvsurface_layout(
        self->pylon,
        static_cast<GstPylonNvsurfaceLayoutEnum>(self->nvsurface_layout));
    gst_pylon_set_gpu_id(self->pylon, self->gpu_id);
  }
#endif
  GST_OBJECT_UNLOCK(self);

  if (error) {
    ret = FALSE;
    goto log_gst_error;
  }

  GST_OBJECT_LOCK(self);
  ret = gst_pylon_set_user_config(self->pylon, self->user_set, &error);
  GST_OBJECT_UNLOCK(self);

  if (ret == FALSE && error) {
    goto log_gst_error;
  }

  GST_OBJECT_LOCK(self);
  if (self->pfs_location) {
    using_pfs = TRUE;
    ret = gst_pylon_set_pfs_config(self->pylon, self->pfs_location, &error);
  }
  GST_OBJECT_UNLOCK(self);

  if (using_pfs && ret == FALSE && error) {
    goto log_gst_error;
  }

  /* Configure Line2 and Line3 for illumination control using Pylon API */
  if (self->pylon) {
    GError *line_err = NULL;
    GST_INFO_OBJECT(self, "Configuring Line2 and Line3 for illumination=%d", self->illumination);
    if (!gst_pylon_configure_line2(self->pylon, self->illumination, &line_err)) {
      GST_ERROR_OBJECT(self, "Failed to configure illumination lines: %s",
                      line_err ? line_err->message : "unknown error");
      g_clear_error(&line_err);
      /* Continue anyway - don't fail the pipeline if line configuration fails */
    } else {
      GST_INFO_OBJECT(self, "Line2 and Line3 configured successfully");
    }
  }

  self->duration = GST_CLOCK_TIME_NONE;

  goto out;

log_gst_error:
  GST_ELEMENT_ERROR(self, LIBRARY, FAILED, ("Failed to start camera."),
                    ("%s", error->message));
  g_error_free(error);

  /* no camera found. Stop pylon SDK */
  Pylon::PylonTerminate();

out:
  return ret;
}

static gboolean gst_pylon_src_stop(GstBaseSrc *src) {
  GstPylonSrc *self = GST_PYLON_SRC(src);
  GError *error = NULL;
  gboolean ret = TRUE;

  GST_INFO_OBJECT(self, "Stopping camera device");

  ret = gst_pylon_stop(self->pylon, &error);

  if (ret == FALSE && error) {
    GST_ELEMENT_ERROR(self, LIBRARY, FAILED, ("Failed to close camera."),
                      ("%s", error->message));
    g_error_free(error);
  }

  // Reset HDR metadata plugin and switcher
  if (self->hdr_plugin) {
    self->hdr_plugin->Reset();
  }
  if (self->hdr_switcher) {
    self->hdr_switcher->Reset();
  }

  gst_pylon_free(self->pylon);
  self->pylon = NULL;

  Pylon::PylonTerminate();

  return ret;
}

/* unlock any pending access to the resource. subclasses should unlock
 * any function ASAP. */
static gboolean gst_pylon_src_unlock(GstBaseSrc *src) {
  GstPylonSrc *self = GST_PYLON_SRC(src);

  GST_LOG_OBJECT(self, "unlock");

  gst_pylon_interrupt_capture(self->pylon);

  return TRUE;
}

/* notify subclasses of a query */
static gboolean gst_pylon_src_query(GstBaseSrc *src, GstQuery *query) {
  GstPylonSrc *self = GST_PYLON_SRC(src);
  gboolean res = FALSE;

  switch (GST_QUERY_TYPE(query)) {
    case GST_QUERY_LATENCY: {
      GstClockTime min_latency;
      GstClockTime max_latency;

      if (GST_CLOCK_TIME_NONE == self->duration) {
        GST_WARNING_OBJECT(
            src, "Can't report latency since framerate is not fixated yet");
        min_latency = 0;
        max_latency = GST_CLOCK_TIME_NONE;
      } else {
        max_latency = min_latency = self->duration;
      }

      GST_DEBUG_OBJECT(
          self, "report latency min %" GST_TIME_FORMAT " max %" GST_TIME_FORMAT,
          GST_TIME_ARGS(min_latency), GST_TIME_ARGS(max_latency));

      gst_query_set_latency(query, TRUE, min_latency, max_latency);

      res = TRUE;
      break;
    }
    default:
      res = GST_BASE_SRC_CLASS(gst_pylon_src_parent_class)->query(src, query);
      break;
  }

  return res;
}

/* add time metadata to buffer */
static void gst_plyon_src_add_metadata(GstPylonSrc *self, GstBuffer *buf) {
  GstClock *clock = NULL;
  GstClockTime abs_time = GST_CLOCK_TIME_NONE;
  GstClockTime base_time = GST_CLOCK_TIME_NONE;
  GstClockTime timestamp = GST_CLOCK_TIME_NONE;
  GstCaps *ref = NULL;
  guint64 offset = G_GUINT64_CONSTANT(0);
  GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN;
  GstPylonMeta *pylon_meta = NULL;
  guint width = 0;
  guint height = 0;
  guint n_planes = 0;
  gint stride[GST_VIDEO_MAX_PLANES] = {0};

  g_return_if_fail(self);
  g_return_if_fail(buf);

  pylon_meta =
      (GstPylonMeta *)gst_buffer_get_meta(buf, GST_PYLON_META_API_TYPE);

  GST_OBJECT_LOCK(self);
  /* set duration */
  GST_BUFFER_DURATION(buf) = self->duration;

  if ((clock = GST_ELEMENT_CLOCK(self))) {
    /* we have a clock, get base time and ref clock */
    base_time = GST_ELEMENT(self)->base_time;
    gst_object_ref(clock);
  } else {
    /* no clock, can't set timestamps */
    base_time = GST_CLOCK_TIME_NONE;
  }
  GST_OBJECT_UNLOCK(self);

  /* sample pipeline clock */
  if (clock) {
    abs_time = gst_clock_get_time(clock);
    gst_object_unref(clock);
  } else {
    abs_time = GST_CLOCK_TIME_NONE;
  }

  timestamp = abs_time - base_time;
  offset = pylon_meta->block_id;

  GST_BUFFER_TIMESTAMP(buf) = timestamp;
  GST_BUFFER_OFFSET(buf) = offset;
  GST_BUFFER_OFFSET_END(buf) = offset + 1;

  /* add pylon timestamp as reference timestamp meta */
  ref = gst_caps_from_string("timestamp/x-pylon");
  gst_buffer_add_reference_timestamp_meta(buf, ref, pylon_meta->timestamp,
                                          GST_CLOCK_TIME_NONE);
  gst_caps_unref(ref);

  /* add video meta data */
  format = GST_VIDEO_INFO_FORMAT(&self->video_info);
  width = GST_VIDEO_INFO_WIDTH(&self->video_info);
  height = GST_VIDEO_INFO_HEIGHT(&self->video_info);
  n_planes = GST_VIDEO_INFO_N_PLANES(&self->video_info);

  /* assuming pylon formats come in a single plane */
  for (guint p = 0; p < n_planes; p++) {
    stride[p] = pylon_meta->stride;
  }

  gst_buffer_add_video_meta_full(buf, GST_VIDEO_FRAME_FLAG_NONE, format, width,
                                 height, n_planes, self->video_info.offset,
                                 stride);
}

/* ask the subclass to create a buffer with offset and size, the default
 * implementation will call alloc and fill. */
static GstFlowReturn gst_pylon_src_create(GstPushSrc *src, GstBuffer **buf) {
  GstPylonSrc *self = GST_PYLON_SRC(src);
  GError *error = NULL;
  gboolean pylon_ret = TRUE;
  GstFlowReturn ret = GST_FLOW_OK;
  gint capture_error = -1;

  GST_OBJECT_LOCK(self);
  capture_error = self->capture_error;
  GST_OBJECT_UNLOCK(self);

  // Check if we need to send profile switch signals
  gint signal_profile;
  if (self->hdr_switcher->GetPendingSignal(&signal_profile)) {
    GError *switch_error = NULL;
    if (gst_pylon_switch_hdr_profile(self->pylon, signal_profile, &switch_error)) {
      GST_DEBUG_OBJECT(self, "Sent profile switch signal for profile %d", signal_profile);
    } else {
      GST_WARNING_OBJECT(self, "Failed to send profile switch signal: %s",
                         switch_error ? switch_error->message : "Unknown error");
      if (switch_error) g_error_free(switch_error);
    }
  }

  pylon_ret = gst_pylon_capture(
      self->pylon, buf, static_cast<GstPylonCaptureErrorEnum>(capture_error),
      &error);

  if (pylon_ret == FALSE) {
    if (error) {
      GST_ELEMENT_ERROR(self, LIBRARY, FAILED, ("Failed to create buffer."),
                        ("%s", error->message));
      g_error_free(error);
      ret = GST_FLOW_ERROR;
    } else {
      GST_DEBUG_OBJECT(self,
                       "Buffer not created, user requested EOS or device "
                       "connection was lost");
      ret = GST_FLOW_EOS;
    }
    goto done;
  }

  gst_plyon_src_add_metadata(self, *buf);

  // Process and attach HDR metadata if configured
  if (self->hdr_plugin && self->hdr_plugin->IsConfigured()) {
    // Get frame number and exposure time from buffer metadata
    GstPylonMeta *pylon_meta = gst_buffer_get_pylon_meta(*buf);
    if (pylon_meta) {
      guint64 frame_number = pylon_meta->image_number;
      guint32 exposure_time = 0;

      // Try to get exposure time from chunks
      if (pylon_meta->chunks) {
        // Look for exposure time in chunk data
        gdouble chunk_exposure = 0;
        if (gst_structure_get_double(pylon_meta->chunks, "ChunkExposureTime", &chunk_exposure)) {
          exposure_time = (guint32)chunk_exposure;  // Convert to microseconds
          GST_LOG_OBJECT(self, "Got exposure time from chunks: %u μs", exposure_time);
        } else if (gst_structure_get_double(pylon_meta->chunks, "ChunkExposureTimeAbs", &chunk_exposure)) {
          exposure_time = (guint32)chunk_exposure;  // Convert to microseconds
          GST_LOG_OBJECT(self, "Got exposure time from ChunkExposureTimeAbs: %u μs", exposure_time);
        }
      }

      // Attach HDR metadata if we have exposure time
      if (exposure_time > 0) {
        if (!self->hdr_plugin->ProcessAndAttachMetadata(*buf,
                                                         frame_number,
                                                         exposure_time)) {
          GST_WARNING_OBJECT(self, "Failed to attach HDR metadata for frame %lu", frame_number);
        } else {
          GST_LOG_OBJECT(self, "Attached HDR metadata for frame %lu with exposure %u μs",
                        frame_number, exposure_time);
        }
      } else {
        GST_DEBUG_OBJECT(self, "No exposure time available for frame %lu - HDR metadata not attached",
                        frame_number);
      }
    }
  }

  GST_LOG_OBJECT(self, "Created buffer %" GST_PTR_FORMAT, *buf);

done:
  return ret;
}

static guint gst_pylon_src_child_proxy_get_children_count(
    GstChildProxy *child_proxy) {
  return sizeof(gst_pylon_src_child_proxy_names) / sizeof(gchar *);
}

static GObject *gst_pylon_src_child_proxy_get_child_by_name(
    GstChildProxy *child_proxy, const gchar *name) {
  GstPylonSrc *self = GST_PYLON_SRC(child_proxy);
  GObject *obj = NULL;

  GST_DEBUG_OBJECT(self, "Looking for child \"%s\"", name);

  if (!gst_pylon_src_start(GST_BASE_SRC(self))) {
    GST_ERROR_OBJECT(self,
                     "Please specify a camera before attempting to set Pylon "
                     "device properties");
    return NULL;
  }

  if (!g_strcmp0(name, "cam")) {
    GST_OBJECT_LOCK(self);
    obj = gst_pylon_get_camera(self->pylon);
    GST_OBJECT_UNLOCK(self);
  } else if (!g_strcmp0(name, "stream")) {
    GST_OBJECT_LOCK(self);
    obj = gst_pylon_get_stream_grabber(self->pylon);
    GST_OBJECT_UNLOCK(self);
  } else {
    GST_ERROR_OBJECT(
        self, "No child named \"%s\". Use \"cam\" or \"stream\"  instead.",
        name);
  }

  return obj;
}

static GObject *gst_pylon_src_child_proxy_get_child_by_index(
    GstChildProxy *child_proxy, guint index) {
  GstPylonSrc *self = GST_PYLON_SRC(child_proxy);
  GObject *obj = NULL;

  GST_DEBUG_OBJECT(self, "Looking for child at index \"%d\"", index);

  if (index >= gst_pylon_src_child_proxy_get_children_count(child_proxy)) {
    GST_ERROR_OBJECT(
        self, "No child at index \"%d\". Use a valid child index instead.",
        index);
    goto done;
  }

  obj = gst_pylon_src_child_proxy_get_child_by_name(
      child_proxy, gst_pylon_src_child_proxy_names[index]);

done:
  return obj;
}

static void gst_pylon_src_child_proxy_init(GstChildProxyInterface *iface) {
  iface->get_child_by_name = gst_pylon_src_child_proxy_get_child_by_name;
  iface->get_child_by_index = gst_pylon_src_child_proxy_get_child_by_index;
  iface->get_children_count = gst_pylon_src_child_proxy_get_children_count;
}
