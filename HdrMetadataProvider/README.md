# HDR Metadata Provider (C# Implementation)

This is a C# implementation of the HDR Metadata Provider with comprehensive xUnit tests. The provider tracks HDR exposure sequences and maintains a monotonically increasing master sequence counter across profile switches.

## Features

- Dual HDR profile support (Profile 0 and Profile 1)
- Variable-length sequences (1-N exposures per profile)
- Automatic duplicate exposure handling
- Frame gap tolerance
- Out-of-order frame support
- Single exposure profile support
- Master sequence continuity across profile switches

## Building and Running Tests

### Prerequisites

- .NET 8.0 SDK or later
- Visual Studio 2022, VS Code, or any C# IDE

### Build

```bash
cd HdrMetadataProvider
dotnet build
```

### Run Tests

```bash
dotnet test
```

### Run Tests with Detailed Output

```bash
dotnet test --logger "console;verbosity=detailed"
```

### Run Tests with Coverage

```bash
dotnet test --collect:"XPlat Code Coverage"
```

## Test Coverage

The test suite includes:

1. **HdrMetadataProviderTests.cs** - Core functionality tests:
   - Basic configuration
   - Single profile sequence tracking
   - Profile switching
   - Duplicate exposure handling
   - Variable-length sequences
   - Single exposure profiles
   - Reset functionality
   - Master sequence continuity

2. **HdrMetadataProviderGapTests.cs** - Frame gap handling:
   - Frame gaps within sequences
   - Gaps with profile switches
   - Extreme gaps (millions of frames)
   - Single exposure with gaps

3. **HdrMetadataProviderDisorderTests.cs** - Out-of-order frame handling:
   - Out-of-order frames within windows
   - Out-of-order with profile switches
   - Mixed gaps and disorder
   - Backwards frame numbers
   - Repeated same exposure

## Implementation Notes

### Key Design Decisions

1. **Master Sequence**: Increments when:
   - A complete HDR window finishes (all exposures captured)
   - A profile switch is detected
   - For single-exposure profiles, every frame increments

2. **Exposure Mapping**:
   - Uses exposure time as key to identify position in sequence
   - Assumes exposure times are unique within each profile
   - Automatically adjusts duplicates across profiles by adding +1μs

3. **Frame Number Independence**:
   - Frame numbers can have gaps
   - Frame numbers can go backwards
   - Only the exposure sequence matters for HDR tracking

4. **Out-of-Order Handling**:
   - Frames can arrive out of order within a window
   - Window completion detected by seeing index 0 after non-zero index
   - Profile switches always increment master sequence

### Edge Cases Handled

- Gaps in frame numbers don't affect master sequence calculation
- Single exposure sequences (every frame is a new window)
- Duplicate exposures across profiles (automatically adjusted)
- Out-of-order frames within the same HDR window
- Profile switches mid-window
- Extreme frame number jumps (millions of frames)

## Usage Example

```csharp
var provider = new HdrMetadataProviderImpl();

// Configure profiles
provider.SetProfile0Sequence(new List<uint> { 19, 150 });
provider.SetProfile1Sequence(new List<uint> { 250, 350, 450 });

// Process frames
var metadata = provider.ProcessFrame(19, 0);
Console.WriteLine($"Frame 0: MasterSeq={metadata.MasterSequence}, " +
                  $"Profile={metadata.HdrProfile}, " +
                  $"Index={metadata.ExposureSequenceIndex}");

// Switch profiles
metadata = provider.ProcessFrame(250, 10); // Gap + profile switch
Console.WriteLine($"Frame 10: MasterSeq={metadata.MasterSequence}"); // Incremented
```

## Integration with GStreamer Plugin

This C# implementation serves as a reference and testing ground. The actual plugin uses the C++ implementation in `gst-libs/gst/pylon/gsthdrmetadataprovider.cpp`.

The C# tests help validate the logic and can be used to:
1. Verify expected behavior
2. Test edge cases
3. Debug issues
4. Prototype improvements

## License

Same as the parent gst-plugin-pylon project.