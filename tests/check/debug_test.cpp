#include <iostream>
#include <vector>
#include <gst/gst.h>
#include "../../gst-libs/gst/pylon/gsthdrmetadataprovider.h"

int main(int argc, char **argv) {
    gst_init(&argc, &argv);

    HdrMetadataProvider provider;
    std::vector<guint32> profile0 = {19, 150};
    std::vector<guint32> profile1 = {250, 350, 450};

    provider.SetProfile0Sequence(profile0);
    provider.SetProfile1Sequence(profile1);

    std::cout << "\nProfile Switching Debug Test:\n";
    std::cout << "==============================\n";

    // Start with profile 0
    auto meta0 = provider.ProcessFrame(19, 0);
    std::cout << "Frame 0 (exp=19): profile=" << (int)meta0.hdr_profile
              << ", master_seq=" << meta0.master_sequence
              << ", index=" << (int)meta0.exposure_sequence_index << "\n";

    auto meta1 = provider.ProcessFrame(150, 1);
    std::cout << "Frame 1 (exp=150): profile=" << (int)meta1.hdr_profile
              << ", master_seq=" << meta1.master_sequence
              << ", index=" << (int)meta1.exposure_sequence_index << "\n";

    // Switch to profile 1 - master sequence should increment
    auto meta2 = provider.ProcessFrame(250, 2);
    std::cout << "Frame 2 (exp=250) SWITCH: profile=" << (int)meta2.hdr_profile
              << ", master_seq=" << meta2.master_sequence
              << ", index=" << (int)meta2.exposure_sequence_index
              << " [Expected master_seq=1]\n";

    auto meta3 = provider.ProcessFrame(350, 3);
    std::cout << "Frame 3 (exp=350): profile=" << (int)meta3.hdr_profile
              << ", master_seq=" << meta3.master_sequence
              << ", index=" << (int)meta3.exposure_sequence_index << "\n";

    auto meta4 = provider.ProcessFrame(450, 4);
    std::cout << "Frame 4 (exp=450): profile=" << (int)meta4.hdr_profile
              << ", master_seq=" << meta4.master_sequence
              << ", index=" << (int)meta4.exposure_sequence_index << "\n";

    // Complete profile 1 window and start new one
    auto meta5 = provider.ProcessFrame(250, 5);
    std::cout << "Frame 5 (exp=250) NEW WINDOW: profile=" << (int)meta5.hdr_profile
              << ", master_seq=" << meta5.master_sequence
              << ", index=" << (int)meta5.exposure_sequence_index
              << " [Expected master_seq=2]\n";

    // Switch back to profile 0
    auto meta6 = provider.ProcessFrame(19, 6);
    std::cout << "Frame 6 (exp=19) SWITCH: profile=" << (int)meta6.hdr_profile
              << ", master_seq=" << meta6.master_sequence
              << ", index=" << (int)meta6.exposure_sequence_index
              << " [Expected master_seq=3]\n";

    return 0;
}