#!/bin/bash
# Diagnostic script to check sequencer capabilities

echo "============================================"
echo "Camera Sequencer Feature Diagnostic"
echo "============================================"
echo ""

# Create a Python script to check sequencer features using pypylon
cat > /tmp/check_sequencer.py << 'EOF'
#!/usr/bin/env python3
import sys

try:
    from pypylon import pylon

    print("Checking camera sequencer features...\n")

    # Create camera object
    camera = pylon.InstantCamera(pylon.TlFactory.GetInstance().CreateFirstDevice())
    camera.Open()

    print(f"Connected to: {camera.GetDeviceInfo().GetModelName()}")
    print(f"Serial Number: {camera.GetDeviceInfo().GetSerialNumber()}\n")

    # List of sequencer-related features to check
    features_to_check = [
        "SequencerMode",
        "SequencerConfigurationMode",
        "SequencerTriggerSource",
        "SequencerSetSelector",
        "SequencerSetStart",
        "SequencerSetNext",
        "ExposureTime",
        "ExposureTimeAbs",
        "ExposureTimeRaw",
    ]

    print("Sequencer Features Status:")
    print("-" * 40)

    for feature_name in features_to_check:
        try:
            # Try to access as a camera parameter
            if hasattr(camera, feature_name):
                param = getattr(camera, feature_name)
                status = "Available"

                # Try to get value
                try:
                    if hasattr(param, 'GetValue'):
                        value = param.GetValue()
                        status += f" - Current: {value}"

                    # For enumerations, get possible values
                    if hasattr(param, 'GetSymbolics'):
                        symbols = param.GetSymbolics()
                        status += f" - Options: [{', '.join(symbols)}]"

                    # For numeric parameters, get range
                    if hasattr(param, 'GetMin') and hasattr(param, 'GetMax'):
                        min_val = param.GetMin()
                        max_val = param.GetMax()
                        status += f" (Range: {min_val}-{max_val})"

                except:
                    pass

                print(f"✓ {feature_name}: {status}")
            else:
                # Try alternative method
                try:
                    node = camera.GetNodeMap().GetNode(feature_name)
                    if node:
                        print(f"✓ {feature_name}: Available (as node)")
                    else:
                        print(f"✗ {feature_name}: Not found")
                except:
                    print(f"✗ {feature_name}: Not found")
        except Exception as e:
            print(f"✗ {feature_name}: Error - {e}")

    camera.Close()

    print("\n✓ Diagnostic complete!")

except ImportError:
    print("ERROR: pypylon is not installed. Install it with:")
    print("  pip install pypylon")
    sys.exit(1)
except Exception as e:
    print(f"ERROR: {e}")
    sys.exit(1)
EOF

# Try to run with pypylon if available
# First check if venv exists and has pypylon
if [ -f "venv/bin/python" ] && venv/bin/python -c "import pypylon" 2>/dev/null; then
    echo "Using virtual environment with pypylon..."
    venv/bin/python /tmp/check_sequencer.py
elif command -v python3 &> /dev/null; then
    python3 /tmp/check_sequencer.py
else
    echo "Python3 not found. Trying with gst-launch instead..."
    echo ""

    # Fallback: Use GST debug output to see what happens
    echo "Running with GST_DEBUG to capture sequencer configuration attempts:"
    echo ""

    GST_DEBUG=pylonsrc:5,pylon:5 gst-launch-1.0 \
        pylonsrc hdr-sequence="19,150" num-buffers=1 ! \
        fakesink 2>&1 | grep -E "Sequencer|sequencer|trigger|Trigger|Available|exposure"
fi

echo ""
echo "============================================"
echo "Testing the HDR sequence property:"
echo "============================================"
gst-inspect-1.0 pylonsrc 2>/dev/null | grep -A3 "hdr-sequence"

rm -f /tmp/check_sequencer.py