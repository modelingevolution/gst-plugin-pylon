using System.Runtime.InteropServices;

namespace HdrMetadataProvider;

/// <summary>
/// HDR metadata structure for each frame, Pack=1
/// </summary>
[StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential, Pack = 1)]
public struct HdrMetadata
{
    /// <summary>
    /// Monotonically increasing counter for complete HDR windows.
    /// Increments each time a full HDR sequence completes or when profile switching is detected.
    /// </summary>
    public ulong MasterSequence;

    /// <summary>
    /// Actual exposure time in microseconds for the current frame
    /// </summary>
    public uint ExposureValue;

    /// <summary>
    /// Zero-based index of the current exposure within the active HDR sequence (0 to ExposureCount-1)
    /// </summary>
    public byte ExposureSequenceIndex;

    /// <summary>
    /// Total number of exposures in the active HDR sequence
    /// </summary>
    public byte ExposureCount;

    /// <summary>
    /// Currently active HDR profile (0 or 1)
    /// </summary>
    public byte HdrProfile;
}