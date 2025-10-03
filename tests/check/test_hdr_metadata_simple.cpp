/* Copyright (C) 2024 Basler AG
 *
 * Simple unit tests for HdrMetadataProvider without gtest dependency
 */

#include <iostream>
#include <cassert>
#include <vector>
#include <gst/gst.h>
#include "../../gst-libs/gst/pylon/gsthdrmetadataprovider.h"

#define TEST_START(name) std::cout << "Running test: " << name << "... "
#define TEST_PASS() std::cout << "PASSED" << std::endl
#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        std::cout << "FAILED at line " << __LINE__ << ": " #cond << std::endl; \
        return false; \
    } \
} while(0)

bool test_basic_configuration() {
    TEST_START("BasicConfiguration");

    HdrMetadataProvider provider;
    TEST_ASSERT(provider.IsConfigured() == FALSE);

    std::vector<guint32> profile0 = {19, 150};
    provider.SetProfile0Sequence(profile0);

    TEST_ASSERT(provider.IsConfigured() == TRUE);
    TEST_ASSERT(provider.GetActiveProfile() == 0);
    TEST_ASSERT(provider.GetMasterSequence() == 0);

    TEST_PASS();
    return true;
}

bool test_single_profile_sequence() {
    TEST_START("SingleProfileSequence");

    HdrMetadataProvider provider;
    std::vector<guint32> profile0 = {19, 150};
    provider.SetProfile0Sequence(profile0);

    // First window
    auto meta0 = provider.ProcessFrame(19, 0);
    TEST_ASSERT(meta0.master_sequence == 0);
    TEST_ASSERT(meta0.exposure_sequence_index == 0);
    TEST_ASSERT(meta0.exposure_count == 2);
    TEST_ASSERT(meta0.exposure_value == 19);
    TEST_ASSERT(meta0.hdr_profile == 0);

    auto meta1 = provider.ProcessFrame(150, 1);
    TEST_ASSERT(meta1.master_sequence == 0);
    TEST_ASSERT(meta1.exposure_sequence_index == 1);
    TEST_ASSERT(meta1.exposure_count == 2);
    TEST_ASSERT(meta1.exposure_value == 150);
    TEST_ASSERT(meta1.hdr_profile == 0);

    // Second window
    auto meta2 = provider.ProcessFrame(19, 2);
    TEST_ASSERT(meta2.master_sequence == 1);
    TEST_ASSERT(meta2.exposure_sequence_index == 0);
    TEST_ASSERT(meta2.exposure_count == 2);
    TEST_ASSERT(meta2.hdr_profile == 0);

    auto meta3 = provider.ProcessFrame(150, 3);
    TEST_ASSERT(meta3.master_sequence == 1);
    TEST_ASSERT(meta3.exposure_sequence_index == 1);
    TEST_ASSERT(meta3.hdr_profile == 0);

    TEST_PASS();
    return true;
}

bool test_profile_switching() {
    TEST_START("ProfileSwitching");

    HdrMetadataProvider provider;
    std::vector<guint32> profile0 = {19, 150};
    std::vector<guint32> profile1 = {250, 350, 450};

    provider.SetProfile0Sequence(profile0);
    provider.SetProfile1Sequence(profile1);

    // Start with profile 0
    auto meta0 = provider.ProcessFrame(19, 0);
    TEST_ASSERT(meta0.hdr_profile == 0);
    TEST_ASSERT(meta0.master_sequence == 0);

    auto meta1 = provider.ProcessFrame(150, 1);
    TEST_ASSERT(meta1.hdr_profile == 0);
    TEST_ASSERT(meta1.master_sequence == 0);

    // Switch to profile 1 - master sequence should increment
    auto meta2 = provider.ProcessFrame(250, 2);
    TEST_ASSERT(meta2.hdr_profile == 1);
    TEST_ASSERT(meta2.master_sequence == 1);
    TEST_ASSERT(meta2.exposure_sequence_index == 0);
    TEST_ASSERT(meta2.exposure_count == 3);

    auto meta3 = provider.ProcessFrame(350, 3);
    TEST_ASSERT(meta3.hdr_profile == 1);
    TEST_ASSERT(meta3.master_sequence == 1);
    TEST_ASSERT(meta3.exposure_sequence_index == 1);

    auto meta4 = provider.ProcessFrame(450, 4);
    TEST_ASSERT(meta4.hdr_profile == 1);
    TEST_ASSERT(meta4.master_sequence == 1);
    TEST_ASSERT(meta4.exposure_sequence_index == 2);

    // Complete profile 1 window and start new one
    auto meta5 = provider.ProcessFrame(250, 5);
    TEST_ASSERT(meta5.hdr_profile == 1);
    TEST_ASSERT(meta5.master_sequence == 2);
    TEST_ASSERT(meta5.exposure_sequence_index == 0);

    // Switch back to profile 0
    auto meta6 = provider.ProcessFrame(19, 6);
    TEST_ASSERT(meta6.hdr_profile == 0);
    TEST_ASSERT(meta6.master_sequence == 3); // Incremented due to switch
    TEST_ASSERT(meta6.exposure_sequence_index == 0);

    TEST_PASS();
    return true;
}

bool test_duplicate_exposure_handling() {
    TEST_START("DuplicateExposureHandling");

    HdrMetadataProvider provider;
    std::vector<guint32> profile0 = {100, 200};
    std::vector<guint32> profile1 = {100, 300}; // 100 is duplicate

    provider.SetProfile0Sequence(profile0);
    provider.SetProfile1Sequence(profile1);

    // Profile 0 should get the original 100
    auto meta0 = provider.ProcessFrame(100, 0);
    TEST_ASSERT(meta0.hdr_profile == 0);
    TEST_ASSERT(meta0.exposure_sequence_index == 0);

    // Profile 1's duplicate should be detected even with adjusted value
    auto meta1 = provider.ProcessFrame(101, 2);
    TEST_ASSERT(meta1.hdr_profile == 1);
    TEST_ASSERT(meta1.exposure_sequence_index == 0);

    TEST_PASS();
    return true;
}

bool test_variable_length_sequences() {
    TEST_START("VariableLengthSequences");

    HdrMetadataProvider provider;
    std::vector<guint32> profile0 = {10, 30, 90};  // 3 exposures
    std::vector<guint32> profile1 = {100, 200};    // 2 exposures

    provider.SetProfile0Sequence(profile0);
    provider.SetProfile1Sequence(profile1);

    // Test profile 0 (3 exposures)
    auto meta0 = provider.ProcessFrame(10, 0);
    TEST_ASSERT(meta0.exposure_count == 3);
    TEST_ASSERT(meta0.exposure_sequence_index == 0);

    auto meta1 = provider.ProcessFrame(30, 1);
    TEST_ASSERT(meta1.exposure_count == 3);
    TEST_ASSERT(meta1.exposure_sequence_index == 1);

    auto meta2 = provider.ProcessFrame(90, 2);
    TEST_ASSERT(meta2.exposure_count == 3);
    TEST_ASSERT(meta2.exposure_sequence_index == 2);
    TEST_ASSERT(meta2.master_sequence == 0);

    // New window in profile 0
    auto meta3 = provider.ProcessFrame(10, 3);
    TEST_ASSERT(meta3.master_sequence == 1);

    // Switch to profile 1 (2 exposures)
    auto meta4 = provider.ProcessFrame(100, 4);
    TEST_ASSERT(meta4.exposure_count == 2);
    TEST_ASSERT(meta4.exposure_sequence_index == 0);
    TEST_ASSERT(meta4.hdr_profile == 1);
    TEST_ASSERT(meta4.master_sequence == 2); // Incremented due to switch

    TEST_PASS();
    return true;
}

bool test_reset_functionality() {
    TEST_START("ResetFunctionality");

    HdrMetadataProvider provider;
    std::vector<guint32> profile0 = {19, 150};
    provider.SetProfile0Sequence(profile0);

    provider.ProcessFrame(19, 0);
    provider.ProcessFrame(150, 1);
    auto meta2 = provider.ProcessFrame(19, 2);
    TEST_ASSERT(meta2.master_sequence == 1);

    // Reset should clear everything
    provider.Reset();
    TEST_ASSERT(provider.IsConfigured() == FALSE);
    TEST_ASSERT(provider.GetMasterSequence() == 0);

    // Reconfigure and verify reset worked
    provider.SetProfile0Sequence(profile0);
    auto meta3 = provider.ProcessFrame(19, 0);
    TEST_ASSERT(meta3.master_sequence == 0); // Should start from 0 again

    TEST_PASS();
    return true;
}

bool test_master_sequence_continuity() {
    TEST_START("MasterSequenceContinuity");

    HdrMetadataProvider provider;
    std::vector<guint32> profile0 = {19, 150};
    std::vector<guint32> profile1 = {250, 350};

    provider.SetProfile0Sequence(profile0);
    provider.SetProfile1Sequence(profile1);

    guint64 frame = 0;

    // Profile 0, window 0
    provider.ProcessFrame(19, frame++);
    auto meta = provider.ProcessFrame(150, frame++);
    TEST_ASSERT(meta.master_sequence == 0);

    // Profile 0, window 1
    provider.ProcessFrame(19, frame++);
    meta = provider.ProcessFrame(150, frame++);
    TEST_ASSERT(meta.master_sequence == 1);

    // Switch to Profile 1 - master sequence increments
    meta = provider.ProcessFrame(250, frame++);
    TEST_ASSERT(meta.master_sequence == 2);

    provider.ProcessFrame(350, frame++);

    // Profile 1, window 2
    provider.ProcessFrame(250, frame++);
    meta = provider.ProcessFrame(350, frame++);
    TEST_ASSERT(meta.master_sequence == 3);

    // Switch back to Profile 0 - master sequence increments again
    meta = provider.ProcessFrame(19, frame++);
    TEST_ASSERT(meta.master_sequence == 4);

    TEST_PASS();
    return true;
}

int main(int argc, char **argv) {
    // Initialize GStreamer (needed for GST_WARNING, GST_INFO macros)
    gst_init(&argc, &argv);

    std::cout << "\n=== Running HdrMetadataProvider Unit Tests ===" << std::endl;

    int passed = 0;
    int failed = 0;

    // Run all tests
    if (test_basic_configuration()) passed++; else failed++;
    if (test_single_profile_sequence()) passed++; else failed++;
    if (test_profile_switching()) passed++; else failed++;
    if (test_duplicate_exposure_handling()) passed++; else failed++;
    if (test_variable_length_sequences()) passed++; else failed++;
    if (test_reset_functionality()) passed++; else failed++;
    if (test_master_sequence_continuity()) passed++; else failed++;

    std::cout << "\n=== Test Summary ===" << std::endl;
    std::cout << "PASSED: " << passed << std::endl;
    std::cout << "FAILED: " << failed << std::endl;

    return failed == 0 ? 0 : 1;
}