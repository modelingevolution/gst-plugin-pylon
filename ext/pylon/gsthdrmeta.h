/* SPDX-License-Identifier: BSD-2-Clause
 *
 * HDR Metadata for GStreamer - Plugin Independent
 */

#ifndef __GST_HDR_META_H__
#define __GST_HDR_META_H__

#include <gst/gst.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

#define GST_HDR_META_API_TYPE (gst_hdr_meta_api_get_type())
#define GST_HDR_META_INFO (gst_hdr_meta_get_info())

typedef struct _GstHdrMeta GstHdrMeta;

/**
 * GstHdrMeta:
 * @meta: parent #GstMeta
 * @master_sequence: Master sequence number that maintains continuity across HDR profile switches
 * @exposure_sequence_index: Index of current exposure in the HDR sequence (0-based)
 * @exposure_count: Total number of exposures in current HDR profile
 * @exposure_value: Actual exposure time in microseconds
 * @hdr_profile: Current HDR profile (0 or 1)
 *
 * HDR metadata for tracking exposure sequences in multi-exposure HDR imaging.
 * This metadata is plugin-independent and can be used by any source element.
 */
struct _GstHdrMeta {
    GstMeta meta;

    guint64 master_sequence;
    guint8  exposure_sequence_index;
    guint8  exposure_count;
    guint32 exposure_value;
    guint8  hdr_profile;
};

GType gst_hdr_meta_api_get_type (void);
const GstMetaInfo *gst_hdr_meta_get_info (void);

#define gst_buffer_get_hdr_meta(b) \
    ((GstHdrMeta*)gst_buffer_get_meta((b), GST_HDR_META_API_TYPE))

GstHdrMeta *gst_buffer_add_hdr_meta (GstBuffer *buffer,
                                      guint64 master_sequence,
                                      guint8 exposure_sequence_index,
                                      guint8 exposure_count,
                                      guint32 exposure_value,
                                      guint8 hdr_profile);

G_END_DECLS

#endif /* __GST_HDR_META_H__ */