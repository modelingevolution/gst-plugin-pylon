#include <iostream>
#include <vector>
#include <gst/gst.h>
#include "../../gst-libs/gst/pylon/gsthdrmetadataprovider.h"

int main(int argc, char **argv) {
    gst_init(&argc, &argv);

    HdrMetadataProvider provider;
    std::vector<guint32> profile0 = {50};  // Single exposure

    provider.SetProfile0Sequence(profile0);

    std::cout << "\nSingle Exposure Debug Test:\n";
    std::cout << "============================\n";

    // Each frame should be a complete window
    auto meta0 = provider.ProcessFrame(50, 0);
    std::cout << "Frame 0 (exp=50): master_seq=" << meta0.master_sequence
              << ", index=" << (int)meta0.exposure_sequence_index
              << ", count=" << (int)meta0.exposure_count << "\n";

    // Gap - but with single exposure, this should be a new window
    auto meta10 = provider.ProcessFrame(50, 10);
    std::cout << "Frame 10 (exp=50) [GAP]: master_seq=" << meta10.master_sequence
              << ", index=" << (int)meta10.exposure_sequence_index
              << " [Expected master_seq=1]\n";

    auto meta20 = provider.ProcessFrame(50, 20);
    std::cout << "Frame 20 (exp=50) [GAP]: master_seq=" << meta20.master_sequence
              << ", index=" << (int)meta20.exposure_sequence_index
              << " [Expected master_seq=2]\n";

    return 0;
}