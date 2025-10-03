using Microsoft.Extensions.Logging;
using Xunit;
using Xunit.Abstractions;

namespace HdrMetadataProvider.Tests;

public class HdrMetadataProviderWindowSizeTests
{
    private readonly ILogger<HdrMetadataProviderImpl> logger;

    public HdrMetadataProviderWindowSizeTests(ITestOutputHelper outputHelper)
    {
        this.logger = LoggerFactory.Create(builder =>
            {
                builder.AddXunit(outputHelper);
                // Add other loggers, e.g.: AddConsole, AddDebug, etc.
            })
            .CreateLogger<HdrMetadataProviderImpl>();
    }
    [Fact]
    public void WindowSize_1vs1_ShouldMaintainContinuity()
    {
        // Arrange - Both profiles have single exposure (window size = 1)
        var provider = HdrMetadataProviderImpl.Create(logger, new uint[] { 50 }, new uint[] { 200 }, out _, out _);  // Window size 1

        // Act & Assert - Profile 0
        var meta1 = provider.ProcessFrame(1, 50);
        Assert.Equal(1ul, meta1.MasterSequence);  // Frame 1 -> Master 1
        Assert.Equal(0, meta1.HdrProfile);

        var meta2 = provider.ProcessFrame(2, 50);
        Assert.Equal(2ul, meta2.MasterSequence);  // Frame 2 -> Master 2

        var meta3 = provider.ProcessFrame(3, 50);
        Assert.Equal(3ul, meta3.MasterSequence);  // Frame 3 -> Master 3

        // Switch to Profile 1
        var meta4 = provider.ProcessFrame(4, 200);
        Assert.Equal(4ul, meta4.MasterSequence);  // Should continue: Frame 4 -> Master 4
        Assert.Equal(1, meta4.HdrProfile);

        var meta5 = provider.ProcessFrame(5, 200);
        Assert.Equal(5ul, meta5.MasterSequence);  // Frame 5 -> Master 5

        // Switch back to Profile 0
        var meta6 = provider.ProcessFrame(6, 50);
        Assert.Equal(6ul, meta6.MasterSequence);  // Frame 6 -> Master 6
        Assert.Equal(0, meta6.HdrProfile);
    }

    [Fact]
    public void WindowSize_1vs2_ShouldMaintainContinuity()
    {
        // Arrange - Profile 0: window size 1, Profile 1: window size 2
        var provider = HdrMetadataProviderImpl.Create(logger, new uint[] { 50 }, new uint[] { 100, 200 }, out _, out _);  // Window size 2

        // Act & Assert - Profile 0 (single exposure)
        var meta1 = provider.ProcessFrame(1, 50);
        Assert.Equal(1ul, meta1.MasterSequence);

        var meta2 = provider.ProcessFrame(2, 50);
        Assert.Equal(2ul, meta2.MasterSequence);

        var meta3 = provider.ProcessFrame(3, 50);
        Assert.Equal(3ul, meta3.MasterSequence);

        // Switch to Profile 1 (2 exposures)
        var meta4 = provider.ProcessFrame(4, 100);
        Assert.Equal(4ul, meta4.MasterSequence);  // Same window continues
        Assert.Equal(1, meta4.HdrProfile);
        Assert.Equal(0, meta4.ExposureSequenceIndex);

        var meta5 = provider.ProcessFrame(5, 200);
        Assert.Equal(4ul, meta5.MasterSequence);  // Still same window
        Assert.Equal(1, meta5.ExposureSequenceIndex);

        // New window in Profile 1
        var meta6 = provider.ProcessFrame(6, 100);
        Assert.Equal(5ul, meta6.MasterSequence);  // New window

        var meta7 = provider.ProcessFrame(7, 200);
        Assert.Equal(5ul, meta7.MasterSequence);

        // Switch back to Profile 0
        var meta8 = provider.ProcessFrame(8, 50);
        Assert.Equal(6ul, meta8.MasterSequence);  // Should increment
        Assert.Equal(0, meta8.HdrProfile);

        var meta9 = provider.ProcessFrame(9, 50);
        Assert.Equal(7ul, meta9.MasterSequence);
    }

    [Fact]
    public void WindowSize_1vs3_ShouldMaintainContinuity()
    {
        // Arrange - Profile 0: window size 1, Profile 1: window size 3
        var provider = HdrMetadataProviderImpl.Create(logger, new uint[] { 50 }, new uint[] { 100, 200, 300 }, out _, out _);   // Window size 3

        // Act & Assert - Profile 0 (complete 3 windows)
        var meta1 = provider.ProcessFrame(1, 50);
        Assert.Equal(1ul, meta1.MasterSequence);

        var meta2 = provider.ProcessFrame(2, 50);
        Assert.Equal(2ul, meta2.MasterSequence);

        var meta3 = provider.ProcessFrame(3, 50);
        Assert.Equal(3ul, meta3.MasterSequence);

        // Switch to Profile 1 at frame 4 (after complete windows)
        // With w1=1, w2=3: Master should continue as if w1=1 continued
        var meta4 = provider.ProcessFrame(4, 100);
        Assert.Equal(4ul, meta4.MasterSequence);  // Continues counting
        Assert.Equal(1, meta4.HdrProfile);
        Assert.Equal(0, meta4.ExposureSequenceIndex);

        var meta5 = provider.ProcessFrame(5, 200);
        Assert.Equal(4ul, meta5.MasterSequence);  // Same window
        Assert.Equal(1, meta5.ExposureSequenceIndex);

        var meta6 = provider.ProcessFrame(6, 300);
        Assert.Equal(4ul, meta6.MasterSequence);  // Completes window 4
        Assert.Equal(2, meta6.ExposureSequenceIndex);

        // New window in Profile 1
        var meta7 = provider.ProcessFrame(7, 100);
        Assert.Equal(5ul, meta7.MasterSequence);  // New window

        var meta8 = provider.ProcessFrame(8, 200);
        Assert.Equal(5ul, meta8.MasterSequence);

        var meta9 = provider.ProcessFrame(9, 300);
        Assert.Equal(5ul, meta9.MasterSequence);  // Completes window 5

        // Switch back to Profile 0 at frame 10
        var meta10 = provider.ProcessFrame(10, 50);
        Assert.Equal(6ul, meta10.MasterSequence);  // Continues incrementing
        Assert.Equal(0, meta10.HdrProfile);
    }

    [Fact]
    public void WindowSize_2vs3_ShouldMaintainContinuity()
    {
        // Arrange - Profile 0: window size 2, Profile 1: window size 3
        var provider = HdrMetadataProviderImpl.Create(logger, new uint[] { 19, 150 }, new uint[] { 250, 350, 450 }, out _, out _);  // Window size 3

        // Act & Assert - Profile 0 (complete 3 windows)
        var meta1 = provider.ProcessFrame(1, 19);
        Assert.Equal(1ul, meta1.MasterSequence);  // Window 1

        var meta2 = provider.ProcessFrame(2, 150);
        Assert.Equal(1ul, meta2.MasterSequence);  // Complete window 1

        var meta3 = provider.ProcessFrame(3, 19);
        Assert.Equal(2ul, meta3.MasterSequence);  // Window 2

        var meta4 = provider.ProcessFrame(4, 150);
        Assert.Equal(2ul, meta4.MasterSequence);  // Complete window 2

        var meta5 = provider.ProcessFrame(5, 19);
        Assert.Equal(3ul, meta5.MasterSequence);  // Window 3

        var meta6 = provider.ProcessFrame(6, 150);
        Assert.Equal(3ul, meta6.MasterSequence);  // Complete window 3

        // Switch to Profile 1 at frame 7 (after complete windows)
        // Master sequence should continue as if w=2 continued
        var meta7 = provider.ProcessFrame(7, 250);
        Assert.Equal(4ul, meta7.MasterSequence);  // Continues to window 4
        Assert.Equal(1, meta7.HdrProfile);

        var meta8 = provider.ProcessFrame(8, 350);
        Assert.Equal(4ul, meta8.MasterSequence);

        var meta9 = provider.ProcessFrame(9, 450);
        Assert.Equal(4ul, meta9.MasterSequence);  // Complete window 4

        // Continue Profile 1
        var meta10 = provider.ProcessFrame(10, 250);
        Assert.Equal(5ul, meta10.MasterSequence);  // Window 5

        var meta11 = provider.ProcessFrame(11, 350);
        Assert.Equal(5ul, meta11.MasterSequence);

        var meta12 = provider.ProcessFrame(12, 450);
        Assert.Equal(5ul, meta12.MasterSequence);  // Complete window 5

        // Switch back to Profile 0 at frame 13
        var meta13 = provider.ProcessFrame(13, 19);
        Assert.Equal(6ul, meta13.MasterSequence);  // Continues to window 6
        Assert.Equal(0, meta13.HdrProfile);

        var meta14 = provider.ProcessFrame(14, 150);
        Assert.Equal(6ul, meta14.MasterSequence);  // Complete window 6
    }

    [Fact]
    public void WindowSize_2vs4_ShouldMaintainContinuity()
    {
        // Arrange - Profile 0: window size 2, Profile 1: window size 4
        var provider = HdrMetadataProviderImpl.Create(logger, new uint[] { 50, 100 }, new uint[] { 200, 300, 400, 500 }, out _, out _); // Window size 4

        // Act & Assert - Profile 0 (complete 2 windows)
        var meta1 = provider.ProcessFrame(1, 50);
        Assert.Equal(1ul, meta1.MasterSequence);

        var meta2 = provider.ProcessFrame(2, 100);
        Assert.Equal(1ul, meta2.MasterSequence);  // Complete window 1

        var meta3 = provider.ProcessFrame(3, 50);
        Assert.Equal(2ul, meta3.MasterSequence);

        var meta4 = provider.ProcessFrame(4, 100);
        Assert.Equal(2ul, meta4.MasterSequence);  // Complete window 2

        // Switch to Profile 1 at frame 5 (after complete windows)
        // Master should continue as if w=2 continued
        var meta5 = provider.ProcessFrame(5, 200);
        Assert.Equal(3ul, meta5.MasterSequence);  // Continues to window 3
        Assert.Equal(1, meta5.HdrProfile);

        var meta6 = provider.ProcessFrame(6, 300);
        Assert.Equal(3ul, meta6.MasterSequence);

        var meta7 = provider.ProcessFrame(7, 400);
        Assert.Equal(3ul, meta7.MasterSequence);

        var meta8 = provider.ProcessFrame(8, 500);
        Assert.Equal(3ul, meta8.MasterSequence);  // Complete window 3

        // Continue Profile 1
        var meta9 = provider.ProcessFrame(9, 200);
        Assert.Equal(4ul, meta9.MasterSequence);  // Window 4

        var meta10 = provider.ProcessFrame(10, 300);
        Assert.Equal(4ul, meta10.MasterSequence);

        var meta11 = provider.ProcessFrame(11, 400);
        Assert.Equal(4ul, meta11.MasterSequence);

        var meta12 = provider.ProcessFrame(12, 500);
        Assert.Equal(4ul, meta12.MasterSequence);  // Complete window 4

        // Switch back to Profile 0 at frame 13
        var meta13 = provider.ProcessFrame(13, 50);
        Assert.Equal(5ul, meta13.MasterSequence);  // Continues to window 5
        Assert.Equal(0, meta13.HdrProfile);

        var meta14 = provider.ProcessFrame(14, 100);
        Assert.Equal(5ul, meta14.MasterSequence);  // Complete window 5
    }

    [Fact]
    public void WindowSize_MultipleSwtiches_2vs3_ShouldMaintainContinuity()
    {
        // Test multiple switches to verify offset calculation stability
        var provider = HdrMetadataProviderImpl.Create(logger, new uint[] { 10, 20 }, new uint[] { 30, 40, 50 }, out _, out _); // Window size 3

        ulong frame = 1;

        // Profile 0: complete 2 windows
        provider.ProcessFrame(frame++, 10);
        var meta = provider.ProcessFrame(frame++, 20);
        Assert.Equal(1ul, meta.MasterSequence);  // Complete window 1

        provider.ProcessFrame(frame++, 10);
        meta = provider.ProcessFrame(frame++, 20);
        Assert.Equal(2ul, meta.MasterSequence);  // Complete window 2

        // Switch to Profile 1 at frame 5
        meta = provider.ProcessFrame(frame++, 30);
        Assert.Equal(3ul, meta.MasterSequence);  // Continues to window 3
        Assert.Equal(1, meta.HdrProfile);

        provider.ProcessFrame(frame++, 40);
        meta = provider.ProcessFrame(frame++, 50);  // Frame 7
        Assert.Equal(3ul, meta.MasterSequence);  // Complete window 3

        // Continue Profile 1 - complete window 4
        provider.ProcessFrame(frame++, 30);
        provider.ProcessFrame(frame++, 40);
        meta = provider.ProcessFrame(frame++, 50);  // Frame 10
        Assert.Equal(4ul, meta.MasterSequence);  // Complete window 4

        // Switch back to Profile 0 at frame 11
        meta = provider.ProcessFrame(frame++, 10);  // Frame 11
        Assert.Equal(5ul, meta.MasterSequence);  // Continues to window 5
        Assert.Equal(0, meta.HdrProfile);

        meta = provider.ProcessFrame(frame++, 20);  // Frame 12
        Assert.Equal(5ul, meta.MasterSequence);  // Complete window 5

        // Continue Profile 0 - complete window 6
        provider.ProcessFrame(frame++, 10);  // Frame 13
        meta = provider.ProcessFrame(frame++, 20);  // Frame 14
        Assert.Equal(6ul, meta.MasterSequence);  // Complete window 6

        // Switch to Profile 1 again at frame 15
        meta = provider.ProcessFrame(frame++, 30);  // Frame 15
        Assert.Equal(7ul, meta.MasterSequence);  // Continues to window 7
        Assert.Equal(1, meta.HdrProfile);

        provider.ProcessFrame(frame++, 40);
        meta = provider.ProcessFrame(frame++, 50);  // Frame 17
        Assert.Equal(7ul, meta.MasterSequence);  // Complete window 7
    }

    [Fact]
    public void WindowSize_LargerRatios_3vs6_ShouldMaintainContinuity()
    {
        // Test with 1:2 ratio (3 vs 6 exposures)
        var provider = HdrMetadataProviderImpl.Create(logger, new uint[] { 10, 20, 30 }, new uint[] { 100, 110, 120, 130, 140, 150 }, out _, out _); // Window size 6

        // Profile 0: Complete two windows
        provider.ProcessFrame(1, 10);
        provider.ProcessFrame(2, 20);
        var meta = provider.ProcessFrame(3, 30);
        Assert.Equal(1ul, meta.MasterSequence);  // Complete window 1

        provider.ProcessFrame(4, 10);
        provider.ProcessFrame(5, 20);
        meta = provider.ProcessFrame(6, 30);
        Assert.Equal(2ul, meta.MasterSequence);  // Complete window 2

        // Switch to Profile 1 at frame 7 (after complete windows)
        // Master should continue as if w=3 continued
        meta = provider.ProcessFrame(7, 100);
        Assert.Equal(3ul, meta.MasterSequence);  // Continues to window 3
        Assert.Equal(1, meta.HdrProfile);

        provider.ProcessFrame(8, 110);
        provider.ProcessFrame(9, 120);
        provider.ProcessFrame(10, 130);
        provider.ProcessFrame(11, 140);
        meta = provider.ProcessFrame(12, 150);
        Assert.Equal(3ul, meta.MasterSequence);  // Complete window 3

        // Continue Profile 1
        provider.ProcessFrame(13, 100);
        provider.ProcessFrame(14, 110);
        provider.ProcessFrame(15, 120);
        provider.ProcessFrame(16, 130);
        provider.ProcessFrame(17, 140);
        meta = provider.ProcessFrame(18, 150);
        Assert.Equal(4ul, meta.MasterSequence);  // Complete window 4
    }
}