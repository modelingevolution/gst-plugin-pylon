#!/usr/bin/env python3
"""
Check which parameters need to be set in each sequencer set
"""

from pypylon import pylon

try:
    camera = pylon.InstantCamera(pylon.TlFactory.GetInstance().CreateFirstDevice())
    camera.Open()

    print(f"Connected to: {camera.GetDeviceInfo().GetModelName()}")
    print(f"Serial Number: {camera.GetDeviceInfo().GetSerialNumber()}\n")

    # Parameters that might need to be preserved in sequencer sets
    params_to_check = [
        "Width",
        "Height",
        "PixelFormat",
        "Gain",
        "GainRaw",
        "BlackLevel",
        "Gamma",
        "ExposureTime",
        "BinningHorizontal",
        "BinningVertical",
        "DecimationHorizontal",
        "DecimationVertical",
        "OffsetX",
        "OffsetY"
    ]

    print("Checking which parameters are affected by sequencer sets...")
    print("-" * 60)

    # Store original values
    original_values = {}

    # First, check what parameters exist and can be read
    available_params = []
    for param_name in params_to_check:
        try:
            param = getattr(camera, param_name)
            if param.IsReadable():
                value = param.GetValue()
                original_values[param_name] = value
                available_params.append(param_name)
                print(f"✓ {param_name}: {value}")
        except:
            print(f"✗ {param_name}: Not available")

    print("\n" + "=" * 60)
    print("Testing sequencer parameter storage...")
    print("=" * 60)

    # Make sure sequencer is off
    if camera.SequencerMode.GetValue() == "On":
        camera.SequencerMode.SetValue("Off")

    # Enter configuration mode
    camera.SequencerConfigurationMode.SetValue("On")

    # Configure Set 0 with different values (where possible)
    camera.SequencerSetSelector.SetValue(0)
    print("\n--- Modifying Set 0 ---")

    test_changes = {}
    for param_name in available_params:
        try:
            param = getattr(camera, param_name)
            if param.IsWritable():
                if param_name == "Width":
                    # Try to set a different width
                    new_val = 640 if original_values[param_name] != 640 else 800
                    param.SetValue(new_val)
                    test_changes[param_name] = new_val
                    print(f"  Set {param_name} to {new_val}")
                elif param_name == "Height":
                    # Try to set a different height
                    new_val = 480 if original_values[param_name] != 480 else 600
                    param.SetValue(new_val)
                    test_changes[param_name] = new_val
                    print(f"  Set {param_name} to {new_val}")
                elif param_name == "ExposureTime":
                    # Set exposure to 100
                    param.SetValue(100.0)
                    test_changes[param_name] = 100.0
                    print(f"  Set {param_name} to 100.0")
        except Exception as e:
            print(f"  Could not modify {param_name}: {e}")

    # Check Set 1 to see if it has different values
    camera.SequencerSetSelector.SetValue(1)
    print("\n--- Checking Set 1 (should have original values) ---")

    for param_name in available_params:
        try:
            param = getattr(camera, param_name)
            value = param.GetValue()
            if param_name in test_changes:
                if value != test_changes[param_name]:
                    print(f"  {param_name}: {value} (different from Set 0: {test_changes[param_name]})")
                else:
                    print(f"  {param_name}: {value} (SAME as Set 0!)")
        except:
            pass

    # Exit configuration mode
    camera.SequencerConfigurationMode.SetValue("Off")

    print("\n" + "=" * 60)
    print("IMPORTANT: Parameters that show 'SAME as Set 0' are")
    print("shared across ALL sequencer sets and need special handling!")
    print("=" * 60)

    camera.Close()

except Exception as e:
    print(f"Error: {e}")