#include "HdrMetadataProvider.h"
#include <cassert>
#include <iostream>
#include <iomanip>

using namespace HdrMetadata;

void test_window_size_2vs3() {
    std::cout << "Testing Window Size 2 vs 3..." << std::endl;

    std::vector<uint32_t> profile0 = {19, 150};
    std::vector<uint32_t> profile1 = {250, 350, 450};
    std::vector<uint32_t> adjusted0, adjusted1;

    auto provider = HdrMetadataProvider::Create(profile0, profile1, adjusted0, adjusted1);

    // Profile 0 (complete 3 windows)
    auto meta = provider->ProcessFrame(1, 19);
    assert(meta.MasterSequence == 1);
    assert(meta.HdrProfile == 0);

    meta = provider->ProcessFrame(2, 150);
    assert(meta.MasterSequence == 1);  // Complete window 1

    meta = provider->ProcessFrame(3, 19);
    assert(meta.MasterSequence == 2);  // Window 2

    meta = provider->ProcessFrame(4, 150);
    assert(meta.MasterSequence == 2);  // Complete window 2

    meta = provider->ProcessFrame(5, 19);
    assert(meta.MasterSequence == 3);  // Window 3

    meta = provider->ProcessFrame(6, 150);
    assert(meta.MasterSequence == 3);  // Complete window 3

    // Switch to Profile 1 at frame 7
    meta = provider->ProcessFrame(7, 250);
    assert(meta.MasterSequence == 4);  // Continues to window 4
    assert(meta.HdrProfile == 1);

    meta = provider->ProcessFrame(8, 350);
    assert(meta.MasterSequence == 4);

    meta = provider->ProcessFrame(9, 450);
    assert(meta.MasterSequence == 4);  // Complete window 4

    // Continue Profile 1
    meta = provider->ProcessFrame(10, 250);
    assert(meta.MasterSequence == 5);  // Window 5

    meta = provider->ProcessFrame(11, 350);
    assert(meta.MasterSequence == 5);

    meta = provider->ProcessFrame(12, 450);
    assert(meta.MasterSequence == 5);  // Complete window 5

    // Switch back to Profile 0 at frame 13
    meta = provider->ProcessFrame(13, 19);
    assert(meta.MasterSequence == 6);  // Continues to window 6
    assert(meta.HdrProfile == 0);

    meta = provider->ProcessFrame(14, 150);
    assert(meta.MasterSequence == 6);  // Complete window 6

    std::cout << "✓ Window Size 2 vs 3 test passed" << std::endl;
}

void test_duplicate_exposures() {
    std::cout << "Testing Duplicate Exposure Adjustment..." << std::endl;

    std::vector<uint32_t> profile0 = {100, 200, 300};
    std::vector<uint32_t> profile1 = {100, 250, 300};
    std::vector<uint32_t> adjusted0, adjusted1;

    auto provider = HdrMetadataProvider::Create(profile0, profile1, adjusted0, adjusted1);

    // Check adjustments
    assert(adjusted0[0] == 100);
    assert(adjusted0[1] == 200);
    assert(adjusted0[2] == 300);

    assert(adjusted1[0] == 101);  // 100 -> 101 (adjusted)
    assert(adjusted1[1] == 250);  // 250 unchanged
    assert(adjusted1[2] == 301);  // 300 -> 301 (adjusted)

    // Process frames with adjusted values
    auto meta = provider->ProcessFrame(1, 100);
    assert(meta.HdrProfile == 0);
    assert(meta.ExposureSequenceIndex == 0);

    // Profile 1 should use adjusted value (101)
    meta = provider->ProcessFrame(2, 101);
    assert(meta.HdrProfile == 1);
    assert(meta.ExposureSequenceIndex == 0);

    meta = provider->ProcessFrame(3, 301);
    assert(meta.HdrProfile == 1);
    assert(meta.ExposureSequenceIndex == 2);

    std::cout << "✓ Duplicate exposure adjustment test passed" << std::endl;
}

void test_window_size_1vs1() {
    std::cout << "Testing Window Size 1 vs 1..." << std::endl;

    std::vector<uint32_t> profile0 = {50};
    std::vector<uint32_t> profile1 = {200};
    std::vector<uint32_t> adjusted0, adjusted1;

    auto provider = HdrMetadataProvider::Create(profile0, profile1, adjusted0, adjusted1);

    // Profile 0
    auto meta = provider->ProcessFrame(1, 50);
    assert(meta.MasterSequence == 1);
    assert(meta.HdrProfile == 0);

    meta = provider->ProcessFrame(2, 50);
    assert(meta.MasterSequence == 2);

    meta = provider->ProcessFrame(3, 50);
    assert(meta.MasterSequence == 3);

    // Switch to Profile 1
    meta = provider->ProcessFrame(4, 200);
    assert(meta.MasterSequence == 4);
    assert(meta.HdrProfile == 1);

    meta = provider->ProcessFrame(5, 200);
    assert(meta.MasterSequence == 5);

    // Switch back to Profile 0
    meta = provider->ProcessFrame(6, 50);
    assert(meta.MasterSequence == 6);
    assert(meta.HdrProfile == 0);

    std::cout << "✓ Window Size 1 vs 1 test passed" << std::endl;
}

void test_extensive_switching() {
    std::cout << "Testing Extensive Profile Switching..." << std::endl;

    std::vector<uint32_t> profile0 = {19, 150};
    std::vector<uint32_t> profile1 = {250, 350, 450};
    std::vector<uint32_t> adjusted0, adjusted1;

    auto provider = HdrMetadataProvider::Create(profile0, profile1, adjusted0, adjusted1);

    uint64_t frame = 1;
    uint64_t expectedMaster = 1;

    // Start with Profile 0 - complete 2 windows
    provider->ProcessFrame(frame++, 19);
    auto meta = provider->ProcessFrame(frame++, 150);
    assert(meta.MasterSequence == expectedMaster++);  // Window 1
    assert(meta.HdrProfile == 0);

    provider->ProcessFrame(frame++, 19);
    meta = provider->ProcessFrame(frame++, 150);
    assert(meta.MasterSequence == expectedMaster++);  // Window 2

    // Switch to Profile 1
    meta = provider->ProcessFrame(frame++, 250);
    assert(meta.MasterSequence == expectedMaster);    // Window 3 starts
    assert(meta.HdrProfile == 1);

    provider->ProcessFrame(frame++, 350);
    meta = provider->ProcessFrame(frame++, 450);
    assert(meta.MasterSequence == expectedMaster++);  // Window 3 complete

    // Complete one more window in Profile 1
    provider->ProcessFrame(frame++, 250);
    provider->ProcessFrame(frame++, 350);
    meta = provider->ProcessFrame(frame++, 450);
    assert(meta.MasterSequence == expectedMaster++);  // Window 4

    // Switch back to Profile 0
    meta = provider->ProcessFrame(frame++, 19);
    assert(meta.MasterSequence == expectedMaster);    // Window 5 starts
    assert(meta.HdrProfile == 0);

    meta = provider->ProcessFrame(frame++, 150);
    assert(meta.MasterSequence == expectedMaster++);  // Window 5 complete

    std::cout << "✓ Extensive profile switching test passed" << std::endl;
}

void test_extreme_gaps() {
    std::cout << "Testing Extreme Gaps..." << std::endl;

    std::vector<uint32_t> profile0 = {100, 200};
    std::vector<uint32_t> profile1 = {};
    std::vector<uint32_t> adjusted0, adjusted1;

    auto provider = HdrMetadataProvider::Create(profile0, profile1, adjusted0, adjusted1);

    auto meta = provider->ProcessFrame(1, 100);
    assert(meta.MasterSequence == 1);

    // Huge gap - jump to frame 1000000
    meta = provider->ProcessFrame(1000000, 200);
    assert(meta.MasterSequence == 500000);  // Still calculated correctly
    assert(meta.ExposureSequenceIndex == 1);

    // Continue with huge frame numbers
    meta = provider->ProcessFrame(1000001, 100);
    assert(meta.MasterSequence == 500001);  // New window
    assert(meta.ExposureSequenceIndex == 0);

    std::cout << "✓ Extreme gaps test passed" << std::endl;
}

void test_mid_cycle_switches() {
    std::cout << "Testing Mid-Cycle Switches..." << std::endl;

    std::vector<uint32_t> profile0 = {10, 20};
    std::vector<uint32_t> profile1 = {30, 40};
    std::vector<uint32_t> adjusted0, adjusted1;

    auto provider = HdrMetadataProvider::Create(profile0, profile1, adjusted0, adjusted1);

    uint64_t f = 1;
    uint64_t m = 1;

    // Start with Profile 0
    auto meta = provider->ProcessFrame(f++, 10);
    assert(meta.MasterSequence == m);
    assert(meta.HdrProfile == 0);
    assert(meta.ExposureSequenceIndex == 0);

    // Mid-cycle switch to Profile 1
    meta = provider->ProcessFrame(f++, 40);
    assert(meta.MasterSequence == m);  // Should still be window 1
    assert(meta.HdrProfile == 1);
    assert(meta.ExposureSequenceIndex == 1);

    // Continue in Profile 1
    meta = provider->ProcessFrame(f++, 30);
    assert(meta.MasterSequence == ++m);
    assert(meta.ExposureSequenceIndex == 0);

    meta = provider->ProcessFrame(f++, 40);
    assert(meta.MasterSequence == m);

    // Start new window in Profile 1
    meta = provider->ProcessFrame(f++, 30);
    assert(meta.MasterSequence == ++m);

    // Mid-cycle switch back to Profile 0
    meta = provider->ProcessFrame(f++, 10);
    assert(meta.MasterSequence == ++m);  // Should advance to window 4
    assert(meta.HdrProfile == 0);
    assert(meta.ExposureSequenceIndex == 0);

    std::cout << "✓ Mid-cycle switches test passed" << std::endl;
}

int main() {
    std::cout << "Running HDR Metadata Provider C++ Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        test_window_size_1vs1();
        test_window_size_2vs3();
        test_duplicate_exposures();
        test_extensive_switching();
        test_extreme_gaps();
        test_mid_cycle_switches();

        std::cout << "========================================" << std::endl;
        std::cout << "All tests passed successfully! ✓" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with error: " << e.what() << std::endl;
        return 1;
    }
}