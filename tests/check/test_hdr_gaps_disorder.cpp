/* Copyright (C) 2024 Basler AG
 *
 * Unit tests for HdrMetadataProvider with gaps and out-of-order frames
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

bool test_frame_gaps() {
    TEST_START("FrameGaps");

    HdrMetadataProvider provider;
    std::vector<guint32> profile0 = {19, 150, 250};  // 3 exposures
    provider.SetProfile0Sequence(profile0);

    // First complete window (frames 0-2)
    auto meta0 = provider.ProcessFrame(19, 0);
    TEST_ASSERT(meta0.master_sequence == 0);
    TEST_ASSERT(meta0.exposure_sequence_index == 0);
    TEST_ASSERT(meta0.exposure_count == 3);

    auto meta1 = provider.ProcessFrame(150, 1);
    TEST_ASSERT(meta1.master_sequence == 0);
    TEST_ASSERT(meta1.exposure_sequence_index == 1);

    auto meta2 = provider.ProcessFrame(250, 2);
    TEST_ASSERT(meta2.master_sequence == 0);
    TEST_ASSERT(meta2.exposure_sequence_index == 2);

    // GAP: Skip frames 3-9, jump to frame 10
    // This should start a new window
    auto meta10 = provider.ProcessFrame(19, 10);
    TEST_ASSERT(meta10.master_sequence == 1);  // New window
    TEST_ASSERT(meta10.exposure_sequence_index == 0);

    auto meta11 = provider.ProcessFrame(150, 11);
    TEST_ASSERT(meta11.master_sequence == 1);
    TEST_ASSERT(meta11.exposure_sequence_index == 1);

    // GAP: Skip to frame 20 mid-sequence
    // Since we're jumping from index 1 to index 2, this continues the same window
    auto meta20 = provider.ProcessFrame(250, 20);
    TEST_ASSERT(meta20.master_sequence == 1);  // Same window
    TEST_ASSERT(meta20.exposure_sequence_index == 2);

    // GAP: Big jump to frame 100, starting new window
    auto meta100 = provider.ProcessFrame(19, 100);
    TEST_ASSERT(meta100.master_sequence == 2);  // New window
    TEST_ASSERT(meta100.exposure_sequence_index == 0);

    // Continue normally
    auto meta101 = provider.ProcessFrame(150, 101);
    TEST_ASSERT(meta101.master_sequence == 2);
    TEST_ASSERT(meta101.exposure_sequence_index == 1);

    TEST_PASS();
    return true;
}

bool test_frame_gaps_with_profile_switch() {
    TEST_START("FrameGapsWithProfileSwitch");

    HdrMetadataProvider provider;
    std::vector<guint32> profile0 = {19, 150};
    std::vector<guint32> profile1 = {250, 350, 450};

    provider.SetProfile0Sequence(profile0);
    provider.SetProfile1Sequence(profile1);

    // Profile 0 window
    auto meta0 = provider.ProcessFrame(19, 0);
    TEST_ASSERT(meta0.master_sequence == 0);
    TEST_ASSERT(meta0.hdr_profile == 0);

    auto meta1 = provider.ProcessFrame(150, 1);
    TEST_ASSERT(meta1.master_sequence == 0);
    TEST_ASSERT(meta1.hdr_profile == 0);

    // GAP + PROFILE SWITCH: Jump to frame 50 with profile 1
    auto meta50 = provider.ProcessFrame(250, 50);
    TEST_ASSERT(meta50.master_sequence == 1);  // Incremented due to profile switch
    TEST_ASSERT(meta50.hdr_profile == 1);
    TEST_ASSERT(meta50.exposure_sequence_index == 0);

    auto meta51 = provider.ProcessFrame(350, 51);
    TEST_ASSERT(meta51.master_sequence == 1);
    TEST_ASSERT(meta51.hdr_profile == 1);

    // GAP within same profile
    auto meta60 = provider.ProcessFrame(450, 60);
    TEST_ASSERT(meta60.master_sequence == 1);  // Same window
    TEST_ASSERT(meta60.exposure_sequence_index == 2);

    // GAP + New window in same profile
    auto meta70 = provider.ProcessFrame(250, 70);
    TEST_ASSERT(meta70.master_sequence == 2);  // New window
    TEST_ASSERT(meta70.hdr_profile == 1);
    TEST_ASSERT(meta70.exposure_sequence_index == 0);

    // GAP + Switch back to profile 0
    auto meta100 = provider.ProcessFrame(19, 100);
    TEST_ASSERT(meta100.master_sequence == 3);  // Incremented due to profile switch
    TEST_ASSERT(meta100.hdr_profile == 0);

    TEST_PASS();
    return true;
}

bool test_out_of_order_frames() {
    TEST_START("OutOfOrderFrames");

    HdrMetadataProvider provider;
    std::vector<guint32> profile0 = {10, 30, 90};  // 3 exposures
    provider.SetProfile0Sequence(profile0);

    // Start normal
    auto meta0 = provider.ProcessFrame(10, 0);
    TEST_ASSERT(meta0.master_sequence == 0);
    TEST_ASSERT(meta0.exposure_sequence_index == 0);

    // OUT OF ORDER: Jump to index 2 (skipping index 1)
    auto meta1 = provider.ProcessFrame(90, 1);
    TEST_ASSERT(meta1.master_sequence == 0);  // Same window
    TEST_ASSERT(meta1.exposure_sequence_index == 2);  // Index 2, not 1!

    // OUT OF ORDER: Go back to index 1
    auto meta2 = provider.ProcessFrame(30, 2);
    TEST_ASSERT(meta2.master_sequence == 0);  // Still same window
    TEST_ASSERT(meta2.exposure_sequence_index == 1);

    // Start new window normally
    auto meta3 = provider.ProcessFrame(10, 3);
    TEST_ASSERT(meta3.master_sequence == 1);  // New window
    TEST_ASSERT(meta3.exposure_sequence_index == 0);

    // OUT OF ORDER: Within new window, do 0 -> 2 -> 1
    auto meta4 = provider.ProcessFrame(90, 4);
    TEST_ASSERT(meta4.master_sequence == 1);
    TEST_ASSERT(meta4.exposure_sequence_index == 2);

    auto meta5 = provider.ProcessFrame(30, 5);
    TEST_ASSERT(meta5.master_sequence == 1);
    TEST_ASSERT(meta5.exposure_sequence_index == 1);

    // New window again
    auto meta6 = provider.ProcessFrame(10, 6);
    TEST_ASSERT(meta6.master_sequence == 2);

    TEST_PASS();
    return true;
}

bool test_out_of_order_with_profile_switch() {
    TEST_START("OutOfOrderWithProfileSwitch");

    HdrMetadataProvider provider;
    std::vector<guint32> profile0 = {19, 150};
    std::vector<guint32> profile1 = {250, 350, 450};

    provider.SetProfile0Sequence(profile0);
    provider.SetProfile1Sequence(profile1);

    // Start with profile 0
    auto meta0 = provider.ProcessFrame(19, 0);
    TEST_ASSERT(meta0.master_sequence == 0);
    TEST_ASSERT(meta0.hdr_profile == 0);

    // OUT OF ORDER in profile 0: Skip to second exposure
    auto meta1 = provider.ProcessFrame(150, 1);
    TEST_ASSERT(meta1.master_sequence == 0);
    TEST_ASSERT(meta1.exposure_sequence_index == 1);

    // Switch to profile 1
    auto meta2 = provider.ProcessFrame(250, 2);
    TEST_ASSERT(meta2.master_sequence == 1);
    TEST_ASSERT(meta2.hdr_profile == 1);
    TEST_ASSERT(meta2.exposure_sequence_index == 0);

    // OUT OF ORDER in profile 1: Jump to index 2
    auto meta3 = provider.ProcessFrame(450, 3);
    TEST_ASSERT(meta3.master_sequence == 1);
    TEST_ASSERT(meta3.exposure_sequence_index == 2);

    // Back to index 1
    auto meta4 = provider.ProcessFrame(350, 4);
    TEST_ASSERT(meta4.master_sequence == 1);
    TEST_ASSERT(meta4.exposure_sequence_index == 1);

    // New window in profile 1, but start with index 1 (out of order)
    auto meta5 = provider.ProcessFrame(350, 5);
    TEST_ASSERT(meta5.master_sequence == 1);  // Same window (didn't start with index 0)
    TEST_ASSERT(meta5.exposure_sequence_index == 1);

    // Now index 0 - this should trigger new window
    auto meta6 = provider.ProcessFrame(250, 6);
    TEST_ASSERT(meta6.master_sequence == 2);  // New window
    TEST_ASSERT(meta6.exposure_sequence_index == 0);

    TEST_PASS();
    return true;
}

bool test_extreme_gaps() {
    TEST_START("ExtremeGaps");

    HdrMetadataProvider provider;
    std::vector<guint32> profile0 = {100, 200};
    provider.SetProfile0Sequence(profile0);

    // Frame 0
    auto meta0 = provider.ProcessFrame(100, 0);
    TEST_ASSERT(meta0.master_sequence == 0);

    // Huge gap - jump to frame 1000000
    auto meta1M = provider.ProcessFrame(200, 1000000);
    TEST_ASSERT(meta1M.master_sequence == 0);  // Still in first window
    TEST_ASSERT(meta1M.exposure_sequence_index == 1);

    // Continue with huge frame numbers
    auto meta1M1 = provider.ProcessFrame(100, 1000001);
    TEST_ASSERT(meta1M1.master_sequence == 1);  // New window
    TEST_ASSERT(meta1M1.exposure_sequence_index == 0);

    // Jump backwards (should not affect master sequence logic)
    auto meta10 = provider.ProcessFrame(200, 10);
    TEST_ASSERT(meta10.master_sequence == 1);  // Continues from where we were
    TEST_ASSERT(meta10.exposure_sequence_index == 1);

    TEST_PASS();
    return true;
}

bool test_single_exposure_with_gaps() {
    TEST_START("SingleExposureWithGaps");

    HdrMetadataProvider provider;
    std::vector<guint32> profile0 = {50};  // Single exposure
    std::vector<guint32> profile1 = {200}; // Single exposure

    provider.SetProfile0Sequence(profile0);
    provider.SetProfile1Sequence(profile1);

    // Each frame is a complete window
    auto meta0 = provider.ProcessFrame(50, 0);
    TEST_ASSERT(meta0.master_sequence == 0);
    TEST_ASSERT(meta0.exposure_count == 1);

    // Gap - each frame increments master sequence
    auto meta10 = provider.ProcessFrame(50, 10);
    TEST_ASSERT(meta10.master_sequence == 1);

    auto meta20 = provider.ProcessFrame(50, 20);
    TEST_ASSERT(meta20.master_sequence == 2);

    // Switch to profile 1 with gap
    auto meta100 = provider.ProcessFrame(200, 100);
    TEST_ASSERT(meta100.master_sequence == 3);
    TEST_ASSERT(meta100.hdr_profile == 1);

    // Another frame in profile 1
    auto meta200 = provider.ProcessFrame(200, 200);
    TEST_ASSERT(meta200.master_sequence == 4);

    // Switch back to profile 0
    auto meta300 = provider.ProcessFrame(50, 300);
    TEST_ASSERT(meta300.master_sequence == 5);
    TEST_ASSERT(meta300.hdr_profile == 0);

    TEST_PASS();
    return true;
}


int main(int argc, char **argv) {
    // Initialize GStreamer
    gst_init(&argc, &argv);

    std::cout << "\n=== Running HDR Metadata Provider Gap & Disorder Tests ===" << std::endl;

    int passed = 0;
    int failed = 0;

    // Run all tests
    if (test_frame_gaps()) passed++; else failed++;
    if (test_frame_gaps_with_profile_switch()) passed++; else failed++;
    if (test_out_of_order_frames()) passed++; else failed++;
    if (test_out_of_order_with_profile_switch()) passed++; else failed++;
    if (test_extreme_gaps()) passed++; else failed++;
    if (test_single_exposure_with_gaps()) passed++; else failed++;

    std::cout << "\n=== Test Summary ===" << std::endl;
    std::cout << "PASSED: " << passed << std::endl;
    std::cout << "FAILED: " << failed << std::endl;

    if (failed > 0) {
        std::cout << "\nNote: Some tests may fail due to design decisions about how to handle" << std::endl;
        std::cout << "ambiguous cases like repeated exposure values in a sequence." << std::endl;
    }

    return failed == 0 ? 0 : 1;
}