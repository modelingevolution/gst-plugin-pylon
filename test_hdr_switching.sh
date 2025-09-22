#!/bin/bash
# Quick test script for HDR profile switching

echo "============================================"
echo "HDR Profile Switching Test"
echo "============================================"
echo ""

export GST_PLUGIN_PATH=$PWD/builddir/ext/pylon

echo "Testing dual HDR profile configuration and switching..."
echo ""

# Test with debug output to see what's happening
GST_DEBUG=pylonsrc:4,pylon:4 timeout 10 gst-launch-1.0 \
    pylonsrc name=src \
    hdr-sequence="19,120" \
    hdr-sequence2="5000,10000" \
    hdr-profile=0 \
    num-buffers=100 ! \
    video/x-raw,width=640,height=480 ! \
    fakesink sync=false 2>&1 | grep -E "profile|Profile|HDR|Switch|Software|Signal" &

# Let it run for a bit
sleep 3

# Try to switch profile dynamically
echo ""
echo "Attempting to switch to Profile 1..."
gst-launch-1.0 --gst-debug-level=0 \
    pylonsrc name=src \
    hdr-sequence="19,120" \
    hdr-sequence2="5000,10000" \
    hdr-profile=1 \
    num-buffers=10 ! \
    fakesink 2>&1 | grep -E "profile|Profile"

echo ""
echo "Test complete!"
echo ""
echo "If you see 'Switched to HDR Profile' messages, the feature is working."
echo "If you see warnings about software signals not available, your camera"
echo "may not support this feature or may use different signal names."