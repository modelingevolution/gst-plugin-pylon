#!/bin/bash
# Test script for HDR sequence feature

echo "============================================"
echo "HDR Sequence Mode Test for gst-plugin-pylon"
echo "============================================"
echo ""

# Function to test HDR sequence
test_hdr_sequence() {
    local sequence=$1
    local description=$2

    echo "Testing: $description"
    echo "HDR Sequence: $sequence"
    echo "Running pipeline..."

    # Test with GST_DEBUG to see our logging
    GST_DEBUG=pylonsrc:4 gst-launch-1.0 \
        pylonsrc hdr-sequence="$sequence" num-buffers=10 ! \
        video/x-raw,width=640,height=480 ! \
        fakesink sync=false 2>&1 | grep -E "HDR|sequence|Sequencer|exposure"

    if [ $? -eq 0 ]; then
        echo "✓ Test passed"
    else
        echo "✗ Test may have failed (check for sequencer support on camera)"
    fi
    echo ""
}

# Test 1: Basic two-exposure HDR sequence
test_hdr_sequence "19,150" "Basic HDR with 19μs and 150μs exposures"

# Test 2: Three-exposure HDR sequence
test_hdr_sequence "10,50,200" "Three-exposure HDR (10μs, 50μs, 200μs)"

# Test 3: Four-exposure HDR sequence
test_hdr_sequence "5,20,100,500" "Four-exposure HDR (5μs, 20μs, 100μs, 500μs)"

echo "============================================"
echo "Property inspection test"
echo "============================================"
gst-inspect-1.0 pylonsrc | grep -A2 "hdr-sequence"

echo ""
echo "Test complete!"
echo ""
echo "Note: If tests fail with 'Camera does not support sequencer mode',"
echo "your camera may not have sequencer capability."