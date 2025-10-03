using Microsoft.Extensions.Logging;
using Xunit;
using Xunit.Abstractions;

namespace HdrMetadataProvider.Tests;

public class HdrMetadataProviderGapTests
{
    private readonly ILogger<HdrMetadataProviderImpl> logger;

    public HdrMetadataProviderGapTests(ITestOutputHelper outputHelper)
    {
        this.logger = LoggerFactory.Create(builder =>
            {
                builder.AddXunit(outputHelper);
                // Add other loggers, e.g.: AddConsole, AddDebug, etc.
            })
            .CreateLogger<HdrMetadataProviderImpl>();
    }

    [Fact]
    public void ExtremeGaps_ShouldHandleCorrectly()
    {
        // Arrange
        var provider = HdrMetadataProviderImpl.Create(logger, [100, 200], [], out _, out _);

        // Act & Assert
        var meta0 = provider.ProcessFrame(1, 100);
        Assert.Equal(1ul, meta0.MasterSequence);

        // Huge gap - jump to frame 1000000
        var meta1M = provider.ProcessFrame(1000000, 200);
        Assert.Equal(500000ul, meta1M.MasterSequence); // Still in first window
        Assert.Equal(1, meta1M.ExposureSequenceIndex);

        // Continue with huge frame numbers
        var meta1M1 = provider.ProcessFrame(1000001, 100);
        Assert.Equal(500001ul, meta1M1.MasterSequence); // New window
        Assert.Equal(0, meta1M1.ExposureSequenceIndex);

        // Jump backwards (frame numbers don't matter, only sequence)
        var meta10 = provider.ProcessFrame(10, 200);
        Assert.Equal(5ul, meta10.MasterSequence); // Continues from where we were
        Assert.Equal(1, meta10.ExposureSequenceIndex);
    }

   
}