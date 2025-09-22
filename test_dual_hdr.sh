#!/bin/bash
# Test script for dual HDR profile feature

echo "============================================"
echo "Dual HDR Profile Test for gst-plugin-pylon"
echo "============================================"
echo ""

# Function to test dual HDR profiles with switching
test_dual_hdr() {
    echo "Testing dual HDR profiles configuration"
    echo "========================================"
    echo "Profile 0: 19,120 μs"
    echo "Profile 1: 5000,10000 μs"
    echo ""

    # Test 1: Start with Profile 0
    echo "Test 1: Starting with Profile 0"
    echo "--------------------------------"
    timeout 5 gst-launch-1.0 \
        pylonsrc hdr-sequence="19,120" hdr-sequence2="5000,10000" hdr-profile=0 num-buffers=20 ! \
        video/x-raw,width=640,height=480 ! \
        fakesink sync=false 2>&1 | grep -E "HDR|profile|Profile|sequence"

    echo ""

    # Test 2: Start with Profile 1
    echo "Test 2: Starting with Profile 1"
    echo "--------------------------------"
    timeout 5 gst-launch-1.0 \
        pylonsrc hdr-sequence="19,120" hdr-sequence2="5000,10000" hdr-profile=1 num-buffers=20 ! \
        video/x-raw,width=640,height=480 ! \
        fakesink sync=false 2>&1 | grep -E "HDR|profile|Profile|sequence"

    echo ""
}

# Function to test runtime switching
test_runtime_switching() {
    echo "Testing runtime profile switching"
    echo "================================="
    echo ""

    cat > /tmp/test_hdr_switch.py << 'EOF'
#!/usr/bin/env python3
import gi
gi.require_version('Gst', '1.0')
from gi.repository import Gst, GLib
import time
import threading

Gst.init(None)

# Create pipeline
pipeline = Gst.parse_launch(
    'pylonsrc name=src hdr-sequence="19,120" hdr-sequence2="5000,10000" hdr-profile=0 ! '
    'video/x-raw,width=640,height=480 ! '
    'fakesink sync=false'
)

src = pipeline.get_by_name('src')

def switch_profiles():
    """Switch profiles during streaming"""
    time.sleep(2)
    print("Switching to Profile 1...")
    src.set_property('hdr-profile', 1)

    time.sleep(2)
    print("Switching back to Profile 0...")
    src.set_property('hdr-profile', 0)

    time.sleep(2)
    print("Switching to Profile 1 again...")
    src.set_property('hdr-profile', 1)

    time.sleep(2)
    print("Test complete, stopping pipeline...")
    pipeline.send_event(Gst.Event.new_eos())

# Start the switching thread
switch_thread = threading.Thread(target=switch_profiles)
switch_thread.daemon = True

# Set up bus watch
bus = pipeline.get_bus()
bus.add_signal_watch()

def on_message(bus, message):
    t = message.type
    if t == Gst.MessageType.EOS:
        print("End-of-stream")
        loop.quit()
    elif t == Gst.MessageType.ERROR:
        err, debug = message.parse_error()
        print(f"Error: {err}, {debug}")
        loop.quit()

bus.connect("message", on_message)

# Create main loop
loop = GLib.MainLoop()

# Start pipeline and switching thread
print("Starting pipeline with Profile 0...")
pipeline.set_state(Gst.State.PLAYING)
switch_thread.start()

try:
    loop.run()
except KeyboardInterrupt:
    pass

# Cleanup
pipeline.set_state(Gst.State.NULL)
EOF

    if command -v python3 &> /dev/null && python3 -c "import gi" 2>/dev/null; then
        echo "Running Python test for runtime switching..."
        python3 /tmp/test_hdr_switch.py
    else
        echo "Python3 with GStreamer bindings not available."
        echo "Skipping runtime switching test."
    fi

    rm -f /tmp/test_hdr_switch.py
}

# Function to inspect properties
inspect_properties() {
    echo "Property inspection"
    echo "==================="
    echo ""
    gst-inspect-1.0 pylonsrc 2>/dev/null | grep -E "hdr-sequence|hdr-profile" -A2
}

# Main test execution
echo "Starting dual HDR profile tests..."
echo ""

# Test basic dual HDR configuration
test_dual_hdr

echo ""
echo "============================================"
echo ""

# Test runtime switching
test_runtime_switching

echo ""
echo "============================================"
echo ""

# Inspect properties
inspect_properties

echo ""
echo "============================================"
echo "Dual HDR profile tests complete!"
echo ""
echo "Note: If tests fail with 'Camera does not support sequencer mode',"
echo "your camera may not have the required sequencer capabilities."
echo ""
echo "For cameras with sequencer support, you should see:"
echo "- Successful configuration of both profiles"
echo "- Runtime switching between profiles"
echo "- Smooth transitions without frame drops"