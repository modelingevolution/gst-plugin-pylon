#!/usr/bin/env python3
"""
Check the current state of the sequencer and pixel format settings
"""

from pypylon import pylon

try:
    camera = pylon.InstantCamera(pylon.TlFactory.GetInstance().CreateFirstDevice())
    camera.Open()

    print(f"Connected to: {camera.GetDeviceInfo().GetModelName()}")
    print(f"Serial Number: {camera.GetDeviceInfo().GetSerialNumber()}\n")

    # Check current sequencer state
    print('Current Sequencer State:')
    print(f'SequencerMode: {camera.SequencerMode.GetValue()}')
    print(f'SequencerConfigurationMode: {camera.SequencerConfigurationMode.GetValue()}')

    # Check if pixel format can change in sequencer
    print('\n--- Checking Sequencer Sets Configuration ---')

    # Store original values
    orig_seq_mode = camera.SequencerMode.GetValue()
    orig_config_mode = camera.SequencerConfigurationMode.GetValue()
    orig_pixel_format = camera.PixelFormat.GetValue()

    print(f'Original Pixel Format (outside sequencer): {orig_pixel_format}')

    try:
        # Make sure sequencer is off first
        if camera.SequencerMode.GetValue() == "On":
            camera.SequencerMode.SetValue("Off")

        # Enter config mode
        camera.SequencerConfigurationMode.SetValue('On')

        # Check first few sequencer sets
        for set_idx in range(3):
            camera.SequencerSetSelector.SetValue(set_idx)

            print(f'\n--- Sequencer Set {set_idx} ---')
            print(f'  Exposure Time: {camera.ExposureTime.GetValue():.2f} μs')
            print(f'  Pixel Format: {camera.PixelFormat.GetValue()}')
            print(f'  Width: {camera.Width.GetValue()}')
            print(f'  Height: {camera.Height.GetValue()}')
            print(f'  SequencerSetNext: {camera.SequencerSetNext.GetValue()}')

        # Exit config mode
        camera.SequencerConfigurationMode.SetValue('Off')

        # Restore original sequencer mode
        camera.SequencerMode.SetValue(orig_seq_mode)

    except Exception as e:
        print(f'Error checking sequencer sets: {e}')
        # Try to exit config mode if error occurred
        try:
            camera.SequencerConfigurationMode.SetValue('Off')
        except:
            pass

    # Check if sequencer is currently enabled
    print(f'\n--- Final State ---')
    print(f'SequencerMode: {camera.SequencerMode.GetValue()}')
    print(f'Current Pixel Format: {camera.PixelFormat.GetValue()}')

    # Check what exposure mode is active
    try:
        if hasattr(camera, 'ExposureMode'):
            print(f'Exposure Mode: {camera.ExposureMode.GetValue()}')
    except:
        pass

    # Check trigger configuration
    print(f'\n--- Trigger Configuration ---')
    print(f'SequencerTriggerSource: {camera.SequencerTriggerSource.GetValue()}')

    camera.Close()

except Exception as e:
    print(f"Error: {e}")