#!/bin/bash
# Test script to show exposure time debug output for HDR sequences

echo "============================================"
echo "HDR Exposure Time Debug Test"
echo "============================================"
echo ""

export GST_PLUGIN_PATH=$PWD/builddir/ext/pylon

echo "Testing HDR sequence with exposure time debug output..."
echo "Profile 0: 19μs, 150μs"
echo "Profile 1: 250μs, 350μs"
echo ""
echo "Watch the debug output for exposure times..."
echo ""

# Run with GST_DEBUG set to see our exposure time output
# Using INFO level (4) to see the main exposure messages
echo "Running pipeline with automatic chunk enabling..."
echo ""

GST_DEBUG=pylonsrc:4,pylon:4 gst-launch-1.0 \
    pylonsrc \
    hdr-sequence="19,150" \
    hdr-sequence2="250,350" \
    hdr-profile=0 \
    num-buffers=20 ! \
    video/x-raw,width=640,height=480 ! \
    fakesink sync=false 2>&1 | grep -E "ChunkModeActive|ChunkEnable|ExposureTime|HDR Frame|exposure|Frame [0-9]+ captured|chunk"

echo ""
echo "Test complete! Check the output above to see exposure times for each frame."
echo "You should see the exposure values cycling through the sequence."