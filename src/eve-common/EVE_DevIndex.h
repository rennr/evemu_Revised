 /**
  * @name EVE_DevIndex.h
  *     sovereignty development indices (military, industrial, strategic) as the Crucible client draws and gates them
  *
  * these are pure functions, so the unit tests (eve-test) can check them
  *
  *  the client (common/script/sys/devIndexUtil.py) runs the military and industrial indices in six bands (levels 0..5)
  *  of devIndexDecayRate * devIndexDecayTime = 100000 * 96 = 9.6M points each, so an idle index loses one level every
  *  96 hours.  each band has a gain modifier:  activity is worth less the higher the index already is.
  *  the strategic ("Sovereignty") index is whole days since the claim:  levels 1-5 at 7/21/35/65/100 days (timeIndexLevels).
  *   upgrades gate on sovSvc.GetDevIndexLevel:  the count of band tops strictly below the points
  *   the IHub bars draw sovSvc.GetIndexLevel:  the band the points are in, 5 from 57.6M up
  *  the Bloodtear alliance's 2010 Industry Index Report fits these constants exactly at 1.6 points per m3 mined:
  *   1.5M/3M/6M/12M/24M m3 to reach levels 1-5, 0.75M/1.5M/3M/6M/12M m3 a day to hold them
  */


#ifndef EVE_DEV_INDEX_H
#define EVE_DEV_INDEX_H

#include <cstdint>


namespace DevIndex {

    const int64_t DecayRate     = 100000;               // devIndexDecayRate, points per hour
    const int64_t DecayTime     = 96;                   // devIndexDecayTime, hours per band
    const int64_t LevelSize     = DecayRate * DecayTime;    // 9.6M
    const int64_t MaxPoints     = LevelSize * 6;        // top of the level 5 band
    const int64_t FiletimeHour  = 36000000000LL;        // 100ns ticks

    // gain modifiers by current band
    const double MilitaryMod[6]   = {5.0, 2.0, 1.0, 0.75, 0.5, 0.25};
    const double IndustrialMod[6] = {4.0, 2.0, 1.0, 0.5, 0.25, 0.125};
    // days of sovereignty for strategic levels 1..5  (timeIndexLevels)
    const uint8_t TimeLevelDays[6] = {0, 7, 21, 35, 65, 100};

    // level used for upgrade requirements:  count of band tops strictly below points, max 5  (sovSvc.GetDevIndexLevel)
    inline uint8_t LevelForPoints(int64_t points)
    {
        uint8_t level(0);
        while ((level < 5) and (LevelSize * (level + 1) < points))
            ++level;
        return level;
    }

    // band the points are in, for gain modifiers.  also the level the IHub bars show (sovSvc.GetIndexLevel)
    inline uint8_t BandForPoints(int64_t points)
    {
        if (points <= 0)
            return 0;
        int64_t band(points / LevelSize);
        return (band > 5 ? 5 : (uint8_t)band);
    }

    // highest strategic level reached after this many whole days  (GetTimeIndexLevelForDays)
    inline uint8_t LevelForClaimDays(int64_t days)
    {
        for (uint8_t level = 5; level > 0; --level)
            if (days >= TimeLevelDays[level])
                return level;
        return 0;
    }

    // rawPoints of activity (before the modifier) are worth this many points at currentPoints.  0 below one point
    inline int64_t GainPoints(double rawPoints, int64_t currentPoints, bool military)
    {
        if (rawPoints <= 0)
            return 0;
        uint8_t band(BandForPoints(currentPoints));
        int64_t points((int64_t)(rawPoints * (military ? MilitaryMod[band] : IndustrialMod[band])));
        return (points < 1 ? 0 : points);
    }

    // points after a gain, capped at the top of the scale
    inline int64_t AddPoints(int64_t points, int64_t gain)
    {
        int64_t sum(points + gain);
        return (sum > MaxPoints ? MaxPoints : sum);
    }

    // linear decay over elapsed filetime at perHour points an hour
    inline int64_t DecayAmount(int64_t elapsed, double perHour)
    {
        if (elapsed <= 0)
            return 0;
        return (int64_t)(perHour * ((double)elapsed / FiletimeHour));
    }

    // points after a decay, floored at 0
    inline int64_t DecayPoints(int64_t points, int64_t decay)
    {
        int64_t left(points - decay);
        return (left < 0 ? 0 : left);
    }

    // the IHub's up/down hint:  the period's activity outpaced its decay
    inline bool IsIncreasing(int64_t gain, int64_t decay)
    {
        return (gain > decay);
    }

    /* GM:  the points that put an index at level (0-5):  the middle of that band, so an idle index keeps the level
     *  for two days (48 h of decay) before it drops.  0 for level 0 */
    inline int64_t PointsForLevel(uint8_t level)
    {
        if (level == 0)
            return 0;
        if (level > 5)
            level = 5;
        return (LevelSize * level) + (LevelSize / 2);
    }
}

#endif  // EVE_DEV_INDEX_H
