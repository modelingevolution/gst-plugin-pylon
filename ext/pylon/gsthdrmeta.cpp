/* SPDX-License-Identifier: BSD-2-Clause
 *
 * HDR Metadata for GStreamer - Plugin Independent
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gsthdrmeta.h"

static gboolean
gst_hdr_meta_init (GstMeta *meta, gpointer params, GstBuffer *buffer)
{
    GstHdrMeta *hdr_meta = (GstHdrMeta *) meta;

    hdr_meta->master_sequence = 0;
    hdr_meta->exposure_sequence_index = 0;
    hdr_meta->exposure_count = 0;
    hdr_meta->exposure_value = 0;
    hdr_meta->hdr_profile = 0;

    return TRUE;
}

static void
gst_hdr_meta_free (GstMeta *meta, GstBuffer *buffer)
{
    /* Nothing to free for this simple metadata */
}

static gboolean
gst_hdr_meta_transform (GstBuffer *dest, GstMeta *meta,
                       GstBuffer *buffer, GQuark type,
                       gpointer data)
{
    GstHdrMeta *src_meta = (GstHdrMeta *) meta;

    /* Only copy metadata on simple copy transforms */
    if (GST_META_TRANSFORM_IS_COPY (type)) {
        GstHdrMeta *dest_meta = gst_buffer_add_hdr_meta (dest,
            src_meta->master_sequence,
            src_meta->exposure_sequence_index,
            src_meta->exposure_count,
            src_meta->exposure_value,
            src_meta->hdr_profile);

        if (!dest_meta)
            return FALSE;
    } else {
        /* Don't transform for other types (subset, etc.) */
        return FALSE;
    }

    return TRUE;
}

GType
gst_hdr_meta_api_get_type (void)
{
    static GType type = 0;
    static const gchar *tags[] = {
        GST_META_TAG_VIDEO_STR,
        NULL
    };

    if (g_once_init_enter (&type)) {
        GType _type = gst_meta_api_type_register ("GstHdrMetaAPI", tags);
        g_once_init_leave (&type, _type);
    }

    return type;
}

const GstMetaInfo *
gst_hdr_meta_get_info (void)
{
    static const GstMetaInfo *meta_info = NULL;

    if (g_once_init_enter ((GstMetaInfo **) &meta_info)) {
        const GstMetaInfo *mi = gst_meta_register (
            GST_HDR_META_API_TYPE,
            "GstHdrMeta",
            sizeof (GstHdrMeta),
            gst_hdr_meta_init,
            gst_hdr_meta_free,
            gst_hdr_meta_transform);

        g_once_init_leave ((GstMetaInfo **) &meta_info, (GstMetaInfo *) mi);
    }

    return meta_info;
}

/**
 * gst_buffer_add_hdr_meta:
 * @buffer: a #GstBuffer
 * @master_sequence: master sequence number
 * @exposure_sequence_index: index in HDR sequence
 * @exposure_count: total exposures in profile
 * @exposure_value: exposure time in microseconds
 * @hdr_profile: HDR profile (0 or 1)
 *
 * Attaches HDR metadata to a buffer.
 *
 * Returns: (transfer none): the #GstHdrMeta added to @buffer
 */
GstHdrMeta *
gst_buffer_add_hdr_meta (GstBuffer *buffer,
                        guint64 master_sequence,
                        guint8 exposure_sequence_index,
                        guint8 exposure_count,
                        guint32 exposure_value,
                        guint8 hdr_profile)
{
    GstHdrMeta *meta;

    g_return_val_if_fail (GST_IS_BUFFER (buffer), NULL);

    meta = (GstHdrMeta *) gst_buffer_add_meta (buffer,
        GST_HDR_META_INFO, NULL);

    if (!meta)
        return NULL;

    meta->master_sequence = master_sequence;
    meta->exposure_sequence_index = exposure_sequence_index;
    meta->exposure_count = exposure_count;
    meta->exposure_value = exposure_value;
    meta->hdr_profile = hdr_profile;

    return meta;
}