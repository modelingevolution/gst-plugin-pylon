using System.Collections.Frozen;
using System.Collections.Immutable;
using Microsoft.Extensions.Logging;

namespace HdrMetadataProvider;

/// <summary>
/// Implementation of HDR metadata provider
/// Tracks HDR sequence position and maintains master sequence across profile switches
/// </summary>
public sealed class HdrMetadataProviderImpl : IHdrMetadataProvider
{
    private readonly record struct ProfileInfo
    {
        public ProfileInfo(in ImmutableArray<uint> Exposures)
        {
            this.Exposures = Exposures;
        }

        public ImmutableArray<uint> Exposures { get; }
        public byte WindowSize => (byte)Exposures.Length;
        
        public long GetMasterSequence(long n) => (n / WindowSize) + ((n % WindowSize) > 0 ? 1 : 0);

    }
    // Exposure to (profile, index) mapping
    private readonly FrozenDictionary<uint, (byte profile, byte index)> _exposureMap;
    private readonly ILogger<HdrMetadataProviderImpl> _logger;
    private ProfileInfo _profile0;
    private ProfileInfo _profile1;
    private byte _lastProfile = 0;
    private byte _lastSequenceIndex = 0;
    private ulong _lastFrameNumber = ulong.MaxValue;
    private long _frameOffset = 0;  // Frame offset for maintaining continuity across profile switches

    
    

    private HdrMetadataProviderImpl(ILogger<HdrMetadataProviderImpl> logger, in ImmutableArray<uint> profile0Exposures, in ImmutableArray<uint> profile1Exposures)
    {
        _logger = logger;
        _profile0 = new ProfileInfo(profile0Exposures);
        _profile1 = new ProfileInfo(profile1Exposures);
        _exposureMap = BuildExposureMap();
    }

    public static IHdrMetadataProvider Create(ILogger<HdrMetadataProviderImpl> logger, uint[] profile0Exposures,
        uint[] profile1Exposures) =>
        Create(logger, profile0Exposures, profile1Exposures, out _, out _);

    public static IHdrMetadataProvider Create(ILogger<HdrMetadataProviderImpl> logger, uint[] profile0Exposures, uint[] profile1Exposures, out uint[] adjustedProfile0, out uint[] adjustedProfile1)
    {
        var provider = new HdrMetadataProviderImpl(
            logger,
            profile0Exposures?.Length > 0 ? [.. profile0Exposures] : ImmutableArray<uint>.Empty,
            profile1Exposures?.Length > 0 ? [.. profile1Exposures] : ImmutableArray<uint>.Empty
        );

        // Build adjusted exposure arrays from the exposure map
        adjustedProfile0 = new uint[provider._profile0.WindowSize];
        adjustedProfile1 = new uint[provider._profile1.WindowSize];

        foreach (var kvp in provider._exposureMap)
        {
            if (kvp.Value.profile == 0)
                adjustedProfile0[kvp.Value.index] = kvp.Key;
            else
                adjustedProfile1[kvp.Value.index] = kvp.Key;
        }

        return provider;
    }

    
    
    public HdrMetadata ProcessFrame(ulong frameNumber, uint actualExposureTime)
    {
        if(frameNumber == 0) throw new ArgumentException("Frame number must be greater than zero");
        // Look up the exposure to find profile and index
        var (profile, index) = LookupExposure(actualExposureTime);

        // Get exposure count for current profile
        byte exposureCount = profile == 0 ? _profile0.WindowSize : _profile1.WindowSize;
        
        // Detect profile switch
        if (_lastFrameNumber != ulong.MaxValue && profile != _lastProfile)
        {
            ref ProfileInfo prvProfile = ref GetProfile(_lastProfile);
            ref ProfileInfo newProfile = ref GetProfile(profile);

            CalculateFrameOffset(frameNumber, prvProfile, newProfile, index);

            _logger.LogInformation("Profile switch at frame {Frame}: {OldProfile} -> {NewProfile}, index={Index}, offset k={Offset}, total offset={TotalOffset}",
                frameNumber, _lastProfile, profile, index, _frameOffset, this._frameOffset);
        }

        // Calculate master sequence with offset
        long adjustedFrame = (long)frameNumber + _frameOffset;
        var masterSequence = GetProfile(profile).GetMasterSequence(adjustedFrame);

        // Create metadata
        var metadata = new HdrMetadata
        {
            MasterSequence = (ulong)masterSequence,
            ExposureSequenceIndex = index,
            ExposureCount = exposureCount,
            ExposureValue = actualExposureTime,
            HdrProfile = profile
        };

        // Update state for next frame
        _lastProfile = profile;
        _lastSequenceIndex = index;
        _lastFrameNumber = frameNumber;

        return metadata;
    }
    private void CalculateFrameOffset(ulong n, in ProfileInfo prv, in ProfileInfo nx, byte sequenceIndex)
    {
        var prv_m = prv.GetMasterSequence((long)n + _frameOffset);

        var mid_prv = prv.WindowSize > 1 && _lastSequenceIndex < prv.WindowSize - 1;

        var adjustedFrameNumber = (long)(prv_m * nx.WindowSize);
        _frameOffset = adjustedFrameNumber - (long)n; // min

        for(int i = 1; i <= nx.WindowSize; i++)
        {
            if (nx.GetMasterSequence((long)n + _frameOffset - 1) == prv_m)
                _frameOffset -= 1;
            else break;
        }

        _frameOffset += sequenceIndex;

        var start_nx = sequenceIndex == 0;
        if (!mid_prv || !start_nx) return;
        if (nx.GetMasterSequence((long)n + _frameOffset) == prv_m)
            _frameOffset += nx.WindowSize;
    }
    

    private FrozenDictionary<uint, (byte profile, byte index)> BuildExposureMap()
    {
        // Check if at least one profile is configured
        if (_profile0.WindowSize == 0 && _profile1.WindowSize == 0)
        {
            return FrozenDictionary<uint, (byte profile, byte index)>.Empty;
        }

        var tempMap = new Dictionary<uint, (byte profile, byte index)>();

        // Add profile 0 exposures
        for (byte i = 0; i < _profile0.WindowSize; i++)
        {
            tempMap[_profile0.Exposures[i]] = (0, i);
        }

        // Add profile 1 exposures
        for (byte i = 0; i < _profile1.WindowSize; i++)
        {
            tempMap[_profile1.Exposures[i]] = (1, i);
        }

        // Handle duplicates
        HandleDuplicateExposures(tempMap);

        // Freeze the map for better performance
        return tempMap.ToFrozenDictionary();
    }

    private void HandleDuplicateExposures(Dictionary<uint, (byte profile, byte index)> tempMap)
    {
        var seen = new HashSet<uint>();
        var duplicates = new List<uint>();

        // Find duplicates across both profiles
        foreach (var exp in _profile0.Exposures)
        {
            if (!seen.Add(exp))
                duplicates.Add(exp);
        }

        foreach (var exp in _profile1.Exposures)
        {
            if (!seen.Add(exp))
                duplicates.Add(exp);
        }

        // Adjust duplicates
        foreach (var dupExposure in duplicates)
        {
            // Find which profiles have this exposure
            byte? profile0Index = null;
            byte? profile1Index = null;

            for (byte i = 0; i < _profile0.WindowSize; i++)
            {
                if (_profile0.Exposures[i] != dupExposure) continue;
                profile0Index = i;
                break;
            }

            for (byte i = 0; i < _profile1.WindowSize; i++)
            {
                if (_profile1.Exposures[i] != dupExposure) continue;
                profile1Index = i;
                break;
            }

            // If found in both, adjust profile 1's value
            if (!profile0Index.HasValue || !profile1Index.HasValue) continue;
            uint adjustedExposure = dupExposure;

            // Find a unique value by incrementing
            while (tempMap.ContainsKey(adjustedExposure))
            {
                adjustedExposure++;
            }

            // Update mapping - Profile 0 keeps original, Profile 1 gets adjusted
            tempMap.Remove(dupExposure);
            tempMap[dupExposure] = (0, profile0Index.Value);
            tempMap[adjustedExposure] = (1, profile1Index.Value);

            _logger.LogWarning("Duplicate exposure {DupExposure}μs. Profile 1 index {Index} adjusted to {AdjustedExposure}μs",
                dupExposure, profile1Index.Value, adjustedExposure);
        }
    }

    private ref ProfileInfo GetProfile(byte profile)
    {
        if (profile == 0) return ref this._profile0;
        return ref this._profile1;
    }

    private (byte profile, byte index) LookupExposure(uint exposureTime)
    {
        if (_exposureMap.TryGetValue(exposureTime, out var result))
        {
            return result;
        }

        // This should not happen - exposure values should exactly match configured values
        throw new ArgumentException($"Unexpected exposure time {exposureTime}μs not found in configured sequences");
    }
}