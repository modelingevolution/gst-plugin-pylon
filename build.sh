#!/bin/bash

# Build script for gst-plugin-pylon
set -e

echo "=== Building gst-plugin-pylon ==="

# Check if pylon is installed
if [ ! -d "/opt/pylon" ]; then
    echo "ERROR: Pylon SDK not found in /opt/pylon"
    echo "Please install Basler pylon SDK first:"
    echo "  1. Download from: https://www.baslerweb.com/en/downloads/software-downloads/"
    echo "  2. Install the .deb package with: sudo dpkg -i pylon_*.deb"
    exit 1
fi

# Set pylon root
export PYLON_ROOT=/opt/pylon
echo "Using PYLON_ROOT=$PYLON_ROOT"

# Clean previous build if exists
if [ -d "builddir" ]; then
    echo "Removing existing build directory..."
    rm -rf builddir
fi

# Configure with meson
echo "Configuring build with meson..."
meson setup builddir --prefix /usr/ \
    -Dpython-bindings=disabled \
    -Dtests=enabled \
    -Dexamples=enabled

# Build
echo "Building with ninja..."
ninja -C builddir

# Show build result
echo "=== Build completed ==="
echo "Plugin location: builddir/ext/pylon/"
echo ""
echo "To test the plugin without installing:"
echo "  export GST_PLUGIN_PATH=\$PWD/builddir/ext/pylon"
echo "  gst-inspect-1.0 pylonsrc"
echo ""
echo "To run tests:"
echo "  ninja -C builddir test"
echo ""
echo "To install system-wide:"
echo "  sudo ninja -C builddir install"