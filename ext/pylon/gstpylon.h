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

#ifndef _GST_PYLON_H_
#define _GST_PYLON_H_

#include <glib.h>
#include <gst/gst.h>

#include <atomic>

typedef struct _GstPylon GstPylon;

typedef enum {
  ENUM_KEEP = 0,
  ENUM_SKIP = 1,
  ENUM_ABORT = 2,
} GstPylonCaptureErrorEnum;

#ifdef NVMM_ENABLED
typedef enum {
  ENUM_BLOCK_LINEAR = 0,
  ENUM_PITCH = 1,
} GstPylonNvsurfaceLayoutEnum;

#  define PROP_GPU_ID_DEFAULT 0
#  define PROP_NVSURFACE_LAYOUT_DEFAULT ENUM_PITCH
#endif

GstPylon *gst_pylon_new(GstElement *gstpylonsrc, const gchar *device_user_name,
                        const gchar *device_serial_number, gint device_index,
                        gboolean enable_correction, GError **err);
gboolean gst_pylon_set_user_config(GstPylon *self, const gchar *user_set,
                                   GError **err);
void gst_pylon_free(GstPylon *self);

gboolean gst_pylon_start(GstPylon *self, GError **err);
gboolean gst_pylon_stop(GstPylon *self, GError **err);
void gst_pylon_interrupt_capture(GstPylon *self);
gboolean gst_pylon_capture(GstPylon *self, GstBuffer **buf,
                           GstPylonCaptureErrorEnum capture_error,
                           std::atomic<guint64> *error_count, GError **err);
GstCaps *gst_pylon_query_configuration(GstPylon *self, GError **err);
gboolean gst_pylon_get_startup_geometry(GstPylon *self, gint *start_width,
                                        gint *start_height);
gboolean gst_pylon_set_configuration(GstPylon *self, const GstCaps *conf,
                                     GError **err);
gboolean gst_pylon_set_pfs_config(GstPylon *self, const gchar *pfs_location,
                                  GError **err);
gboolean gst_pylon_configure_hdr_sequence(GstPylon *self, const gchar *hdr_sequence,
                                          gint offset_x, gint offset_y,
                                          GError **err);
gboolean gst_pylon_configure_dual_hdr_sequence(GstPylon *self,
                                               const gchar *hdr_sequence1,
                                               const gchar *hdr_sequence2,
                                               gint offset_x, gint offset_y,
                                               GError **err);
gboolean gst_pylon_switch_hdr_profile(GstPylon *self, gint profile, GError **err);

/* Basic mode (basic-camera=true): the camera has no usable Sequencer /
 * SoftwareSignal support, so profiles are just two exposure/gain pairs written
 * directly to the streaming camera. configure_basic_exposure puts the device in
 * plain manual-exposure operation (sequencer off, ExposureMode=Timed,
 * ExposureAuto/GainAuto off, sensor offsets applied); set_exposure_gain writes
 * one pair, clipping to the camera's range and logging what was accepted. */
gboolean gst_pylon_configure_basic_exposure(GstPylon *self, gint offset_x,
                                            gint offset_y, GError **err);
gboolean gst_pylon_set_exposure_gain(GstPylon *self, gdouble exposure_us,
                                     gdouble gain, GError **err);
/* Gain-only write (float "Gain", else integer "GainRaw"): clips to the live
 * range, reads back into *applied. Never touches ExposureTime, so it is the one
 * to use under ExposureMode=TriggerWidth (camera-slave), where ExposureTime is
 * not writable. FALSE + err when the camera has no writable gain node. */
gboolean gst_pylon_set_gain(GstPylon *self, gdouble gain, gdouble *applied,
                            GError **err);
gchar *gst_pylon_camera_get_string_properties();
gchar *gst_pylon_stream_grabber_get_string_properties();

GObject *gst_pylon_get_camera(GstPylon *self);
GObject *gst_pylon_get_stream_grabber(GstPylon *self);

gboolean gst_pylon_is_same_device(GstPylon *self, const gint device_index,
                                  const gchar *device_user_name,
                                  const gchar *device_serial_number);

gboolean gst_pylon_configure_line2(GstPylon *self, gboolean illumination,
                                   GError **err);

/* epic-060: configure the camera as an external-trigger slave. The Line-1 pulse
 * width drives the exposure (ExposureMode=TriggerWidth); chunks stay enabled so
 * ChunkExposureTime keeps flowing for classification. grab_timeout_us bounds the
 * per-frame grab wait (0 => unbounded); line_debouncer_us sets the Line-1
 * debouncer (<= 5 us). */
gboolean gst_pylon_configure_camera_slave(GstPylon *self,
                                          gdouble line_debouncer_us,
                                          gint64 grab_timeout_us,
                                          gboolean gain_controlled,
                                          const gdouble gain[2],
                                          gint initial_profile,
                                          gboolean *gain_is_raw,
                                          gdouble *gain_applied,
                                          GError **err);
/* epic-135: when gain_controlled, the call also forces GainAuto=Off, range-checks
 * gain[0..1] against the camera's Gain (or GainRaw) limits and writes
 * gain[initial_profile] BEFORE grabbing starts (fail fast on any of those). With
 * gain_controlled == FALSE the camera's gain is left exactly as configured. */

gdouble gst_pylon_get_device_temperature(GstPylon *self, GError **err);

void gst_pylon_device_reset(GstPylon *self);

#ifdef NVMM_ENABLED
void gst_pylon_set_nvsurface_layout(
    GstPylon *self, const GstPylonNvsurfaceLayoutEnum nvsurface_layout);
GstPylonNvsurfaceLayoutEnum gst_pylon_get_nvsurface_layout(GstPylon *self);

void gst_pylon_set_gpu_id(GstPylon *self, const gint gpu_id);
guint gst_pylon_get_gpu_id(GstPylon *self);
#endif /* NVMM_ENABLED */

#endif
