using Microsoft.Extensions.Logging;
using Xunit;
using Xunit.Abstractions;

namespace HdrMetadataProvider.Tests;

public class HdrMetadataProviderAdjustmentTests
{
    private readonly ILogger<HdrMetadataProviderImpl> logger;

    public HdrMetadataProviderAdjustmentTests(ITestOutputHelper outputHelper)
    {
        this.logger = LoggerFactory.Create(builder =>
            {
                builder.AddXunit(outputHelper);
                // Add other loggers, e.g.: AddConsole, AddDebug, etc.
            })
            .CreateLogger<HdrMetadataProviderImpl>();
    }
    [Fact]
    public void SetProfile_ShouldReturnAdjustedValues_WhenDuplicatesExist()
    {
        // Arrange
        var provider = HdrMetadataProviderImpl.Create(logger, new uint[] { 100, 200, 300 }, new uint[] { 100, 250, 300 }, out var profile0Values, out var profile1Values);
        // Assert - Profile 0 should be unchanged
        Assert.Equal(new uint[] { 100, 200, 300 }, profile0Values);

        // Profile 1 should have adjusted values for duplicates
        Assert.Equal(101u, profile1Values[0]); // 100 -> 101 (adjusted)
        Assert.Equal(250u, profile1Values[1]); // 250 unchanged
        Assert.Equal(301u, profile1Values[2]); // 300 -> 301 (adjusted)
    }

    [Fact]
    public void SetProfile_ShouldReturnOriginalValues_WhenNoDuplicates()
    {
        // Arrange
        var provider = HdrMetadataProviderImpl.Create(logger, new uint[] { 100, 200 }, new uint[] { 300, 400 }, out var profile0Values, out var profile1Values);
        // Assert - No adjustments needed
        Assert.Equal(new uint[] { 100, 200 }, profile0Values);
        Assert.Equal(new uint[] { 300, 400 }, profile1Values);
    }

    [Fact]
    public void ProcessFrame_ShouldRecognizeAdjustedExposures()
    {
        // Arrange
        var provider = HdrMetadataProviderImpl.Create(logger, new uint[] { 100, 200 }, new uint[] { 100, 300 }, out var profile0Values, out var profile1Values);
        // Act & Assert - Process frames with adjusted values
        // Profile 0 uses original value
        var meta0 = provider.ProcessFrame(1, 100);
        Assert.Equal(0, meta0.HdrProfile);
        Assert.Equal(0, meta0.ExposureSequenceIndex);

        // Profile 1 should use adjusted value (101)
        var meta1 = provider.ProcessFrame(2, 101); // Using adjusted value!
        Assert.Equal(1, meta1.HdrProfile);
        Assert.Equal(0, meta1.ExposureSequenceIndex);

        var meta2 = provider.ProcessFrame(3, 300);
        Assert.Equal(1, meta2.HdrProfile);
        Assert.Equal(1, meta2.ExposureSequenceIndex);
    }

    [Fact]
    public void SetProfile_CalledMultipleTimes_ShouldRecalculateAdjustments()
    {
        // Act - First configuration
        var provider1 = HdrMetadataProviderImpl.Create(logger, new uint[] { 100, 200 }, new uint[] { 100, 300 }, out _, out var profile1Values1);
        Assert.Equal(101u, profile1Values1[0]); // Adjusted

        // Reconfigure with different values
        var provider2 = HdrMetadataProviderImpl.Create(logger, new uint[] { 150, 250 }, new uint[] { 150, 350 }, out _, out var profile1Values2);
        Assert.Equal(151u, profile1Values2[0]); // New adjustment

        // Reconfigure with no duplicates
        var provider3 = HdrMetadataProviderImpl.Create(logger, new uint[] { 100, 200 }, new uint[] { 300, 400 }, out _, out var profile1Values3);
        Assert.Equal(new uint[] { 300, 400 }, profile1Values3); // No adjustments
    }

    [Fact]
    public void SetProfile_WithComplexDuplicates_ShouldAdjustCorrectly()
    {
        // Arrange
        var provider = HdrMetadataProviderImpl.Create(logger, new uint[] { 10, 20, 30, 40, 50 }, new uint[] { 20, 30, 40, 60, 70 }, out var profile0Values, out var profile1Values);
        // Assert
        Assert.Equal(new uint[] { 10, 20, 30, 40, 50 }, profile0Values);

        // Profile 1 should have adjustments for all duplicates
        Assert.Equal(21u, profile1Values[0]); // 20 -> 21
        Assert.Equal(31u, profile1Values[1]); // 30 -> 31
        Assert.Equal(41u, profile1Values[2]); // 40 -> 41
        Assert.Equal(60u, profile1Values[3]); // 60 unchanged
        Assert.Equal(70u, profile1Values[4]); // 70 unchanged
    }

    [Fact]
    public void UsageExample_ForCameraConfiguration()
    {
        // This shows how the return values would be used to configure the camera

        var provider = HdrMetadataProviderImpl.Create(logger, new uint[] { 19, 150 }, new uint[] { 19, 250 }, out var profile0Values, out var profile1Values);
        // Use returned values to configure camera sequencer
        // Profile 0: Use as-is
        Assert.Equal(new uint[] { 19, 150 }, profile0Values);

        // Profile 1: Use adjusted values
        Assert.Equal(20u, profile1Values[0]); // Camera should be configured with 20, not 19
        Assert.Equal(250u, profile1Values[1]);

        // When processing frames, the camera will send the adjusted exposure values
        var meta = provider.ProcessFrame(1, 20); // Camera sends 20, not 19
        Assert.Equal(1, meta.HdrProfile);
        Assert.Equal(0, meta.ExposureSequenceIndex);
    }
}