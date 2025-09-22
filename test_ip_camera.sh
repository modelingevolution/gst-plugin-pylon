#!/bin/bash

# Test script for IP camera support in gst-plugin-pylon
set -e

echo "=== Testing IP Camera Support ==="

# Check if build directory exists
if [ ! -d "builddir" ]; then
    echo "ERROR: Build directory not found. Please run ./build.sh first"
    exit 1
fi

# Set plugin path
export GST_PLUGIN_PATH=$PWD/builddir/ext/pylon
echo "Using GST_PLUGIN_PATH=$GST_PLUGIN_PATH"

echo ""
echo "Checking if device-ip-address property is available..."
if gst-inspect-1.0 pylonsrc 2>/dev/null | grep -q "device-ip-address"; then
    echo "✓ device-ip-address property found"
    gst-inspect-1.0 pylonsrc | grep -A2 "device-ip-address"
else
    echo "✗ device-ip-address property not found"
    echo "Make sure the plugin was built with the IP address support"
    exit 1
fi

echo ""
echo "=== Usage Examples ==="
echo ""
echo "To connect to a GigE camera by IP address:"
echo "  gst-launch-1.0 pylonsrc device-ip-address=\"192.168.1.100\" ! videoconvert ! autovideosink"
echo ""
echo "To list available cameras (including GigE):"
echo "  gst-launch-1.0 pylonsrc ! fakesink"
echo ""
echo "To combine IP with other filters:"
echo "  gst-launch-1.0 pylonsrc device-ip-address=\"192.168.1.100\" device-index=0 ! videoconvert ! autovideosink"
echo ""
echo "=== Testing IP address property (dry run) ==="
echo "This will attempt to connect to IP 192.168.1.100 (will fail if no camera at this address):"
echo ""
echo "Command: gst-launch-1.0 pylonsrc device-ip-address=\"192.168.1.100\" num-buffers=1 ! fakesink"
echo ""
echo "Running test (Ctrl+C to cancel)..."
echo ""

# Try to connect (this will fail if no camera, but tests the property)
set +e
timeout 5 gst-launch-1.0 pylonsrc device-ip-address="192.168.1.100" num-buffers=1 ! fakesink 2>&1 | head -20

echo ""
echo "=== IP Camera Configuration Tips ==="
echo ""
echo "1. Ensure camera and host are on same subnet:"
echo "   Host: 192.168.1.1/24"
echo "   Camera: 192.168.1.100/24"
echo ""
echo "2. Use Basler pylon IP Configurator to set camera IP"
echo ""
echo "3. Test connection with pylon Viewer first"
echo ""
echo "4. For better performance with GigE cameras:"
echo "   - Enable jumbo frames (MTU 9000)"
echo "   - Use dedicated network adapter"
echo "   - Set stream::MaxTransferSize for USB3/GigE optimization"