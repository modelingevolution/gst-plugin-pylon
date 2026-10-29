/* gst-plugin-pylon — "exposure[:gain]" sequence grammar (epic-135).
 *
 * Shared by pylonsrc's basic-camera and camera-slave branches and by the unit
 * test in tests/check/generic/pylonsequence.c. Header-only and glib-only on
 * purpose: it must compile without the pylon SDK.
 *
 * Grammar:  seq := pair ("," pair)*      pair := exposure_us [":" gain]
 *   - exposure_us : positive number
 *   - gain        : non-negative number (dB on cameras with a float "Gain" node,
 *                   raw units on cameras that only have "GainRaw")
 *   - whitespace around tokens is ignored; an empty / blank string is valid and
 *     yields count == 0 (the caller decides whether that is allowed)
 *   - at most GST_PYLON_SEQUENCE_MAX pairs (= the Pico's PICO_MAX_FRAMES)
 * Anything else is INVALID (FALSE + message): the caller must fail configure
 * instead of guessing — a typo must never silently run a profile at the wrong
 * exposure or gain.
 */
#ifndef __GST_PYLON_SEQUENCE_H__
#define __GST_PYLON_SEQUENCE_H__

#include <glib.h>
#include <string.h>

G_BEGIN_DECLS

#define GST_PYLON_SEQUENCE_MAX 16

typedef struct {
  guint    count;                              /* pairs parsed; 0 for an empty string */
  gdouble  exposure[GST_PYLON_SEQUENCE_MAX];   /* us */
  gdouble  gain[GST_PYLON_SEQUENCE_MAX];       /* 0.0 when the pair has no ":gain" */
  gboolean gain_given;                         /* at least one pair carried ":gain" */
  gboolean mixed_gain;                         /* effective gains differ between pairs */
} GstPylonSequence;

/* Plain decimal only: digits with at most one '.', optional leading '+'. This
 * rejects "nan", "inf", hex and exponents, which g_ascii_strtod would accept and
 * which would then slip through a '< min || > max' range check (NaN compares
 * false) or overflow the uint32 the exposures are cast to. */
static inline gboolean
gst_pylon_sequence_parse_number(const gchar *tok, gdouble *out)
{
  const gchar *p = tok;
  gboolean digits = FALSE, dot = FALSE;
  gchar *end = NULL;

  if (!tok || !*tok) return FALSE;
  if (*p == '+') p++;
  for (; *p; p++) {
    if (g_ascii_isdigit(*p)) digits = TRUE;
    else if (*p == '.' && !dot) dot = TRUE;
    else return FALSE;
  }
  if (!digits) return FALSE;
  *out = g_ascii_strtod(tok, &end);
  return end != NULL && end != tok && *end == '\0' && *out <= (gdouble)G_MAXUINT32;
}

/* Returns TRUE and fills *out on success (count may be 0). Returns FALSE and, when
 * err_msg is non-NULL, a g_malloc'd message naming the offending pair otherwise;
 * *out is zeroed on failure. */
static inline gboolean
gst_pylon_sequence_parse(const gchar *s, GstPylonSequence *out, gchar **err_msg)
{
  gchar   *copy = NULL;
  gchar  **pairs = NULL;
  gboolean ok = TRUE;
  gdouble  first_gain = 0.0;
  guint    i;

  memset(out, 0, sizeof(*out));
  if (err_msg) *err_msg = NULL;
  if (!s) return TRUE;

  copy = g_strdup(s);
  g_strstrip(copy);
  if (!*copy) { g_free(copy); return TRUE; }

  pairs = g_strsplit(copy, ",", -1);
  for (i = 0; pairs[i] && ok; i++) {
    gchar  *pair = g_strstrip(pairs[i]);
    gchar **parts = NULL;
    guint   nparts = 0;
    gdouble exposure = 0.0, gain = 0.0;

    if (!*pair) {
      if (err_msg) *err_msg = g_strdup_printf("pair %u is empty", i + 1);
      ok = FALSE;
      break;
    }
    if (out->count >= GST_PYLON_SEQUENCE_MAX) {
      if (err_msg) *err_msg = g_strdup_printf("more than %d pairs", GST_PYLON_SEQUENCE_MAX);
      ok = FALSE;
      break;
    }

    parts = g_strsplit(pair, ":", -1);
    nparts = g_strv_length(parts);
    if (nparts < 1 || nparts > 2 ||
        !gst_pylon_sequence_parse_number(g_strstrip(parts[0]), &exposure) || exposure <= 0.0) {
      if (err_msg) *err_msg = g_strdup_printf("pair %u ('%s'): exposure must be a positive number, "
                                              "optionally followed by ':gain'", i + 1, pair);
      ok = FALSE;
    } else if (nparts == 2) {
      if (!gst_pylon_sequence_parse_number(g_strstrip(parts[1]), &gain) || gain < 0.0) {
        if (err_msg) *err_msg = g_strdup_printf("pair %u ('%s'): gain must be a non-negative number",
                                                i + 1, pair);
        ok = FALSE;
      } else {
        out->gain_given = TRUE;
      }
    }
    g_strfreev(parts);
    if (!ok) break;

    if (out->count == 0) first_gain = gain;
    else if (gain != first_gain) out->mixed_gain = TRUE;
    out->exposure[out->count] = exposure;
    out->gain[out->count] = gain;
    out->count++;
  }

  g_strfreev(pairs);
  g_free(copy);
  if (!ok) memset(out, 0, sizeof(*out));
  return ok;
}

G_END_DECLS

#endif /* __GST_PYLON_SEQUENCE_H__ */
