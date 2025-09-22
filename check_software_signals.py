#!/usr/bin/env python3
"""
Diagnostic script to check software signal capabilities for HDR profile switching
"""

import sys

try:
    from pypylon import pylon

    print("Checking camera software signal features...")
    print("=" * 50)

    # Create camera object
    camera = pylon.InstantCamera(pylon.TlFactory.GetInstance().CreateFirstDevice())
    camera.Open()

    print(f"Connected to: {camera.GetDeviceInfo().GetModelName()}")
    print(f"Serial Number: {camera.GetDeviceInfo().GetSerialNumber()}\n")

    # Features to check for software signals
    signal_features = [
        "SoftwareSignalSelector",
        "SoftwareSignalPulse",
        "SoftwareSignal1",
        "SoftwareSignal2",
        "SequencerTriggerSource",
        "SequencerTriggerActivation",
        "TriggerSoftware",
        "SoftwareTriggerExecute",
    ]

    print("Software Signal Features:")
    print("-" * 40)

    for feature_name in signal_features:
        try:
            if hasattr(camera, feature_name):
                param = getattr(camera, feature_name)
                print(f"✓ {feature_name}: Available", end="")

                # Try to get current value
                try:
                    if hasattr(param, 'GetValue'):
                        value = param.GetValue()
                        print(f" - Current: {value}", end="")

                    # For enumerations, get possible values
                    if hasattr(param, 'GetSymbolics'):
                        symbols = param.GetSymbolics()
                        print(f"\n  Options: {', '.join(symbols)}", end="")
                except:
                    pass
                print()
            else:
                print(f"✗ {feature_name}: Not found")
        except Exception as e:
            print(f"✗ {feature_name}: Error - {e}")

    # Test software signal switching
    print("\n" + "=" * 50)
    print("Testing Software Signal Switching:")
    print("-" * 40)

    try:
        if hasattr(camera, 'SoftwareSignalSelector') and hasattr(camera, 'SoftwareSignalPulse'):
            selector = camera.SoftwareSignalSelector
            pulse = camera.SoftwareSignalPulse

            print("Available signals:")
            signals = selector.GetSymbolics()
            for sig in signals:
                print(f"  - {sig}")

            # Try to switch between signals
            for signal in ['SoftwareSignal1', 'SoftwareSignal2']:
                if signal in signals:
                    print(f"\nTesting {signal}:")
                    selector.SetValue(signal)
                    print(f"  Selected: {selector.GetValue()}")

                    # Check if pulse is available
                    if hasattr(pulse, 'Execute'):
                        print(f"  Pulse command: Available")
                    else:
                        print(f"  Pulse command: Not executable")
        else:
            print("SoftwareSignalSelector or SoftwareSignalPulse not available")

    except Exception as e:
        print(f"Error testing software signals: {e}")

    # Check sequencer trigger sources
    print("\n" + "=" * 50)
    print("Sequencer Trigger Sources:")
    print("-" * 40)

    try:
        if hasattr(camera, 'SequencerTriggerSource'):
            trigger_source = camera.SequencerTriggerSource
            print("Available trigger sources:")
            sources = trigger_source.GetSymbolics()
            for src in sources:
                print(f"  - {src}")
                if 'Software' in src or 'Signal' in src:
                    print(f"    ^ Good for HDR profile switching")
        else:
            print("SequencerTriggerSource not available")
    except Exception as e:
        print(f"Error checking trigger sources: {e}")

    camera.Close()
    print("\n✓ Diagnostic complete!")

except ImportError:
    print("ERROR: pypylon is not installed. Install it with:")
    print("  pip install pypylon")
    sys.exit(1)
except Exception as e:
    print(f"ERROR: {e}")
    sys.exit(1)