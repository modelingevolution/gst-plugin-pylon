#!/bin/bash
# Simple test for HDR sequence feature

echo "Testing HDR sequence with gst-launch-1.0..."
echo ""
echo "Command: gst-launch-1.0 pylonsrc hdr-sequence=\"19,150\" num-buffers=100 ! videoconvert ! autovideosink"
echo ""

# Run with debug output to see our HDR sequence logs
GST_DEBUG=pylonsrc:4,pylon:4 gst-launch-1.0 \
    pylonsrc hdr-sequence="19,150" num-buffers=100 ! \
    videoconvert ! \
    autovideosink

echo ""
echo "Pipeline completed."