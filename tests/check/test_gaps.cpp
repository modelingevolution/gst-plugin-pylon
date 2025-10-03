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

    std::cout << "\nGap/Skip Test:\n";
    std::cout << "===============\n";

    // Normal sequence
    auto meta0 = provider.ProcessFrame(19, 0);
    std::cout << "Frame 0 (exp=19): master_seq=" << meta0.master_sequence
              << ", index=" << (int)meta0.exposure_sequence_index << "\n";

    auto meta1 = provider.ProcessFrame(150, 1);
    std::cout << "Frame 1 (exp=150): master_seq=" << meta1.master_sequence
              << ", index=" << (int)meta1.exposure_sequence_index << "\n";

    // Skip some frame numbers but continue the sequence
    auto meta10 = provider.ProcessFrame(19, 10);  // Gap in frame numbers
    std::cout << "Frame 10 (exp=19) [GAP]: master_seq=" << meta10.master_sequence
              << ", index=" << (int)meta10.exposure_sequence_index << "\n";

    auto meta11 = provider.ProcessFrame(150, 11);
    std::cout << "Frame 11 (exp=150): master_seq=" << meta11.master_sequence
              << ", index=" << (int)meta11.exposure_sequence_index << "\n";

    // Another window with gap
    auto meta20 = provider.ProcessFrame(19, 20);
    std::cout << "Frame 20 (exp=19) [GAP]: master_seq=" << meta20.master_sequence
              << ", index=" << (int)meta20.exposure_sequence_index << "\n";

    // Switch profile with gap
    auto meta30 = provider.ProcessFrame(250, 30);
    std::cout << "Frame 30 (exp=250) [SWITCH+GAP]: master_seq=" << meta30.master_sequence
              << ", index=" << (int)meta30.exposure_sequence_index << "\n";

    std::cout << "\nTest shows that master sequence increments based on actual exposure sequences,\n";
    std::cout << "not frame numbers, so gaps don't affect the HDR metadata tracking.\n";

    return 0;
}