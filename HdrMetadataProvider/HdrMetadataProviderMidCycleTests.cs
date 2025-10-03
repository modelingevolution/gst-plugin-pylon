using Microsoft.Extensions.Logging;
using Xunit;
using Xunit.Abstractions;

namespace HdrMetadataProvider.Tests;

public class HdrMetadataProviderMidCycleTests
{
    
    private readonly ILogger<HdrMetadataProviderImpl> logger;

    public HdrMetadataProviderMidCycleTests(ITestOutputHelper outputHelper)
    {
        this.logger = LoggerFactory.Create(builder =>
            {
                builder.AddXunit(outputHelper);
                // Add other loggers, e.g.: AddConsole, AddDebug, etc.
            })
            .CreateLogger<HdrMetadataProviderImpl>();
    
    }

    [Fact]
    public void WindowSize_2vs2_MidCycleSwitches_ShouldMaintainContinuity()
    {
        // Arrange - Both profiles have window size 2
        var provider = HdrMetadataProviderImpl.Create(logger, new uint[] { 10, 20 }, new uint[] { 30, 40 }, out _, out _);
        ulong f = 1;
        ulong m = 1;

        // Act & Assert - Start with Profile 0
        var meta1 = provider.ProcessFrame(f++, 10);
        Assert.Equal(m, meta1.MasterSequence);
        Assert.Equal(0, meta1.HdrProfile);
        Assert.Equal(0, meta1.ExposureSequenceIndex);

        // SWITCH 1: Mid-cycle switch to Profile 1 at frame 2
        var meta2 = provider.ProcessFrame(f++, 40);
        Assert.Equal(m, meta2.MasterSequence);  // Should still be window 1
        Assert.Equal(1, meta2.HdrProfile);
        Assert.Equal(1, meta2.ExposureSequenceIndex);  // First exposure of Profile 1


        // Continue in Profile 1 - complete window
        var meta4 = provider.ProcessFrame(f++, 30);
        Assert.Equal(++m, meta4.MasterSequence);
        Assert.Equal(0, meta4.ExposureSequenceIndex);

        var meta5 = provider.ProcessFrame(f++, 40);
        Assert.Equal(m, meta5.MasterSequence);  // Window 3

        // Start new window in Profile 1
        var meta6 = provider.ProcessFrame(f++, 30);
        Assert.Equal(++m, meta6.MasterSequence);

        // SWITCH 2: Mid-cycle switch back to Profile 0 at frame 7
        var meta7 = provider.ProcessFrame(f++, 10);
        Assert.Equal(++m, meta7.MasterSequence);  // Should advance to window 4
        Assert.Equal(0, meta7.HdrProfile);
        Assert.Equal(0, meta7.ExposureSequenceIndex);

        var meta8 = provider.ProcessFrame(f++, 20);
        Assert.Equal(m, meta8.MasterSequence);  // Complete window 4
        Assert.Equal(1, meta8.ExposureSequenceIndex);

        // Continue in Profile 0
        var meta9 = provider.ProcessFrame(f++, 10);
        Assert.Equal(++m, meta9.MasterSequence);  // Window 5

        // SWITCH 3: Another mid-cycle switch at frame 10
        var meta10 = provider.ProcessFrame(f++, 30);
        Assert.Equal(++m, meta10.MasterSequence);  // Should not stay in window 5, move to next because it looks like we've just started a new window.
        Assert.Equal(1, meta10.HdrProfile);

        var meta11 = provider.ProcessFrame(f++, 40);
        Assert.Equal(m, meta11.MasterSequence);  // Window 6

        var meta12 = provider.ProcessFrame(f++, 30);
        Assert.Equal(++m, meta12.MasterSequence);  
    }



}