#!/usr/bin/env python3
"""
Diagnostic script to check SequencerPathSelector support for dual HDR profiles
"""

import sys

try:
    from pypylon import pylon

    print("Checking camera sequencer path selector support...")
    print("=" * 50)

    # Create camera object
    camera = pylon.InstantCamera(pylon.TlFactory.GetInstance().CreateFirstDevice())
    camera.Open()

    print(f"Connected to: {camera.GetDeviceInfo().GetModelName()}")
    print(f"Serial Number: {camera.GetDeviceInfo().GetSerialNumber()}\n")

    # Features to check for path-based sequencer
    path_features = [
        "SequencerMode",
        "SequencerConfigurationMode",
        "SequencerSetSelector",
        "SequencerPathSelector",
        "SequencerSetNext",
        "SequencerTriggerSource",
        "SequencerSetStart",
        "SequencerSetLoad",
        "SequencerSetSave",
        "SoftwareSignalSelector",
        "SoftwareSignalPulse",
    ]

    print("Sequencer Path Features:")
    print("-" * 40)

    critical_features = []
    for feature_name in path_features:
        try:
            if hasattr(camera, feature_name):
                param = getattr(camera, feature_name)
                is_critical = feature_name in ["SequencerPathSelector", "SequencerMode"]

                if is_critical:
                    critical_features.append(feature_name)

                print(f"{'✓' if is_critical else '○'} {feature_name}: Available", end="")

                # Try to get info about the parameter
                try:
                    if hasattr(param, 'GetValue'):
                        value = param.GetValue()
                        print(f" - Current: {value}", end="")

                    # For integer parameters, get range
                    if hasattr(param, 'GetMin') and hasattr(param, 'GetMax'):
                        min_val = param.GetMin()
                        max_val = param.GetMax()
                        print(f" (Range: {min_val}-{max_val})", end="")

                    # For enumerations, get possible values
                    if hasattr(param, 'GetSymbolics'):
                        symbols = param.GetSymbolics()
                        print(f"\n  Options: {', '.join(symbols)}", end="")
                except:
                    pass
                print()
            else:
                if feature_name in ["SequencerPathSelector", "SequencerMode"]:
                    print(f"✗ {feature_name}: NOT FOUND (CRITICAL)")
                else:
                    print(f"✗ {feature_name}: Not found")
        except Exception as e:
            print(f"✗ {feature_name}: Error - {e}")

    # Check if critical features are available
    print("\n" + "=" * 50)
    print("Dual HDR Profile Capability Check:")
    print("-" * 40)

    has_path_selector = "SequencerPathSelector" in critical_features
    has_sequencer = "SequencerMode" in critical_features

    if has_path_selector and has_sequencer:
        print("✓ Camera SUPPORTS dual HDR profiles with path branching")
        print("  - SequencerPathSelector is available")
        print("  - Sequencer mode is available")
    elif has_sequencer and not has_path_selector:
        print("⚠ Camera supports sequencer mode but NOT path branching")
        print("  - Single HDR profile mode is supported")
        print("  - Dual profile mode requires SequencerPathSelector")
    else:
        print("✗ Camera does NOT support HDR sequencer mode")

    # Test path selector if available
    if has_path_selector:
        print("\n" + "=" * 50)
        print("Testing SequencerPathSelector:")
        print("-" * 40)

        try:
            path_selector = camera.SequencerPathSelector
            min_path = path_selector.GetMin()
            max_path = path_selector.GetMax()
            current = path_selector.GetValue()

            print(f"Path selector range: {min_path} to {max_path}")
            print(f"Current path: {current}")

            # Try to set different paths
            for path in range(min_path, min(max_path + 1, 3)):
                try:
                    path_selector.SetValue(path)
                    print(f"  ✓ Can set path {path}")
                except:
                    print(f"  ✗ Cannot set path {path}")

        except Exception as e:
            print(f"Error testing path selector: {e}")

    camera.Close()
    print("\n✓ Diagnostic complete!")

except ImportError:
    print("ERROR: pypylon is not installed. Install it with:")
    print("  pip install pypylon")
    sys.exit(1)
except Exception as e:
    print(f"ERROR: {e}")
    sys.exit(1)