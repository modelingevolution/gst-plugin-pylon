/* Copyright (C) 2024 Basler AG
 *
 * Unit tests for HdrMetadataProvider
 */

#include <gtest/gtest.h>
#include <gst/gst.h>
#include "../../gst-libs/gst/pylon/gsthdrmetadataprovider.h"

class HdrMetadataProviderTest : public ::testing::Test {
protected:
    void SetUp() override {
        provider = new HdrMetadataProvider();
    }

    void TearDown() override {
        delete provider;
    }

    HdrMetadataProvider* provider;
};

// Test basic configuration
TEST_F(HdrMetadataProviderTest, BasicConfiguration) {
    EXPECT_FALSE(provider->IsConfigured());

    std::vector<guint32> profile0 = {19, 150};
    provider->SetProfile0Sequence(profile0);

    EXPECT_TRUE(provider->IsConfigured());
    EXPECT_EQ(provider->GetActiveProfile(), 0);
    EXPECT_EQ(provider->GetMasterSequence(), 0);
}

// Test single profile sequence
TEST_F(HdrMetadataProviderTest, SingleProfileSequence) {
    std::vector<guint32> profile0 = {19, 150};
    provider->SetProfile0Sequence(profile0);

    // First window
    auto meta0 = provider->ProcessFrame(19, 0);
    EXPECT_EQ(meta0.master_sequence, 0);
    EXPECT_EQ(meta0.exposure_sequence_index, 0);
    EXPECT_EQ(meta0.exposure_count, 2);
    EXPECT_EQ(meta0.exposure_value, 19);
    EXPECT_EQ(meta0.hdr_profile, 0);

    auto meta1 = provider->ProcessFrame(150, 1);
    EXPECT_EQ(meta1.master_sequence, 0);
    EXPECT_EQ(meta1.exposure_sequence_index, 1);
    EXPECT_EQ(meta1.exposure_count, 2);
    EXPECT_EQ(meta1.exposure_value, 150);
    EXPECT_EQ(meta1.hdr_profile, 0);

    // Second window
    auto meta2 = provider->ProcessFrame(19, 2);
    EXPECT_EQ(meta2.master_sequence, 1);
    EXPECT_EQ(meta2.exposure_sequence_index, 0);
    EXPECT_EQ(meta2.exposure_count, 2);
    EXPECT_EQ(meta2.hdr_profile, 0);

    auto meta3 = provider->ProcessFrame(150, 3);
    EXPECT_EQ(meta3.master_sequence, 1);
    EXPECT_EQ(meta3.exposure_sequence_index, 1);
    EXPECT_EQ(meta3.hdr_profile, 0);
}

// Test dual profile configuration
TEST_F(HdrMetadataProviderTest, DualProfileConfiguration) {
    std::vector<guint32> profile0 = {19, 150};
    std::vector<guint32> profile1 = {250, 350, 450};

    provider->SetProfile0Sequence(profile0);
    provider->SetProfile1Sequence(profile1);

    EXPECT_TRUE(provider->IsConfigured());
}

// Test profile switching
TEST_F(HdrMetadataProviderTest, ProfileSwitching) {
    std::vector<guint32> profile0 = {19, 150};
    std::vector<guint32> profile1 = {250, 350, 450};

    provider->SetProfile0Sequence(profile0);
    provider->SetProfile1Sequence(profile1);

    // Start with profile 0
    auto meta0 = provider->ProcessFrame(19, 0);
    EXPECT_EQ(meta0.hdr_profile, 0);
    EXPECT_EQ(meta0.master_sequence, 0);

    auto meta1 = provider->ProcessFrame(150, 1);
    EXPECT_EQ(meta1.hdr_profile, 0);
    EXPECT_EQ(meta1.master_sequence, 0);

    // Switch to profile 1 - master sequence should increment
    auto meta2 = provider->ProcessFrame(250, 2);
    EXPECT_EQ(meta2.hdr_profile, 1);
    EXPECT_EQ(meta2.master_sequence, 1);
    EXPECT_EQ(meta2.exposure_sequence_index, 0);
    EXPECT_EQ(meta2.exposure_count, 3);

    auto meta3 = provider->ProcessFrame(350, 3);
    EXPECT_EQ(meta3.hdr_profile, 1);
    EXPECT_EQ(meta3.master_sequence, 1);
    EXPECT_EQ(meta3.exposure_sequence_index, 1);

    auto meta4 = provider->ProcessFrame(450, 4);
    EXPECT_EQ(meta4.hdr_profile, 1);
    EXPECT_EQ(meta4.master_sequence, 1);
    EXPECT_EQ(meta4.exposure_sequence_index, 2);

    // Complete profile 1 window and start new one
    auto meta5 = provider->ProcessFrame(250, 5);
    EXPECT_EQ(meta5.hdr_profile, 1);
    EXPECT_EQ(meta5.master_sequence, 2);
    EXPECT_EQ(meta5.exposure_sequence_index, 0);

    // Switch back to profile 0
    auto meta6 = provider->ProcessFrame(19, 6);
    EXPECT_EQ(meta6.hdr_profile, 0);
    EXPECT_EQ(meta6.master_sequence, 3); // Incremented due to switch
    EXPECT_EQ(meta6.exposure_sequence_index, 0);
}

// Test duplicate exposure handling
TEST_F(HdrMetadataProviderTest, DuplicateExposureHandling) {
    std::vector<guint32> profile0 = {100, 200};
    std::vector<guint32> profile1 = {100, 300}; // 100 is duplicate

    provider->SetProfile0Sequence(profile0);
    provider->SetProfile1Sequence(profile1);

    // Profile 0 should get the original 100
    auto meta0 = provider->ProcessFrame(100, 0);
    EXPECT_EQ(meta0.hdr_profile, 0);
    EXPECT_EQ(meta0.exposure_sequence_index, 0);

    // Profile 1's duplicate should be detected even with adjusted value
    // The provider should recognize 101 as the adjusted duplicate
    auto meta1 = provider->ProcessFrame(101, 2);
    EXPECT_EQ(meta1.hdr_profile, 1);
    EXPECT_EQ(meta1.exposure_sequence_index, 0);
}

// Test variable length sequences
TEST_F(HdrMetadataProviderTest, VariableLengthSequences) {
    std::vector<guint32> profile0 = {10, 30, 90};      // 3 exposures
    std::vector<guint32> profile1 = {100, 200};        // 2 exposures

    provider->SetProfile0Sequence(profile0);
    provider->SetProfile1Sequence(profile1);

    // Test profile 0 (3 exposures)
    auto meta0 = provider->ProcessFrame(10, 0);
    EXPECT_EQ(meta0.exposure_count, 3);
    EXPECT_EQ(meta0.exposure_sequence_index, 0);

    auto meta1 = provider->ProcessFrame(30, 1);
    EXPECT_EQ(meta1.exposure_count, 3);
    EXPECT_EQ(meta1.exposure_sequence_index, 1);

    auto meta2 = provider->ProcessFrame(90, 2);
    EXPECT_EQ(meta2.exposure_count, 3);
    EXPECT_EQ(meta2.exposure_sequence_index, 2);
    EXPECT_EQ(meta2.master_sequence, 0);

    // New window in profile 0
    auto meta3 = provider->ProcessFrame(10, 3);
    EXPECT_EQ(meta3.master_sequence, 1);

    // Switch to profile 1 (2 exposures)
    auto meta4 = provider->ProcessFrame(100, 4);
    EXPECT_EQ(meta4.exposure_count, 2);
    EXPECT_EQ(meta4.exposure_sequence_index, 0);
    EXPECT_EQ(meta4.hdr_profile, 1);
    EXPECT_EQ(meta4.master_sequence, 2); // Incremented due to switch
}

// Test single exposure profiles
TEST_F(HdrMetadataProviderTest, SingleExposureProfiles) {
    std::vector<guint32> profile0 = {50};
    std::vector<guint32> profile1 = {200};

    provider->SetProfile0Sequence(profile0);
    provider->SetProfile1Sequence(profile1);

    // Each frame is a complete window with single exposure
    auto meta0 = provider->ProcessFrame(50, 0);
    EXPECT_EQ(meta0.exposure_count, 1);
    EXPECT_EQ(meta0.exposure_sequence_index, 0);
    EXPECT_EQ(meta0.master_sequence, 0);

    auto meta1 = provider->ProcessFrame(50, 1);
    EXPECT_EQ(meta1.master_sequence, 1);

    auto meta2 = provider->ProcessFrame(50, 2);
    EXPECT_EQ(meta2.master_sequence, 2);

    // Switch to profile 1
    auto meta3 = provider->ProcessFrame(200, 3);
    EXPECT_EQ(meta3.hdr_profile, 1);
    EXPECT_EQ(meta3.master_sequence, 3); // Incremented due to switch
}

// Test reset functionality
TEST_F(HdrMetadataProviderTest, ResetFunctionality) {
    std::vector<guint32> profile0 = {19, 150};
    provider->SetProfile0Sequence(profile0);

    auto meta0 = provider->ProcessFrame(19, 0);
    auto meta1 = provider->ProcessFrame(150, 1);
    auto meta2 = provider->ProcessFrame(19, 2);
    EXPECT_EQ(meta2.master_sequence, 1);

    // Reset should clear everything
    provider->Reset();
    EXPECT_FALSE(provider->IsConfigured());
    EXPECT_EQ(provider->GetMasterSequence(), 0);

    // Reconfigure and verify reset worked
    provider->SetProfile0Sequence(profile0);
    auto meta3 = provider->ProcessFrame(19, 0);
    EXPECT_EQ(meta3.master_sequence, 0); // Should start from 0 again
}

// Test master sequence continuity across profile switches
TEST_F(HdrMetadataProviderTest, MasterSequenceContinuity) {
    std::vector<guint32> profile0 = {19, 150};
    std::vector<guint32> profile1 = {250, 350};

    provider->SetProfile0Sequence(profile0);
    provider->SetProfile1Sequence(profile1);

    guint64 frame = 0;

    // Profile 0, window 0
    provider->ProcessFrame(19, frame++);
    auto meta = provider->ProcessFrame(150, frame++);
    EXPECT_EQ(meta.master_sequence, 0);

    // Profile 0, window 1
    provider->ProcessFrame(19, frame++);
    meta = provider->ProcessFrame(150, frame++);
    EXPECT_EQ(meta.master_sequence, 1);

    // Switch to Profile 1 - master sequence increments
    meta = provider->ProcessFrame(250, frame++);
    EXPECT_EQ(meta.master_sequence, 2);

    provider->ProcessFrame(350, frame++);

    // Profile 1, window 2
    provider->ProcessFrame(250, frame++);
    meta = provider->ProcessFrame(350, frame++);
    EXPECT_EQ(meta.master_sequence, 3);

    // Switch back to Profile 0 - master sequence increments again
    meta = provider->ProcessFrame(19, frame++);
    EXPECT_EQ(meta.master_sequence, 4);
}

// Test unknown exposure handling
TEST_F(HdrMetadataProviderTest, UnknownExposureHandling) {
    std::vector<guint32> profile0 = {19, 150};
    provider->SetProfile0Sequence(profile0);

    // Process an unknown exposure value - should log error and return fallback
    auto meta = provider->ProcessFrame(999, 0);
    // Fallback behavior: returns profile 0, index 0 but logs GST_ERROR
    EXPECT_EQ(meta.hdr_profile, 0);
    EXPECT_EQ(meta.exposure_sequence_index, 0);
    EXPECT_EQ(meta.exposure_value, 999);
    // Note: In production, this should never happen - indicates configuration mismatch
}

// Test with maximum sequences
TEST_F(HdrMetadataProviderTest, MaximumSequences) {
    // Test with 8 exposures per profile (16 total)
    std::vector<guint32> profile0 = {10, 20, 30, 40, 50, 60, 70, 80};
    std::vector<guint32> profile1 = {100, 110, 120, 130, 140, 150, 160, 170};

    provider->SetProfile0Sequence(profile0);
    provider->SetProfile1Sequence(profile1);

    // Process through profile 0
    for (guint32 i = 0; i < 8; i++) {
        auto meta = provider->ProcessFrame(profile0[i], i);
        EXPECT_EQ(meta.exposure_sequence_index, i);
        EXPECT_EQ(meta.exposure_count, 8);
        EXPECT_EQ(meta.hdr_profile, 0);
    }

    // Next window
    auto meta = provider->ProcessFrame(10, 8);
    EXPECT_EQ(meta.master_sequence, 1);
}

int main(int argc, char **argv) {
    // Initialize GStreamer (needed for GST_WARNING, GST_INFO macros)
    gst_init(&argc, &argv);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}