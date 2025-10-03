using Microsoft.Extensions.Logging;
using Xunit;
using Xunit.Abstractions;

namespace HdrMetadataProvider.Tests;

public class HdrMetadataProviderExtensiveTests
{
    private readonly ILogger<HdrMetadataProviderImpl> logger;

    public HdrMetadataProviderExtensiveTests(ITestOutputHelper outputHelper)
    {
        this.logger = LoggerFactory.Create(builder =>
            {
                builder.AddXunit(outputHelper);
                // Add other loggers, e.g.: AddConsole, AddDebug, etc.
            })
            .CreateLogger<HdrMetadataProviderImpl>();
    }
    [Fact]
    public void ExtensiveProfileSwitching_12Switches_ShouldMaintainContinuity()
    {
        // Arrange - Profile 0: window size 2, Profile 1: window size 3
        var provider = HdrMetadataProviderImpl.Create(logger, new uint[] { 19, 150 }, new uint[] { 250, 350, 450 }, out _, out _);  // Window size 3

        ulong frame = 1;
        ulong expectedMaster = 1;

        // Switch 1: Start with Profile 0 - complete 2 windows
        provider.ProcessFrame(frame++, 19);
        var meta = provider.ProcessFrame(frame++, 150);
        Assert.Equal(expectedMaster++, meta.MasterSequence); // Window 1
        Assert.Equal(0, meta.HdrProfile);

        provider.ProcessFrame(frame++, 19);
        meta = provider.ProcessFrame(frame++, 150);
        Assert.Equal(expectedMaster++, meta.MasterSequence); // Window 2

        // Switch 2: Profile 0 -> Profile 1 at frame 5
        meta = provider.ProcessFrame(frame++, 250);
        Assert.Equal(expectedMaster, meta.MasterSequence);   // Window 3 starts
        Assert.Equal(1, meta.HdrProfile);

        provider.ProcessFrame(frame++, 350);
        meta = provider.ProcessFrame(frame++, 450);
        Assert.Equal(expectedMaster++, meta.MasterSequence); // Window 3 complete

        // Complete one more window in Profile 1
        provider.ProcessFrame(frame++, 250);
        provider.ProcessFrame(frame++, 350);
        meta = provider.ProcessFrame(frame++, 450);
        Assert.Equal(expectedMaster++, meta.MasterSequence); // Window 4

        // Switch 3: Profile 1 -> Profile 0 at frame 11
        meta = provider.ProcessFrame(frame++, 19);
        Assert.Equal(expectedMaster, meta.MasterSequence);   // Window 5 starts
        Assert.Equal(0, meta.HdrProfile);

        meta = provider.ProcessFrame(frame++, 150);
        Assert.Equal(expectedMaster++, meta.MasterSequence); // Window 5 complete

        // Complete two more windows in Profile 0
        provider.ProcessFrame(frame++, 19);
        meta = provider.ProcessFrame(frame++, 150);
        Assert.Equal(expectedMaster++, meta.MasterSequence); // Window 6

        provider.ProcessFrame(frame++, 19);
        meta = provider.ProcessFrame(frame++, 150);
        Assert.Equal(expectedMaster++, meta.MasterSequence); // Window 7

        // Switch 4: Profile 0 -> Profile 1 at frame 17
        meta = provider.ProcessFrame(frame++, 250);
        Assert.Equal(expectedMaster, meta.MasterSequence);   // Window 8 starts
        Assert.Equal(1, meta.HdrProfile);

        provider.ProcessFrame(frame++, 350);
        meta = provider.ProcessFrame(frame++, 450);
        Assert.Equal(expectedMaster++, meta.MasterSequence); // Window 8 complete

        // Switch 5: Profile 1 -> Profile 0 at frame 20 (immediate switch after one window)
        meta = provider.ProcessFrame(frame++, 19);
        Assert.Equal(expectedMaster, meta.MasterSequence);   // Window 9 starts
        Assert.Equal(0, meta.HdrProfile);

        meta = provider.ProcessFrame(frame++, 150);
        Assert.Equal(expectedMaster++, meta.MasterSequence); // Window 9 complete

        // Switch 6: Profile 0 -> Profile 1 at frame 22
        meta = provider.ProcessFrame(frame++, 250);
        Assert.Equal(expectedMaster, meta.MasterSequence);   // Window 10 starts
        Assert.Equal(1, meta.HdrProfile);

        provider.ProcessFrame(frame++, 350);
        meta = provider.ProcessFrame(frame++, 450);
        Assert.Equal(expectedMaster++, meta.MasterSequence); // Window 10 complete

        // Complete two more windows in Profile 1
        provider.ProcessFrame(frame++, 250);
        provider.ProcessFrame(frame++, 350);
        meta = provider.ProcessFrame(frame++, 450);
        Assert.Equal(expectedMaster++, meta.MasterSequence); // Window 11

        provider.ProcessFrame(frame++, 250);
        provider.ProcessFrame(frame++, 350);
        meta = provider.ProcessFrame(frame++, 450);
        Assert.Equal(expectedMaster++, meta.MasterSequence); // Window 12

        // Switch 7: Profile 1 -> Profile 0 at frame 31
        meta = provider.ProcessFrame(frame++, 19);
        Assert.Equal(expectedMaster, meta.MasterSequence);   // Window 13 starts
        Assert.Equal(0, meta.HdrProfile);

        meta = provider.ProcessFrame(frame++, 150);
        Assert.Equal(expectedMaster++, meta.MasterSequence); // Window 13 complete

        // Switch 8: Profile 0 -> Profile 1 at frame 33
        meta = provider.ProcessFrame(frame++, 250);
        Assert.Equal(expectedMaster, meta.MasterSequence);   // Window 14 starts
        Assert.Equal(1, meta.HdrProfile);

        provider.ProcessFrame(frame++, 350);
        meta = provider.ProcessFrame(frame++, 450);
        Assert.Equal(expectedMaster++, meta.MasterSequence); // Window 14 complete

        // Switch 9: Profile 1 -> Profile 0 at frame 36
        meta = provider.ProcessFrame(frame++, 19);
        Assert.Equal(expectedMaster, meta.MasterSequence);   // Window 15 starts
        Assert.Equal(0, meta.HdrProfile);

        meta = provider.ProcessFrame(frame++, 150);
        Assert.Equal(expectedMaster++, meta.MasterSequence); // Window 15 complete

        // Complete three more windows in Profile 0
        provider.ProcessFrame(frame++, 19);
        meta = provider.ProcessFrame(frame++, 150);
        Assert.Equal(expectedMaster++, meta.MasterSequence); // Window 16

        provider.ProcessFrame(frame++, 19);
        meta = provider.ProcessFrame(frame++, 150);
        Assert.Equal(expectedMaster++, meta.MasterSequence); // Window 17

        provider.ProcessFrame(frame++, 19);
        meta = provider.ProcessFrame(frame++, 150);
        Assert.Equal(expectedMaster++, meta.MasterSequence); // Window 18

        // Switch 10: Profile 0 -> Profile 1 at frame 44
        meta = provider.ProcessFrame(frame++, 250);
        Assert.Equal(expectedMaster, meta.MasterSequence);   // Window 19 starts
        Assert.Equal(1, meta.HdrProfile);

        provider.ProcessFrame(frame++, 350);
        meta = provider.ProcessFrame(frame++, 450);
        Assert.Equal(expectedMaster++, meta.MasterSequence); // Window 19 complete

        // Switch 11: Profile 1 -> Profile 0 at frame 47
        meta = provider.ProcessFrame(frame++, 19);
        Assert.Equal(expectedMaster, meta.MasterSequence);   // Window 20 starts
        Assert.Equal(0, meta.HdrProfile);

        meta = provider.ProcessFrame(frame++, 150);
        Assert.Equal(expectedMaster++, meta.MasterSequence); // Window 20 complete

        // Switch 12: Profile 0 -> Profile 1 at frame 49
        meta = provider.ProcessFrame(frame++, 250);
        Assert.Equal(expectedMaster, meta.MasterSequence);   // Window 21 starts
        Assert.Equal(1, meta.HdrProfile);

        provider.ProcessFrame(frame++, 350);
        meta = provider.ProcessFrame(frame++, 450);
        Assert.Equal(expectedMaster++, meta.MasterSequence); // Window 21 complete

        // Final verification - continue a bit more
        provider.ProcessFrame(frame++, 250);
        provider.ProcessFrame(frame++, 350);
        meta = provider.ProcessFrame(frame++, 450);
        Assert.Equal(expectedMaster++, meta.MasterSequence); // Window 22

        Assert.Equal(55ul, frame);  // Total frames processed
        
    }

    [Fact]
    public void ExtensiveProfileSwitching_3vs4_WithManySwitches()
    {
        // Test with different window sizes: 3 vs 4
        var provider = HdrMetadataProviderImpl.Create(logger, new uint[] { 10, 20, 30 }, new uint[] { 100, 200, 300, 400 }, out _, out _);   // Window size 4

        ulong frame = 1;
        ulong expectedMaster = 1;

        // Start with Profile 0 - complete 3 windows
        for (int w = 0; w < 3; w++)
        {
            provider.ProcessFrame(frame++, 10);
            provider.ProcessFrame(frame++, 20);
            var m = provider.ProcessFrame(frame++, 30);
            Assert.Equal(expectedMaster++, m.MasterSequence);
            Assert.Equal(0, m.HdrProfile);
        }

        // Switch to Profile 1 at frame 10
        var meta = provider.ProcessFrame(frame++, 100);
        Assert.Equal(expectedMaster, meta.MasterSequence);
        Assert.Equal(1, meta.HdrProfile);

        provider.ProcessFrame(frame++, 200);
        provider.ProcessFrame(frame++, 300);
        meta = provider.ProcessFrame(frame++, 400);
        Assert.Equal(expectedMaster++, meta.MasterSequence);

        // Continue with 10 more switches
        for (int switchNum = 0; switchNum < 10; switchNum++)
        {
            if (switchNum % 2 == 0)
            {
                // Switch to Profile 0
                meta = provider.ProcessFrame(frame++, 10);
                Assert.Equal(expectedMaster, meta.MasterSequence);
                Assert.Equal(0, meta.HdrProfile);

                provider.ProcessFrame(frame++, 20);
                meta = provider.ProcessFrame(frame++, 30);
                Assert.Equal(expectedMaster++, meta.MasterSequence);

                // Complete one more window
                provider.ProcessFrame(frame++, 10);
                provider.ProcessFrame(frame++, 20);
                meta = provider.ProcessFrame(frame++, 30);
                Assert.Equal(expectedMaster++, meta.MasterSequence);
            }
            else
            {
                // Switch to Profile 1
                meta = provider.ProcessFrame(frame++, 100);
                Assert.Equal(expectedMaster, meta.MasterSequence);
                Assert.Equal(1, meta.HdrProfile);

                provider.ProcessFrame(frame++, 200);
                provider.ProcessFrame(frame++, 300);
                meta = provider.ProcessFrame(frame++, 400);
                Assert.Equal(expectedMaster++, meta.MasterSequence);

                // Complete one more window
                provider.ProcessFrame(frame++, 100);
                provider.ProcessFrame(frame++, 200);
                provider.ProcessFrame(frame++, 300);
                meta = provider.ProcessFrame(frame++, 400);
                Assert.Equal(expectedMaster++, meta.MasterSequence);
            }
        }

        // Verify we've processed the expected number of frames and windows
        Assert.True(frame > 80);  // Should have processed many frames
        Assert.True(expectedMaster >= 25);  // Should have completed many windows
    }

    [Fact]
    public void StressTest_RapidSwitching_ShouldMaintainContinuity()
    {
        // Stress test with rapid switching between profiles
        var provider = HdrMetadataProviderImpl.Create(logger, new uint[] { 5 }, new uint[] { 10, 15 }, out _, out _);   // Window size 2

        ulong frame = 1;
        ulong expectedMaster = 1;

        // Perform 20 rapid switches (every window or two)
        for (int i = 0; i < 20; i++)
        {
            if (i % 2 == 0)
            {
                // Profile 0 - single exposure
                var m1 = provider.ProcessFrame(frame++, 5);
                Assert.Equal(expectedMaster++, m1.MasterSequence);
                Assert.Equal(0, m1.HdrProfile);

                var m2 = provider.ProcessFrame(frame++, 5);
                Assert.Equal(expectedMaster++, m2.MasterSequence);
            }
            else
            {
                // Profile 1 - two exposures
                var m1 = provider.ProcessFrame(frame++, 10);
                Assert.Equal(expectedMaster, m1.MasterSequence);
                Assert.Equal(1, m1.HdrProfile);

                var m2 = provider.ProcessFrame(frame++, 15);
                Assert.Equal(expectedMaster++, m2.MasterSequence);

                // One more window
                provider.ProcessFrame(frame++, 10);
                var m3 = provider.ProcessFrame(frame++, 15);
                Assert.Equal(expectedMaster++, m3.MasterSequence);
            }
        }

        // Final verification
        Assert.True(frame > 60);  // Should have processed many frames
        Assert.Equal(41ul, expectedMaster);  // Should have completed 40 windows
    }
}