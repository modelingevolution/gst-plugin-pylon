/* Unit test for gstpylonsequence.h (epic-135). Pure glib; no camera needed.
 * Run: meson setup build -Dtests=enabled && meson test -C build generic_pylonsequence */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <gst/check/gstcheck.h>
#include "gstpylonsequence.h"

static GstPylonSequence seq;
static gchar *err;

#define PARSE_OK(str) do { g_clear_pointer(&err, g_free); \
  fail_unless(gst_pylon_sequence_parse(str, &seq, &err), "'%s' should parse (%s)", str, err ? err : ""); \
  fail_unless(err == NULL); } while (0)
#define PARSE_FAIL(str) do { g_clear_pointer(&err, g_free); \
  fail_if(gst_pylon_sequence_parse(str, &seq, &err), "'%s' should be rejected", str); \
  fail_unless(err != NULL, "'%s': rejection must carry a message", str); \
  fail_unless(seq.count == 0 && !seq.gain_given && !seq.mixed_gain, "'%s': out must be zeroed on failure", str); } while (0)

GST_START_TEST(test_empty_and_null)
{
  PARSE_OK(NULL);  fail_unless(seq.count == 0 && !seq.gain_given);
  PARSE_OK("");    fail_unless(seq.count == 0 && !seq.gain_given);
  PARSE_OK("   "); fail_unless(seq.count == 0 && !seq.gain_given);
}
GST_END_TEST

GST_START_TEST(test_exposure_only)
{
  PARSE_OK("100");
  fail_unless(seq.count == 1);
  fail_unless(seq.exposure[0] == 100.0 && seq.gain[0] == 0.0);
  fail_unless(!seq.gain_given && !seq.mixed_gain);

  PARSE_OK("100,2000");
  fail_unless(seq.count == 2 && seq.exposure[1] == 2000.0 && !seq.gain_given && !seq.mixed_gain);
}
GST_END_TEST

GST_START_TEST(test_gain_given)
{
  PARSE_OK("100:12");
  fail_unless(seq.count == 1 && seq.gain[0] == 12.0 && seq.gain_given && !seq.mixed_gain);

  PARSE_OK("2000:0");                    /* explicit 0 dB still counts as "given" */
  fail_unless(seq.gain_given && seq.gain[0] == 0.0);

  PARSE_OK("100:12,2000:12");
  fail_unless(seq.count == 2 && seq.gain_given && !seq.mixed_gain);

  PARSE_OK("100:12,2000");               /* second pair falls back to 0 -> mixed */
  fail_unless(seq.count == 2 && seq.gain[0] == 12.0 && seq.gain[1] == 0.0 && seq.gain_given && seq.mixed_gain);

  PARSE_OK(" 100 : 12 , 2000 : 6 ");     /* whitespace tolerated */
  fail_unless(seq.count == 2 && seq.exposure[1] == 2000.0 && seq.gain[1] == 6.0 && seq.mixed_gain);

  PARSE_OK("19.5:1.25");
  fail_unless(seq.exposure[0] == 19.5 && seq.gain[0] == 1.25);
}
GST_END_TEST

GST_START_TEST(test_invalid)
{
  PARSE_FAIL("0:5");        /* exposure must be > 0 */
  PARSE_FAIL("-100");
  PARSE_FAIL("1OO:12");     /* typo: letter O */
  PARSE_FAIL("100:12:3");   /* too many fields */
  PARSE_FAIL("100:-1");     /* negative gain */
  PARSE_FAIL("100:");       /* trailing colon, empty gain */
  PARSE_FAIL("100,");       /* trailing comma, empty pair */
  PARSE_FAIL(",100");
  PARSE_FAIL("100,,200");
  PARSE_FAIL("abc");
  PARSE_FAIL("100 200");    /* missing separator */
}
GST_END_TEST

GST_START_TEST(test_pair_cap)
{
  gchar *ok16 = g_strdup("1"); gchar *bad17;
  for (int i = 1; i < 16; i++) { gchar *t = g_strdup_printf("%s,%d", ok16, i + 1); g_free(ok16); ok16 = t; }
  bad17 = g_strdup_printf("%s,17", ok16);
  PARSE_OK(ok16);  fail_unless(seq.count == 16 && seq.exposure[15] == 16.0);
  PARSE_FAIL(bad17);
  g_free(ok16); g_free(bad17);
}
GST_END_TEST

static Suite *
pylonsequence_suite(void)
{
  Suite *s = suite_create("pylonsequence");
  TCase *tc = tcase_create("general");
  suite_add_tcase(s, tc);
  tcase_add_test(tc, test_empty_and_null);
  tcase_add_test(tc, test_exposure_only);
  tcase_add_test(tc, test_gain_given);
  tcase_add_test(tc, test_invalid);
  tcase_add_test(tc, test_pair_cap);
  return s;
}

GST_CHECK_MAIN(pylonsequence);
