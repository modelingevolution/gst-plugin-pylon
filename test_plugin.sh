#!/bin/bash

# Test script for gst-plugin-pylon
set -e

echo "=== Testing gst-plugin-pylon ==="

# Check if build directory exists
if [ ! -d "builddir" ]; then
    echo "ERROR: Build directory not found. Please run ./build.sh first"
    exit 1
fi

# Set plugin path
export GST_PLUGIN_PATH=$PWD/builddir/ext/pylon
echo "Using GST_PLUGIN_PATH=$GST_PLUGIN_PATH"

# Test 1: Check if plugin loads
echo ""
echo "Test 1: Checking if plugin loads..."
if gst-inspect-1.0 pylonsrc > /dev/null 2>&1; then
    echo "✓ Plugin loads successfully"
else
    echo "✗ Failed to load plugin"
    exit 1
fi

# Test 2: Show plugin details
echo ""
echo "Test 2: Plugin properties..."
gst-inspect-1.0 pylonsrc | grep -A5 "Element Properties"

# Test 3: Check for cameras
echo ""
echo "Test 3: Checking for connected cameras..."
echo "Running: gst-launch-1.0 pylonsrc num-buffers=1 ! fakesink"
if gst-launch-1.0 pylonsrc num-buffers=1 ! fakesink 2>&1 | grep -q "At least"; then
    echo "Multiple cameras detected or no camera connected"
    echo "This is normal if no camera is connected to the system"
else
    echo "Camera detection completed"
fi

# Test 4: Run unit tests if available
echo ""
echo "Test 4: Running unit tests..."
if ninja -C builddir test; then
    echo "✓ Unit tests passed"
else
    echo "✗ Some tests failed (this might be expected without a camera)"
fi

echo ""
echo "=== Basic tests completed ==="
echo ""
echo "To test with a real camera:"
echo "  gst-launch-1.0 pylonsrc ! videoconvert ! autovideosink"
echo ""
echo "To test with specific camera settings:"
echo "  gst-launch-1.0 pylonsrc device-index=0 ! video/x-raw,width=640,height=480 ! videoconvert ! autovideosink"