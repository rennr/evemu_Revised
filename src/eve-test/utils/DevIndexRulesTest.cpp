/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2006 - 2021 The EVEmu Team
    For the latest information visit https://evemu.dev
    ------------------------------------------------------------------------------------
    This program is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your option) any later
    version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License along with
    this program; if not, write to the Free Software Foundation, Inc., 59 Temple
    Place - Suite 330, Boston, MA 02111-1307, USA, or go to
    http://www.gnu.org/copyleft/lesser.txt.
    ------------------------------------------------------------------------------------
*/

#include "eve-test.h"

#include "EVE_DevIndex.h"

namespace {
    int failures = 0;

    void Check(bool ok, const char* what)
    {
        ::printf("%s %s\n", (ok ? "  ok  " : "  FAIL"), what);
        if (!ok)
            ++failures;
    }

    // mine m3 in small chunks (each cycle uses the modifier of the band it starts in), as AsteroidSE::RemoveQuantity does
    int64_t Mine(int64_t points, double m3, double perM3)
    {
        const double chunk(1000.0);
        while (m3 > 0) {
            double now(m3 < chunk ? m3 : chunk);
            points = DevIndex::AddPoints(points, DevIndex::GainPoints(now * perM3, points, false));
            m3 -= now;
        }
        return points;
    }

    // the Bloodtear fit (EVE_DevIndex.h)
    static constexpr double BloodtearPerM3 = 1.6;
    static constexpr int64_t M = 1000000;
}

int utils_DevIndexRulesTest( int /*argc*/, char* /*argv*/[] )
{
    using namespace DevIndex;
    ::printf("\nDevelopment index rules\n");

    // client constants
    Check(LevelSize == 9600000, "a level is devIndexDecayRate * devIndexDecayTime = 9.6M points");
    Check(MaxPoints == 57600000, "the scale tops out at 57.6M");

    // upgrade level:  band tops strictly below the points (sovSvc.GetDevIndexLevel)
    Check(LevelForPoints(0) == 0, "0 points is level 0");
    Check(LevelForPoints(LevelSize) == 0, "exactly 9.6M is still level 0");
    Check(LevelForPoints(LevelSize + 1) == 1, "9.6M + 1 is level 1");
    Check(LevelForPoints(LevelSize * 2 + 1) == 2, "19.2M + 1 is level 2");
    Check(LevelForPoints(LevelSize * 5) == 4, "exactly 48M is level 4");
    Check(LevelForPoints(LevelSize * 5 + 1) == 5, "48M + 1 is level 5");
    Check(LevelForPoints(MaxPoints) == 5, "57.6M is level 5");
    Check(LevelForPoints(MaxPoints * 2) == 5, "the level stops at 5");

    // band (gain modifier, IHub bars)
    Check(BandForPoints(-5) == 0, "negative points are band 0");
    Check(BandForPoints(LevelSize - 1) == 0, "just under 9.6M is band 0");
    Check(BandForPoints(LevelSize) == 1, "9.6M is band 1 (the bars show level 1)");
    Check(BandForPoints(MaxPoints) == 5, "57.6M is band 5");

    // strategic days (GetTimeIndexLevelForDays)
    Check(LevelForClaimDays(0) == 0, "no days is strategic 0");
    Check(LevelForClaimDays(6) == 0, "6 days is strategic 0");
    Check(LevelForClaimDays(7) == 1, "7 days is strategic 1");
    Check(LevelForClaimDays(21) == 2, "21 days is strategic 2");
    Check(LevelForClaimDays(35) == 3, "35 days is strategic 3");
    Check(LevelForClaimDays(64) == 3, "64 days is strategic 3");
    Check(LevelForClaimDays(65) == 4, "65 days is strategic 4");
    Check(LevelForClaimDays(100) == 5, "100 days is strategic 5");
    Check(LevelForClaimDays(5000) == 5, "strategic stops at 5");

    // gain modifiers by band
    const double mil[6] = {5.0, 2.0, 1.0, 0.75, 0.5, 0.25};
    const double ind[6] = {4.0, 2.0, 1.0, 0.5, 0.25, 0.125};
    bool milOk(true), indOk(true);
    for (uint8_t band = 0; band < 6; ++band) {
        int64_t at(LevelSize * band + 1);
        milOk &= (GainPoints(1000, at, true) == (int64_t)(1000 * mil[band]));
        indOk &= (GainPoints(1000, at, false) == (int64_t)(1000 * ind[band]));
    }
    Check(milOk, "military gain modifiers 5/2/1/0.75/0.5/0.25");
    Check(indOk, "industrial gain modifiers 4/2/1/0.5/0.25/0.125");
    Check(GainPoints(0.1, 0, false) == 0, "less than a point is nothing");
    Check(GainPoints(-100, 0, true) == 0, "negative activity is nothing");

    // cap
    Check(AddPoints(MaxPoints - 10, 1000) == MaxPoints, "gain stops at 57.6M");
    Check(AddPoints(100, 1000) == 1100, "gain below the cap adds up");

    // decay
    Check(DecayAmount(FiletimeHour, DecayRate) == DecayRate, "an hour decays 100000 points");
    Check(DecayAmount(FiletimeHour * DecayTime, DecayRate) == LevelSize, "96 idle hours decay exactly one level");
    Check(DecayAmount(-FiletimeHour, DecayRate) == 0, "no decay backwards in time");
    Check(BandForPoints(DecayPoints(LevelSize * 3 + 5, DecayAmount(FiletimeHour * DecayTime, DecayRate))) == 2,
          "96 idle hours drop one band");
    Check(DecayPoints(50000, 100000) == 0, "decay stops at 0");
    Check(IsIncreasing(100001, 100000), "more gain than decay is increasing");
    Check(!IsIncreasing(100000, 100000), "gain equal to decay is not increasing");

    // GM levels:  the middle of the band
    bool gmOk(true);
    for (uint8_t level = 0; level < 6; ++level)
        gmOk &= (LevelForPoints(PointsForLevel(level)) == level) and (BandForPoints(PointsForLevel(level)) == level);
    Check(gmOk, "PointsForLevel gives that upgrade level and band");
    Check(LevelForPoints(DecayPoints(PointsForLevel(3), DecayAmount(FiletimeHour * 47, DecayRate))) == 3,
          "a GM level holds 47 idle hours");
    Check(PointsForLevel(9) == PointsForLevel(5), "PointsForLevel stops at 5");

    // bounty equivalents of npcs without a bounty
    Check(UnbountiedBountyISK(759) == 12000, "an asteroid rogue drone frigate counts like a median pirate frigate");
    Check(UnbountiedBountyISK(844) == 8812500, "an asteroid rogue drone commander battleship counts like a pirate commander one");
    Check(UnbountiedBountyISK(805) < UnbountiedBountyISK(759), "deadspace drones count less than asteroid ones, as pirate bounties do");
    Check(UnbountiedBountyISK(761) < UnbountiedBountyISK(759), "a swarm counts less than a frigate");
    Check(UnbountiedBountyISK(383) == 40000, "an unbountied destructible sentry counts the median sentry bounty");
    Check(UnbountiedBountyISK(550) == 0, "pirate groups have their own bounty");
    Check(UnbountiedBountyISK(99) == 0, "empire law guns count nothing");
    bool rising(true);
    const uint32_t asteroid[5] = {759, 758, 757, 755, 756}, commander[5] = {847, 846, 845, 843, 844}, deadspace[5] = {805, 804, 803, 801, 802};
    for (uint8_t i = 1; i < 5; ++i)
        rising &= (UnbountiedBountyISK(asteroid[i]) > UnbountiedBountyISK(asteroid[i - 1]))
              and (UnbountiedBountyISK(commander[i]) > UnbountiedBountyISK(commander[i - 1]))
              and (UnbountiedBountyISK(deadspace[i]) > UnbountiedBountyISK(deadspace[i - 1]));
    Check(rising, "bigger drone hulls count more (Dominion:  \"the bigger the NPC\")");
    // 0.1 points per isk:  a pirate battleship and a drone battleship are worth the same at level 0
    Check(GainPoints(UnbountiedBountyISK(756) * 0.1, 0, true) == 587500, "a drone battleship is 587,500 points at level 0");

    // Bloodtear Industry Index Report (2010) at 1.6 points per m3:  m3 to obtain each level from the one below
    int64_t points(0);
    points = Mine(points, 1.5 * M, BloodtearPerM3);
    Check(points == LevelSize, "1.5M m3 from 0 reaches level 1 (9.6M)");
    Check(LevelForPoints(Mine(points, 1, BloodtearPerM3)) == 1, "...and one more m3 counts for level 1 upgrades");
    points = Mine(points, 3 * M, BloodtearPerM3);
    Check(points == LevelSize * 2, "3M more m3 reaches level 2");
    points = Mine(points, 6 * M, BloodtearPerM3);
    Check(points == LevelSize * 3, "6M more m3 reaches level 3");
    points = Mine(points, 12 * M, BloodtearPerM3);
    Check(points == LevelSize * 4, "12M more m3 reaches level 4");
    points = Mine(points, 24 * M, BloodtearPerM3);
    Check(points == LevelSize * 5, "24M more m3 reaches level 5");

    // m3 a day to hold each level:  a day of mining against 24 hours of decay
    const double hold[5] = {0.75, 1.5, 3, 6, 12};
    bool holdOk(true);
    for (uint8_t level = 1; level <= 5; ++level) {
        int64_t start(PointsForLevel(level)), now(start);
        for (uint8_t hour = 0; hour < 24; ++hour) {
            now = Mine(now, hold[level - 1] * M / 24, BloodtearPerM3);
            now = DecayPoints(now, DecayAmount(FiletimeHour, DecayRate));
        }
        holdOk &= (now == start);
    }
    Check(holdOk, "0.75M/1.5M/3M/6M/12M m3 a day hold levels 1-5 flat");

    ::printf("%d failure(s)\n", failures);
    return (failures == 0 ? 0 : 1);
}
