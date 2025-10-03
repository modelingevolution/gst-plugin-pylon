namespace HdrMetadataProvider;

/// <summary>
/// Interface for HDR metadata provider
/// </summary>
public interface IHdrMetadataProvider
{
    /// <summary>
    /// Process a frame and return its HDR metadata
    /// </summary>
    /// <param name="frameNumber">The frame number (may have gaps)</param>
    /// <param name="actualExposureTime">The actual exposure time from chunk metadata</param>
    /// <returns>HDR metadata for this frame</returns>
    HdrMetadata ProcessFrame(ulong frameNumber, uint actualExposureTime);

}