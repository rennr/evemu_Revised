 /**
  * @name FacilitiesTest.cpp
  *   unit tests of the sim's facility capacity (market/sim/SimFacilities, SimLineCapacity, PLAN 6.6):  the policy, the
  *   player guarantees and the book on the harness' lines, and the world's view of it (Prod::ICapacity)
  *
  * @date:          15Oct26
  *
  */

#include <algorithm>
#include <cstdlib>
#include <array>
#include <initializer_list>
#include <map>
#include <set>
#include <vector>

#include "HarnessData.h"
#include "HarnessFacilities.h"
#include "TestCheck.h"
#include "market/sim/SimFacilities.h"
#include "market/sim/SimLineCapacity.h"
#include "market/sim/SimLineStations.h"

using namespace MktSim;

namespace {
    const int64_t T0 = 1323043200;      // 2011-12-05 00:00 UTC
    const uint32_t Jita = 60003760;

    JobRequest Request(uint32_t actorID, uint32_t stationID, double hours, uint32_t runs = 1)
    {
        JobRequest req;
        req.actorID = actorID;
        req.activity = LineActivity::Manufacturing;
        req.stationID = stationID;
        req.outputTypeID = 587;     // Rifter
        req.runs = runs;
        req.unitsPerRun = 1;
        req.secondsPerRun = hours * 3600.0 / runs;
        return req;
    }

    // every pending line write of the book, by line
    std::map<uint32_t, FacilityBook::LineWrite> Writes(const FacilityBook& book)
    {
        std::vector<FacilityBook::LineWrite> list;
        book.PendingLineWrites(list);
        std::map<uint32_t, FacilityBook::LineWrite> out;
        for (auto& w : list)
            out[w.lineID] = w;
        return out;
    }

    // what a source line holds after a write (FactoryDB::SetNextFreeTimes)
    int64_t Apply(int64_t current, const FacilityBook::LineWrite& w)
    {
        if (w.hasGuard and (current == w.guard))
            return w.value;
        return std::max(current, w.floor);
    }

    // the harness' lines with a line check:  lines in 'player' have a player's job, lines in 'gone' don't exist
    class CheckedLines : public HarnessFacilities {
    public:
        explicit CheckedLines(const FacilityParams& p) : HarnessFacilities(p) { }
        std::set<uint32_t> player, gone;
    protected:
        bool CheckLines(const std::vector<uint32_t>& lineIDs, std::map<uint32_t, std::pair<int64_t, bool>>& into) override {
            for (auto id : lineIDs)
                if (gone.count(id) == 0)
                    into[id] = std::make_pair((int64_t)(T0 + 777), player.count(id) > 0);
            return true;
        }
    };
}

namespace {
    /* node stations (SimLineStations):  a hand-made map.  system: security, region, constellation;  its stations
     *  (manufacturing / invention lines)
     *    1   0.9  R1 C1   100 (50/0)          the node A          11  0.9 R1 C1  1100 (50/0), 1101 (10/0)
     *    12  0.2  R1 C1   1200 (50/0)         low-sec, its constellation's (kept)
     *    2   0.8  R1 C2   200 (50/20), 201 (50/0)          3   0.7 R2 C3  300 (50/0)
     *    4   0.3  R1 C4   400 (50/20)  low-sec             5   0.9 R1 C5  500 (50/20)  reached only through 4
     *    6   0.6  R1 C2   600 (0/20), 601 (50/0)           7   0.6 R1 C6  700 (50/0), 701 (50/20)
     *    8   0.6  R1 C6   800 (50/0)          9   0.6  R1 C6  900 (50/20)          10  0.6 R1 C6  1000 (50/20)
     *  jumps:  1-2, 1-3, 1-4, 4-5, 1-11, 11-12, 2-6, 6-7, 7-8, 8-9, 9-10 */
    struct TestMap {
        std::map<uint32_t, MapSystem> systems;
        std::vector<std::pair<uint32_t, uint32_t>> jumps;
        std::vector<LineStation> stations;
    };

    TestMap MakeMap()
    {
        TestMap m;
        auto sys = [&](uint32_t id, float sec, uint32_t region, uint32_t con) {
            MapSystem s;
            s.security = sec;
            s.regionID = region;
            s.constellationID = con;
            m.systems[id] = s;
        };
        sys(1, 0.9f, 1, 1); sys(11, 0.9f, 1, 1); sys(12, 0.2f, 1, 1); sys(2, 0.8f, 1, 2); sys(3, 0.7f, 2, 3);
        sys(4, 0.3f, 1, 4); sys(5, 0.9f, 1, 5); sys(6, 0.6f, 1, 2); sys(7, 0.6f, 1, 6); sys(8, 0.6f, 1, 6);
        sys(9, 0.6f, 1, 6); sys(10, 0.6f, 1, 6);
        const uint32_t j[][2] = { {1, 2}, {1, 3}, {1, 4}, {4, 5}, {1, 11}, {11, 12}, {2, 6}, {6, 7}, {7, 8}, {8, 9}, {9, 10} };
        for (auto& e : j)
            m.jumps.push_back(std::make_pair(e[0], e[1]));
        auto st = [&](uint32_t id, uint32_t systemID, uint16_t mfg, uint16_t inv) {
            LineStation s;
            s.stationID = id;
            s.systemID = systemID;
            s.constellationID = m.systems[systemID].constellationID;
            s.regionID = m.systems[systemID].regionID;
            s.mfgLines = mfg;
            s.invLines = inv;
            s.refinery = ((id % 200) == 100);       // 100, 300, 500, 700, 900, 1100
            m.stations.push_back(s);
        };
        st(100, 1, 50, 0); st(1100, 11, 50, 0); st(1101, 11, 10, 0); st(1200, 12, 50, 0); st(200, 2, 50, 20); st(201, 2, 50, 0);
        st(300, 3, 50, 0); st(400, 4, 50, 20); st(500, 5, 50, 20); st(600, 6, 0, 20); st(601, 6, 50, 0); st(700, 7, 50, 0);
        st(701, 7, 50, 20); st(800, 8, 50, 0); st(900, 9, 50, 20); st(1000, 10, 50, 20);
        return m;
    }

    NodeData TestNode(uint32_t nodeID, uint32_t stationID, uint32_t systemID, const TestMap& m)
    {
        NodeData n = NodeData();
        n.nodeID = nodeID;
        n.stationID = stationID;
        n.systemID = systemID;
        n.regionID = m.systems.at(systemID).regionID;
        n.security = m.systems.at(systemID).security;
        return n;
    }

    std::vector<uint32_t> Chosen(const std::vector<RankedStation>& ranked, const NodeChoice& c)
    {
        std::vector<uint32_t> out;
        for (auto i : c.chosen)
            out.push_back(ranked[i].stationID);
        return out;
    }

    std::vector<uint32_t> Ids(std::initializer_list<uint32_t> l)    { return std::vector<uint32_t>(l); }

    int NodeStationsTest()
    {
        TestMap m(MakeMap());
        NodeStationParams sp;        // the defaults:  6 / 2 stations, 5 jumps, headroom 2, 60 at most, same band
        NodeData a(TestNode(1, 100, 1, m));
        std::vector<RankedStation> ra(LineStations::Rank(a, m.stations, m.systems, m.jumps, sp));
        double zero[2] = {0.0, 0.0};

        /* the ranked list:  the node's station, its constellation's (its system's first, then by stationID, the low-sec one
         *  kept), then high-sec stations reached through high-sec by jumps, the node's region first, then stationID;  400
         *  (low-sec), 500 (only through low-sec) and 1000 (6 jumps) are not candidates */
        {
            std::vector<uint32_t> ids;
            for (auto& r : ra)
                ids.push_back(r.stationID);
            T_CHECK(ids == Ids({100, 1100, 1101, 1200, 200, 201, 300, 600, 601, 700, 701, 800, 900}));
            T_CHECK(ra[3].base and !ra[4].base);
            T_CHECK_EQ(ra[3].jumps, 2);         // 1200:  its real distance (for the freight)
            T_CHECK_EQ(ra[7].jumps, 2);         // 600
            T_CHECK_EQ(ra[12].jumps, 5);        // 900
            T_CHECK_EQ(ra[0].simCap[LineKind::Mfg], 15);
            T_CHECK_EQ(ra[2].simCap[LineKind::Mfg], 3);     // 10 lines:  floor(3), 8 left free
            T_CHECK_EQ(ra[4].simCap[LineKind::Inv], 6);     // 20 invention lines
            // deterministic
            for (int k = 0; k < 3; ++k) {
                std::vector<RankedStation> again(LineStations::Rank(a, m.stations, m.systems, m.jumps, sp));
                T_CHECK_EQ(again.size(), ra.size());
                for (size_t i = 0; (i < again.size()) and (i < ra.size()); ++i)
                    T_CHECK_EQ(again[i].stationID, ra[i].stationID);
            }
            // a low-sec node takes any route:  500 at 1 jump, 100 at 1
            NodeData b(TestNode(2, 400, 4, m));
            std::vector<RankedStation> rb(LineStations::Rank(b, m.stations, m.systems, m.jumps, sp));
            std::map<uint32_t, uint16_t> jb;
            for (auto& r : rb)
                jb[r.stationID] = r.jumps;
            T_CHECK(jb.count(500) and (jb[500] == 1));
            T_CHECK(jb.count(100) and (jb[100] == 1));
            T_CHECK(jb.count(1200) and (jb[1200] == 3));
            // NodeStationSameBand false:  the high-sec node takes them too
            NodeStationParams any(sp);
            any.sameBand = false;
            std::vector<RankedStation> rany(LineStations::Rank(a, m.stations, m.systems, m.jumps, any));
            bool has400(false), has500(false);
            for (auto& r : rany) {
                has400 = has400 or (r.stationID == 400);
                has500 = has500 or (r.stationID == 500);
            }
            T_CHECK(has400 and has500);
        }

        /* the floors:  6 manufacturing, 2 invention stations.  the base has 4 and 0;  200 brings one of each, 201 the
         *  sixth manufacturing, 300 (manufacturing only) is skipped, 600 (invention only) the second invention */
        {
            std::map<uint32_t, std::array<double, LineKind::Count>> claimed;
            NodeChoice c(LineStations::Choose(ra, zero, sp, claimed));
            T_CHECK(Chosen(ra, c) == Ids({100, 1100, 1101, 1200, 200, 201, 600}));
            T_CHECK_EQ(c.stations[LineKind::Mfg], 6);
            T_CHECK_EQ(c.stations[LineKind::Inv], 2);
            T_CHECK_EQ(c.added, 3);
            T_CHECK_EQ(c.farthest, 2);
            T_CHECK(c.shortfall[0] == 0.0 and c.shortfall[1] == 0.0 and c.virtualLines[0] == 0 and c.virtualLines[1] == 0);
            // the floors claim nothing (no need)
            T_CHECK(claimed[200][LineKind::Mfg] == 0.0);
        }

        /* a constellation at its targets keeps exactly its list;  all floors 0 without the need term:  today's rule */
        {
            NodeStationParams low(sp);
            low.minStations[LineKind::Mfg] = 3;
            low.minStations[LineKind::Inv] = 1;
            NodeData c2(TestNode(3, 200, 2, m));
            std::vector<RankedStation> r2(LineStations::Rank(c2, m.stations, m.systems, m.jumps, low));
            std::map<uint32_t, std::array<double, LineKind::Count>> claimed;
            NodeChoice c(LineStations::Choose(r2, zero, low, claimed));
            T_CHECK(Chosen(r2, c) == Ids({200, 201, 600, 601}));
            T_CHECK_EQ(c.added, 0);
            NodeStationParams old(sp);
            old.minStations[0] = old.minStations[1] = 0;
            old.scaleWithNeed = false;
            double big[2] = {1000.0, 1000.0};
            NodeChoice o(LineStations::Choose(ra, big, old, claimed));
            T_CHECK(Chosen(ra, o) == Ids({100, 1100, 1101, 1200}));
            T_CHECK(o.shortfall[0] == 0.0);
        }

        /* the need term:  wanted = need x 2.  base 48 manufacturing sim lines (15 + 15 + 3 + 15);  wanted 80 takes 200, 201,
         *  300 (93);  twice the need, 160, takes 601 ... 900 (168):  what it counts follows what it wants, within a station */
        {
            NodeStationParams np(sp);
            np.minStations[0] = np.minStations[1] = 0;
            std::map<uint32_t, std::array<double, LineKind::Count>> claimed, claimed2;
            double n40[2] = {40.0, 0.0}, n80[2] = {80.0, 0.0};
            NodeChoice c(LineStations::Choose(ra, n40, np, claimed));
            T_CHECK(Chosen(ra, c) == Ids({100, 1100, 1101, 1200, 200, 201, 300}));
            T_CHECK(c.wanted[0] == 80.0 and c.have[0] == 93.0);
            NodeChoice d(LineStations::Choose(ra, n80, np, claimed2));
            T_CHECK(Chosen(ra, d) == Ids({100, 1100, 1101, 1200, 200, 201, 300, 601, 700, 701, 800, 900}));
            T_CHECK((d.have[0] >= 160.0) and (d.have[0] - 160.0 < 15.0) and (c.have[0] - 80.0 < 15.0));
            // what it claims is what it wants:  300's 15 sim lines, 2 of them
            T_CHECK(claimed[300][0] == 2.0);
            /* a second node wanting the same stations counts only what the first left (nodeID order) */
            NodeChoice e(LineStations::Choose(ra, n40, np, claimed));
            T_CHECK(e.have[0] >= 80.0);
            T_CHECK(Chosen(ra, e) == Ids({100, 1100, 1101, 1200, 300, 601, 700, 701, 800, 900}));     // 200, 201 have nothing left
            T_CHECK(e.have[0] == 13.0 + 5 * 15.0);
            /* the caps:  6 stations at most, or 5 jumps (no 1000):  the shortfall to the line, served by counted lines */
            NodeStationParams cap(np);
            cap.maxStations = 6;
            std::map<uint32_t, std::array<double, LineKind::Count>> cl3;
            NodeChoice f(LineStations::Choose(ra, n80, cap, cl3));
            T_CHECK_EQ(f.count, 6);
            T_CHECK(f.shortfall[0] == 160.0 - 78.0);
            T_CHECK_EQ(f.virtualLines[0], 82);
            double n200[2] = {100.0, 50.0};
            std::map<uint32_t, std::array<double, LineKind::Count>> cl4;
            NodeChoice g(LineStations::Choose(ra, n200, np, cl4));
            T_CHECK_EQ(g.count, ra.size());     // everything within 5 jumps
            T_CHECK(g.shortfall[0] == 200.0 - (48.0 + 8 * 15.0));
            T_CHECK(g.shortfall[1] == 100.0 - 4 * 6.0);
            T_CHECK_EQ(g.virtualLines[1], 76);
            NodeStationParams nofill(np);
            nofill.virtualFill = false;
            std::map<uint32_t, std::array<double, LineKind::Count>> cl5;
            T_CHECK_EQ(LineStations::Choose(ra, n200, nofill, cl5).virtualLines[1], 0);
        }

        /* the operator's list (NodeStations) */
        {
            std::map<uint32_t, std::vector<uint32_t>> o(LineStations::ParseOverride("100:300,201;  x;7:1"));
            T_CHECK(o[100] == Ids({300, 201}));
            T_CHECK(o[7] == Ids({1}));
            std::vector<RankedStation> rf(LineStations::Rank(a, m.stations, m.systems, m.jumps, sp, &o[100]));
            T_CHECK_EQ(rf.size(), 3);
            T_CHECK(rf[1].stationID == 300 and rf[1].base);
        }

        /* the world's view on these stations (LineCapacity):  before Finalize the floors;  then the need;  counted lines for
         *  the shortfall;  freight;  restore;  shared stations;  refining at the node's stations */
        FacilityParams p;
        auto makeLines = [&](HarnessFacilities& fac) {
            for (auto d : { LineActivity::Manufacturing, LineActivity::ResearchTime, LineActivity::ResearchMaterial,
                            LineActivity::Copying, LineActivity::Invention })
                fac.SetDefaultLines(d, 0);
            for (auto& st : m.stations) {
                fac.SetStationLines(st.stationID, LineActivity::Manufacturing, st.mfgLines);
                fac.SetStationLines(st.stationID, LineActivity::Invention, st.invLines);
            }
        };
        {
            HarnessFacilities fac(p);
            makeLines(fac);
            LineCapacity cap(fac, 1000.0, 333.0);
            cap.SetStationParams(sp);
            cap.SetFreight(true, 1.5);
            cap.SetRefineAtNodeStations(true);
            cap.SetNodeRanking(1, ra);
            cap.SetSlots(41, 1000);
            // before Finalize:  the floors' stations (7:  5 x 15 + 3 manufacturing sim lines)
            T_CHECK_EQ(cap.Free(41, 1, LineActivity::Manufacturing, T0), 78);
            T_CHECK_EQ(cap.Free(41, 1, LineActivity::Invention, T0), 12);
            cap.SetLineNeed(1, LineActivity::Manufacturing, 30.0);
            cap.SetLineNeed(1, LineActivity::Manufacturing, 30.0);      // summed:  60, wanted 120
            cap.SetLineNeed(1, LineActivity::Invention, 50.0);          // wanted 100:  24 at stations, 76 counted
            cap.Finalize();
            const LineCapacity::NodeLines* n(cap.Node(1));
            T_CHECK(n != nullptr);
            T_CHECK_EQ(n->virtualLines[LineKind::Inv], 76);
            // invention first:  200, 600, 701, 900;  then manufacturing (93 of 120):  201, 300
            T_CHECK(n->stations == Ids({100, 1100, 1101, 1200, 200, 201, 300, 600, 701, 900}));
            T_CHECK_EQ(cap.Free(41, 1, LineActivity::Invention, T0), 24 + 76);
            T_CHECK_EQ(cap.Jumps(1, 600), 2);
            T_CHECK_EQ(cap.Jumps(1, 100), 0);
            // the invention jobs:  the stations first, then the counted lines, then none
            std::vector<SimJob> jobs;
            uint32_t onStations(0), onVirtual(0);
            for (int i = 0; i < 120; ++i) {
                SimJob job;
                job.actorID = 41;
                job.nodeID = 1;
                job.stationID = 100;
                job.activity = LineActivity::Invention;
                job.typeID = 1;
                job.runs = 1;
                job.qtyOut = 1;
                job.start = T0;
                job.end = T0 + 3600 + i;
                if (!cap.Reserve(job, T0))
                    break;
                if (job.virtualLine) {
                    ++onVirtual;
                    T_CHECK(job.lineID == 0 and job.stationID == 100);
                    T_CHECK(onStations == 24);      // only when the stations are full
                } else {
                    ++onStations;
                }
                jobs.push_back(job);
            }
            T_CHECK_EQ(onStations, 24);
            T_CHECK_EQ(onVirtual, 76);
            T_CHECK_EQ(n->stats[LineKind::Inv].peak, 100);
            T_CHECK(n->stats[LineKind::Inv].reserveShort == 1);
            T_CHECK_EQ(cap.Free(41, 1, LineActivity::Invention, T0), 0);
            // a counted job costs what a line would, pays no freight, and frees its counted line
            SimJob& v(jobs.back());
            T_CHECK(v.virtualLine);
            T_CHECK(cap.JobCost(v) == 1000.0 + 2 * 333.0);
            T_CHECK(cap.Freight(v, 100.0) == 0.0);
            T_CHECK_EQ(cap.CurrentLine(v), 0);
            cap.Release(v);
            T_CHECK_EQ(cap.Free(41, 1, LineActivity::Invention, T0), 1);
            // freight:  m3 x jumps x 1.5 away from the node, 0 at it
            SimJob far(jobs[0]);
            far.stationID = 600;
            T_CHECK(cap.Freight(far, 10.0) == 30.0);
            far.stationID = 200;
            T_CHECK(cap.Freight(far, 10.0) == 15.0);
            far.stationID = 100;
            T_CHECK(cap.Freight(far, 10.0) == 0.0);
            cap.SetFreight(false, 1.5);
            far.stationID = 600;
            T_CHECK(cap.Freight(far, 10.0) == 0.0);
            // a restart:  the counted jobs come back on the counted lines, the others on their stations' lines
            HarnessFacilities fac2(p);
            makeLines(fac2);
            LineCapacity cap2(fac2, 1000.0, 333.0);
            cap2.SetStationParams(sp);
            cap2.SetNodeRanking(1, ra);
            cap2.SetLineNeed(1, LineActivity::Invention, 50.0);
            cap2.Finalize();
            cap2.SetSlots(41, 1000);
            for (size_t i = 0; i + 1 < jobs.size(); ++i)
                cap2.Restore(jobs[i]);
            T_CHECK_EQ(fac2.Jobs(), 24);
            T_CHECK_EQ(cap2.Node(1)->virtualUsed[LineKind::Inv], 75);
            T_CHECK_EQ(cap2.Free(41, 1, LineActivity::Invention, T0), 1);
            // a job saved at a station the node no longer has (300 for invention, say a config change):  it holds its
            //  line until its end and frees it, nothing is booked elsewhere
            SimJob gone(jobs[0]);
            gone.stationID = 1000;          // 6 jumps:  not in the list
            gone.activity = LineActivity::Manufacturing;
            gone.end = T0 + 50000;
            cap2.Restore(gone);
            T_CHECK_EQ(fac2.Jobs(), 25);
            cap2.Release(gone);
            T_CHECK_EQ(fac2.Jobs(), 24);
            // refining:  the node's station first (2e6 m3 an hour), then its other refineries in list order
            T_CHECK(cap.RefineAllowanceAtNode(1, 100, 3e6, T0) == 3e6);
            T_CHECK(fac.RefineAllowance(100, 1.0, T0) == 0.0);          // the node's station's hour is used
            T_CHECK(fac.RefineAllowance(1100, 1e6, T0) == 1e6);         // then 1100's (the next refinery):  1e6 of it
            T_CHECK(fac.RefineAllowance(1100, 2e6, T0) == 0.0);
            T_CHECK(cap.RefineAllowanceAtNode(1, 100, 1e6, T0) > 0.0);  // 300's, 700's, 900's still
            cap.SetRefineAtNodeStations(false);
            T_CHECK(cap.RefineAllowanceAtNode(1, 100, 1e6, T0) == 0.0);
        }

        /* two nodes share a station:  together the sim holds at most the station's share of it, and a player still takes a
         *  line at once (the book's policy is per station) */
        {
            HarnessFacilities fac(p);
            makeLines(fac);
            LineCapacity cap(fac, 1000.0, 333.0);
            cap.SetNodeStations(1, Ids({100, 200}));
            cap.SetNodeStations(2, Ids({300, 200}));
            cap.SetSlots(51, 100);
            cap.SetSlots(52, 100);
            uint32_t at200(0);
            for (int i = 0; i < 100; ++i) {
                SimJob job;
                job.actorID = (i % 2) ? 52 : 51;
                job.nodeID = (i % 2) ? 2 : 1;
                job.stationID = (i % 2) ? 300 : 100;
                job.activity = LineActivity::Manufacturing;
                job.typeID = 1;
                job.runs = 1;
                job.qtyOut = 1;
                job.start = T0;
                job.end = T0 + 3600 + i;
                if (cap.Reserve(job, T0) and (job.stationID == 200))
                    ++at200;
            }
            T_CHECK_EQ(at200, 15);
            FacilityBook::StationUse use;
            T_CHECK(fac.Use(200, LineActivity::Manufacturing, T0, use));
            T_CHECK_EQ(use.simBusy, 15);
            T_CHECK(use.free >= 2);
            std::vector<FacilityJob> held;
            fac.JobsOf(0, held);
            uint32_t line(0);
            for (auto& j : held)
                if ((j.stationID == 200) and (j.lineID != 0))
                    line = j.lineID;
            T_CHECK(line != 0);
            T_CHECK(fac.PlayerTakesLine(line, T0 + 7200, T0 + 7200, T0));
            T_CHECK_EQ(fac.HeldBy(line), 0);
        }
        return MktTest::failures;
    }
}

int FacilitiesTest()
{
    /* the policy */
    T_CHECK_EQ(Capacity::ShareCap(50, 0.3), 15);
    T_CHECK_EQ(Capacity::ShareCap(10, 0.3), 3);
    T_CHECK_EQ(Capacity::ShareCap(3, 0.3), 0);
    T_CHECK_EQ(Capacity::ShareCap(20, 1.5), 20);
    T_CHECK_EQ(Capacity::ShareCap(20, 0.0), 0);
    // the share and the free lines both limit;  the free lines a player needs are never taken
    T_CHECK_EQ(Capacity::Allowed(50, 50, 0, 0.3, 2), 15);
    T_CHECK_EQ(Capacity::Allowed(50, 50, 15, 0.3, 2), 0);
    T_CHECK_EQ(Capacity::Allowed(50, 5, 0, 0.3, 2), 3);
    T_CHECK_EQ(Capacity::Allowed(50, 2, 0, 0.3, 2), 0);
    T_CHECK_EQ(Capacity::Allowed(50, 1, 0, 0.3, 2), 0);
    T_CHECK_EQ(Capacity::Allowed(10, 10, 2, 0.3, 2), 1);
    T_CHECK_EQ(Capacity::Allowed(0, 0, 0, 0.3, 0), 0);
    T_CHECK(Capacity::CanMove(3, 2));
    T_CHECK(!Capacity::CanMove(2, 2));
    // jobs are cut at the longest job, at least one run
    T_CHECK_EQ(Capacity::RunsPerJob(100, 3600.0, 24.0), 24);
    T_CHECK_EQ(Capacity::RunsPerJob(10, 3600.0, 24.0), 10);
    T_CHECK_EQ(Capacity::RunsPerJob(5, 40.0 * 3600.0, 24.0), 1);
    T_CHECK_EQ(Capacity::RunsPerJob(0, 3600.0, 24.0), 0);
    T_CHECK_EQ(Capacity::JobSeconds(3, 1000.5, 1.0), 3002);
    T_CHECK_EQ(Capacity::JobSeconds(1, 0.0, 1.0), 1);
    // install plus every started hour (RamMethods::Calculate)
    T_CHECK_EQ(Capacity::JobCost(1000.0, 333.0, 3600), 133300);
    T_CHECK_EQ(Capacity::JobCost(1000.0, 333.0, 3601), 166600);
    T_CHECK_EQ(Capacity::JobCost(0.0, 0.0, 7200), 0);
    T_CHECK(Capacity::Refill(0.0, 2e6, 1800) == 1e6);
    T_CHECK(Capacity::Refill(1.5e6, 2e6, 3600) == 2e6);

    FacilityParams p;   // the plan's defaults:  real lines, 0.3, 2 free, 24 hours, 2e6 m3/h

    /* a hub with 50 manufacturing lines:  the sim takes 15, on 15 different lines, then is refused */
    {
        HarnessFacilities fac(p);
        T_CHECK_EQ(fac.Available(Jita, LineActivity::Manufacturing, T0), 15);
        std::set<uint32_t> lines;
        uint32_t made(0);
        for (int i = 0; i < 40; ++i) {
            FacilityJob job(fac.Reserve(Request(7, Jita, 3.0), T0));
            if (job.jobID == 0)
                break;
            ++made;
            T_CHECK(job.lineID != 0);
            lines.insert(job.lineID);
            T_CHECK_EQ(job.end - job.start, 3 * 3600);
            T_CHECK_EQ(job.cost, 100000 + 3 * 33300);
            T_CHECK_EQ(job.payeeID, 1000002);
        }
        T_CHECK_EQ(made, 15);
        T_CHECK_EQ(lines.size(), 15);
        FacilityBook::StationUse use;
        T_CHECK(fac.Use(Jita, LineActivity::Manufacturing, T0, use));
        T_CHECK_EQ(use.lines, 50);
        T_CHECK_EQ(use.free, 35);
        T_CHECK_EQ(use.simBusy, 15);
        T_CHECK_EQ(use.simOnLines, 15);
        T_CHECK_EQ(use.allowed, 0);
        // other activities and stations are separate;  a station without lines takes nothing
        T_CHECK_EQ(fac.Available(Jita, LineActivity::Invention, T0), 6);
        fac.SetStationLines(60004588, LineActivity::Manufacturing, 0);  // Rens
        T_CHECK_EQ(fac.Available(60004588, LineActivity::Manufacturing, T0), 0);
        T_CHECK_EQ(fac.Reserve(Request(7, 60004588, 1.0), T0).jobID, 0);
        fac.SetStationLines(60000001, LineActivity::Manufacturing, 3);
        T_CHECK_EQ(fac.Reserve(Request(7, 60000001, 1.0), T0).jobID, 0);
    }

    /* players busy on most lines:  the sim stops where MinFreeLinesPerStation lines are still free */
    {
        HarnessFacilities fac(p);
        fac.SetPlayerBusyShare(0.76);   // 38 of 50 lines
        uint32_t made(0);
        while (fac.Reserve(Request(7, Jita, 2.0), T0).jobID != 0)
            ++made;
        T_CHECK_EQ(made, 10);
        FacilityBook::StationUse use;
        fac.Use(Jita, LineActivity::Manufacturing, T0, use);
        T_CHECK_EQ(use.free, 2);
        T_CHECK_EQ(fac.Available(Jita, LineActivity::Manufacturing, T0 + 3600 + 1), 0);   // the players are busy again the next hour
        // the players' lines free up:  the sim may take more, up to its share
        fac.SetPlayerBusyShare(0.0);
        fac.InvalidateLines();
        T_CHECK_EQ(fac.Available(Jita, LineActivity::Manufacturing, T0 + 3600 + 1), 5);
    }

    /* a player picks a line the sim holds:  quoted as before the sim, the sim's job moves on */
    {
        HarnessFacilities fac(p);
        fac.SetStationLines(Jita, LineActivity::Manufacturing, 10);     // the sim may hold 3
        FacilityJob a(fac.Reserve(Request(7, Jita, 5.0), T0));
        FacilityJob b(fac.Reserve(Request(7, Jita, 5.0), T0));
        T_CHECK((a.jobID != 0) and (b.jobID != 0));
        T_CHECK_EQ(fac.HeldBy(a.lineID), a.jobID);
        // the source shows the sim's time (after a flush):  the player's quote is the old one, so the job starts at once
        T_CHECK_EQ(fac.PlayerView(a.lineID, a.lineSet), a.linePrev);
        T_CHECK(a.linePrev <= T0);
        T_CHECK_EQ(fac.PlayerView(a.lineID + 100, 12345), 12345);     // a line the sim doesn't hold
        uint32_t lineA(a.lineID);
        T_CHECK(fac.PlayerTakesLine(lineA, T0 + 7200, T0 + 7200, T0 + 60));
        const FacilityJob* moved(fac.FindJob(a.jobID));
        T_CHECK(moved != nullptr);
        T_CHECK((moved->lineID != 0) and (moved->lineID != lineA) and (moved->lineID != b.lineID));
        T_CHECK_EQ(moved->end, a.end);      // the job's time is not changed
        T_CHECK_EQ(fac.HeldBy(lineA), 0);
        T_CHECK_EQ(fac.PlayerView(lineA, T0 + 7200), T0 + 7200);
        FacilityBook::StationUse use;
        fac.Use(Jita, LineActivity::Manufacturing, T0 + 60, use);
        T_CHECK_EQ(use.simBusy, 2);
        T_CHECK_EQ(use.free, 10 - 1 - 2);
        /* players take every free line but two:  then a job the player takes from the sim has nowhere to go and runs on
         *  without a line (the sim never leaves less than MinFreeLinesPerStation free for players) */
        uint32_t taken(0);
        for (uint32_t id = 1; id < 3000; ++id) {
            if ((fac.HeldBy(id) != 0) or (id == lineA))
                continue;
            fac.Use(Jita, LineActivity::Manufacturing, T0 + 60, use);
            if (use.free <= 2)
                break;
            fac.PlayerTakesLine(id, T0 + 7200, T0 + 7200, T0 + 60);
            ++taken;
        }
        fac.Use(Jita, LineActivity::Manufacturing, T0 + 60, use);
        T_CHECK_EQ(use.free, 2);
        T_CHECK(taken > 0);
        T_CHECK(fac.PlayerTakesLine(b.lineID, T0 + 7200, T0 + 7200, T0 + 60));
        T_CHECK_EQ(fac.FindJob(b.jobID)->lineID, 0);
        fac.Use(Jita, LineActivity::Manufacturing, T0 + 60, use);
        T_CHECK_EQ(use.free, 2);
        T_CHECK_EQ(use.simBusy, 2);
        T_CHECK_EQ(use.simOnLines, 1);
        T_CHECK_EQ(fac.Available(Jita, LineActivity::Manufacturing, T0 + 60), 0);
        // a job without a line still counts until it ends
        T_CHECK(fac.Release(b.jobID, b.end));
        T_CHECK_EQ(fac.Jobs(), 1);
    }

    /* long runs are split;  a job that ran its time needs no write;  a cancelled one puts its line back now */
    {
        HarnessFacilities fac(p);
        FacilityJob big(fac.Reserve(Request(9, Jita, 100.0, 100), T0));
        T_CHECK_EQ(big.runs, 24);
        T_CHECK_EQ(big.qtyOut, 24);
        T_CHECK_EQ(big.end, T0 + 24 * 3600);
        FacilityJob small(fac.Reserve(Request(9, Jita, 1.0, 2), T0));
        T_CHECK_EQ(small.runs, 2);
        FacilityJob test(fac.Reserve(Request(0, Jita, 0.5), T0));
        T_CHECK(test.jobID != 0);
        T_CHECK_EQ(fac.EndTestJobs(T0 + 1799), 0);
        T_CHECK_EQ(fac.EndTestJobs(T0 + 1800), 1);
        T_CHECK_EQ(fac.FindJobFor(9, Jita, LineActivity::Manufacturing, T0, T0 + 3600), small.jobID);
        T_CHECK(fac.Release(small.jobID, T0 + 3600));
        T_CHECK_EQ(fac.Jobs(), 1);
        T_CHECK_EQ(fac.HeldBy(small.lineID), 0);
        std::map<uint32_t, FacilityBook::LineWrite> w(Writes(fac));
        T_CHECK(w.find(small.lineID) == w.end());
        T_CHECK(w.find(test.lineID) == w.end());
        T_CHECK(w.find(big.lineID) != w.end());
        T_CHECK(fac.Release(big.jobID, T0 + 7200));
        T_CHECK(!fac.Release(big.jobID, T0 + 7200));
        T_CHECK_EQ(fac.Jobs(), 0);
        FacilityBook::StationUse use;
        fac.Use(Jita, LineActivity::Manufacturing, T0 + 7200, use);
        T_CHECK_EQ(use.free, 50);
    }

    /* line writes:  the sim's time goes on with GREATEST (never moves a player's back), and is undone only where it is
     *  still the sim's */
    {
        HarnessFacilities fac(p);
        FacilityJob a(fac.Reserve(Request(7, Jita, 4.0), T0));
        FacilityJob b(fac.Reserve(Request(7, Jita, 4.0), T0));
        std::map<uint32_t, FacilityBook::LineWrite> w(Writes(fac));
        T_CHECK_EQ(w.size(), 2);
        T_CHECK(!w[a.lineID].hasGuard);
        T_CHECK_EQ(w[a.lineID].floor, a.lineSet);
        T_CHECK_EQ(Apply(0, w[a.lineID]), a.lineSet);
        T_CHECK_EQ(Apply(T0 + 99999, w[a.lineID]), T0 + 99999);    // a later time stays
        fac.PendingWritten();
        T_CHECK(!fac.HasPending());
        // a cancelled job's line goes back to its time before the sim, unless a player's time is there
        fac.Release(a.jobID, T0 + 600);
        w = Writes(fac);
        T_CHECK_EQ(w.size(), 1);
        T_CHECK(w[a.lineID].hasGuard);
        T_CHECK_EQ(Apply(a.lineSet, w[a.lineID]), a.linePrev);
        T_CHECK_EQ(Apply(T0 + 5000, w[a.lineID]), T0 + 5000);
        // the sim takes it again before the flush:  the source still has the old time, which the new one replaces
        FacilityJob c(fac.Reserve(Request(7, Jita, 1.0), T0 + 600));
        T_CHECK_EQ(c.lineID, a.lineID);
        T_CHECK_EQ(c.linePrev, a.linePrev);
        w = Writes(fac);
        T_CHECK(w[a.lineID].hasGuard);
        T_CHECK_EQ(Apply(a.lineSet, w[a.lineID]), c.lineSet);
        // before the flush the player is quoted the time before the sim, whatever the source holds
        T_CHECK_EQ(fac.PlayerView(a.lineID, a.lineSet), a.linePrev);
        fac.PendingWritten();
        // released and flushed:  a player is quoted the old time while the source still has the sim's
        fac.Release(c.jobID, T0 + 700);
        T_CHECK_EQ(fac.PlayerView(c.lineID, c.lineSet), c.linePrev);
        // a player takes b's line after the flush:  the undo keeps the player's time
        fac.PlayerTakesLine(b.lineID, T0 + 50000, T0 + 50000, T0 + 700);
        w = Writes(fac);
        T_CHECK(w.find(b.lineID) != w.end());
        T_CHECK_EQ(Apply(T0 + 50000, w[b.lineID]), T0 + 50000);
        const FacilityJob* bNow(fac.FindJob(b.jobID));
        T_CHECK((bNow != nullptr) and (bNow->lineID != 0) and (bNow->lineID != b.lineID));
    }

    /* a restart:  jobs loaded into a new book hold their lines (checked against the lines' state) */
    {
        HarnessFacilities fac(p);
        std::vector<FacilityJob> saved;
        for (int i = 0; i < 5; ++i)
            saved.push_back(fac.Reserve(Request(11 + i, Jita, 1.0 + i), T0));
        CheckedLines again(p);
        again.player.insert(saved[1].lineID);       // a player has it now
        again.gone.insert(saved[2].lineID);
        for (auto& j : saved) {
            FacilityJob loaded(j);
            loaded.linePrev = 0;
            loaded.lineSet = 0;
            again.LoadJob(loaded, false);
        }
        T_CHECK_EQ(again.Jobs(), 5);
        // before the check the jobs hold their lines (a player is quoted them free)
        T_CHECK(again.HeldBy(saved[1].lineID) != 0);
        T_CHECK_EQ(again.PlayerView(saved[0].lineID, T0 + 777), 0);
        T_CHECK_EQ(again.VerifyRestored(), 2);
        T_CHECK_EQ(again.VerifyRestored(), 0);
        T_CHECK_EQ(again.HeldBy(saved[1].lineID), 0);
        T_CHECK_EQ(again.HeldBy(saved[2].lineID), 0);
        T_CHECK(again.HeldBy(saved[0].lineID) != 0);
        T_CHECK(!again.HasPending());
        FacilityBook::StationUse use;
        again.Use(Jita, LineActivity::Manufacturing, T0 + 60, use);
        T_CHECK_EQ(use.simBusy, 5);
        T_CHECK_EQ(use.simOnLines, 3);
        T_CHECK_EQ(use.free, 47);
        T_CHECK_EQ(again.Available(Jita, LineActivity::Manufacturing, T0 + 60), 10);
        // after the check the source's time is the sim's:  a cancelled job undoes it
        uint32_t id0(again.FindJobFor(11, Jita, LineActivity::Manufacturing, saved[0].start, saved[0].end));
        T_CHECK(id0 != 0);
        again.Release(id0, T0 + 60);
        std::map<uint32_t, FacilityBook::LineWrite> w(Writes(again));
        T_CHECK((w.size() == 1) and w[saved[0].lineID].hasGuard);
        T_CHECK_EQ(Apply(T0 + 777, w[saved[0].lineID]), 0);
    }

    /* counters only (UseRealAssemblyLines false):  the same limits, no lines, no line writes */
    {
        FacilityParams v(p);
        v.realLines = false;
        HarnessFacilities fac(v);
        fac.SetPlayerBusyShare(0.9);    // not seen:  counters only
        uint32_t made(0);
        for (;;) {
            FacilityJob job(fac.Reserve(Request(7, Jita, 2.0), T0));
            if (job.jobID == 0)
                break;
            T_CHECK_EQ(job.lineID, 0);
            ++made;
        }
        T_CHECK_EQ(made, 15);
        T_CHECK(Writes(fac).empty());
    }

    /* refinery soft cap:  2e6 m3 an hour per station, an hour's worth at most */
    {
        HarnessFacilities fac(p);
        T_CHECK(fac.RefineAllowance(Jita, 3e6, T0) == 2e6);
        T_CHECK(fac.RefineAllowance(Jita, 1e6, T0) == 0.0);
        T_CHECK(fac.RefineAllowance(60008494, 1e6, T0) == 1e6);
        T_CHECK(fac.RefineAllowance(Jita, 3e6, T0 + 1800) == 1e6);
        T_CHECK(fac.RefineAllowance(Jita, 5e6, T0 + 1800 + 7200) == 2e6);
        FacilityParams nolimit(p);
        nolimit.refineryM3PerHour = 0.0;
        HarnessFacilities unlimited(nolimit);
        T_CHECK(unlimited.RefineAllowance(Jita, 9e9, T0) == 9e9);
    }

    /* the world's view (Prod::ICapacity):  the actor's slots and the station's share both limit;  jobs are found again
     *  by actor, station, start and end;  the line's cost is charged */
    {
        HarnessFacilities fac(p);
        LineCapacity cap(fac, 1000.0, 333.0);
        cap.SetNodeStations(1, std::vector<uint32_t>(1, Jita));
        cap.SetSlots(21, 10);
        cap.SetSlots(22, 10);
        T_CHECK_EQ(cap.Slots(21), 10);
        T_CHECK_EQ(cap.Free(21, 1, LineActivity::Manufacturing, T0), 10);
        T_CHECK_EQ(cap.Free(21, 2, LineActivity::Manufacturing, T0), 0);   // a node without a station
        T_CHECK_EQ(cap.Free(23, 1, LineActivity::Manufacturing, T0), 0);   // no slots
        std::vector<SimJob> jobs;
        for (int i = 0; i < 20; ++i) {
            SimJob job;
            job.actorID = (i < 10) ? 21 : 22;
            job.nodeID = 1;
            job.activity = LineActivity::Manufacturing;
            job.typeID = 587;
            job.runs = 2;
            job.qtyOut = 2;
            job.start = T0;
            job.end = T0 + 3600 * (1 + i % 3);
            if (!cap.Reserve(job, T0))
                continue;
            T_CHECK(job.lineID != 0);
            jobs.push_back(job);
        }
        T_CHECK_EQ(jobs.size(), 15);                // the station's share, not the actors' 20 slots
        T_CHECK_EQ(cap.Free(21, 1, LineActivity::Manufacturing, T0), 0);
        // the planner's probe (not booked) and a booked job cost the same here:  install + rent of started hours
        SimJob probe;
        probe.start = T0;
        probe.end = T0 + 5400;
        T_CHECK(cap.JobCost(probe) == 1000.0 + 2 * 333.0);
        T_CHECK(cap.JobCost(jobs[0]) == 1000.0 + 333.0);
        // a player moves a job off its line:  the world finds it, and learns its line for mktSimJob
        uint32_t oldLine(jobs[0].lineID);
        fac.PlayerTakesLine(oldLine, T0 + 9000, T0 + 9000, T0 + 10);
        uint32_t newLine(cap.CurrentLine(jobs[0]));
        T_CHECK(newLine != oldLine);
        // the world ends a job:  its slot and its share are free again
        cap.Free(21, 1, LineActivity::Manufacturing, jobs[0].end);    // the world's time moves on
        cap.Release(jobs[0]);
        T_CHECK_EQ(fac.Jobs(), 14);
        T_CHECK_EQ(cap.Free(21, 1, LineActivity::Manufacturing, jobs[0].end), 1);
        // the world drops a job it couldn't pay for at once:  nothing of it reaches the lines
        SimJob extra(jobs[0]);
        extra.start = jobs[0].end;
        extra.end = extra.start + 3600;
        T_CHECK(cap.Reserve(extra, extra.start));
        cap.Release(extra);
        T_CHECK_EQ(fac.Jobs(), 14);
        // a restart:  the world's jobs come back on their lines
        HarnessFacilities fac2(p);
        LineCapacity cap2(fac2, 1000.0, 333.0);
        cap2.SetNodeStations(1, std::vector<uint32_t>(1, Jita));
        cap2.SetSlots(21, 10);
        cap2.SetSlots(22, 10);
        for (size_t i = 1; i < jobs.size(); ++i)
            cap2.Restore(jobs[i]);
        T_CHECK_EQ(fac2.Jobs(), 14);
        T_CHECK_EQ(cap2.Free(21, 1, LineActivity::Manufacturing, T0 + 60), 1);
        T_CHECK_EQ(cap2.CurrentLine(jobs[3]), jobs[3].lineID);
        cap2.Release(jobs[3]);
        T_CHECK_EQ(fac2.Jobs(), 13);
    }

    /* a node works at several stations:  the room adds up, a job goes where there is most */
    {
        HarnessFacilities fac(p);
        const uint32_t hub(60004588), other(60004594);
        fac.SetStationLines(hub, LineActivity::Manufacturing, 0);      // Rens VI-8 has no manufacturing lines
        fac.SetStationLines(other, LineActivity::Manufacturing, 10);
        LineCapacity cap(fac, 1000.0, 333.0);
        std::vector<uint32_t> stations;
        stations.push_back(hub);
        stations.push_back(other);
        stations.push_back(Jita);
        cap.SetNodeStations(4, stations);
        cap.SetSlots(31, 100);
        T_CHECK_EQ(cap.Free(31, 4, LineActivity::Manufacturing, T0), 3 + 15);
        SimJob job;
        job.actorID = 31;
        job.nodeID = 4;
        job.stationID = hub;        // the world says the node's station
        job.typeID = 587;
        job.runs = 1;
        job.qtyOut = 1;
        job.start = T0;
        job.end = T0 + 3600;
        T_CHECK(cap.Reserve(job, T0));
        T_CHECK_EQ(job.stationID, Jita);
        uint32_t made(1);
        for (;;) {
            SimJob next(job);
            next.stationID = hub;
            next.lineID = 0;
            next.end = T0 + 3600 + made;
            if (!cap.Reserve(next, T0))
                break;
            ++made;
        }
        T_CHECK_EQ(made, 18);
        T_CHECK_EQ(cap.Free(31, 4, LineActivity::Manufacturing, T0), 0);
        // a production site that isn't one of the node's is kept
        SimJob site(job);
        site.stationID = 60008494;
        site.end = T0 + 7200;
        T_CHECK(cap.Reserve(site, T0));
        T_CHECK_EQ(site.stationID, 60008494);
        cap.Release(job);
        T_CHECK_EQ(cap.Free(31, 4, LineActivity::Manufacturing, T0), 1);
    }

    // the nodes' stations (SimLineStations, PLAN-amarr-production-stations 7)
    NodeStationsTest();

    return MktTest::failures;
}

/* the golden test (PLAN-amarr-production-stations 7):  on the real export (MKTSIM_EXPORT=<export.json>, written by
 *  gen_market_calibration.py --export-json with lineStations), the five hubs' stations by the floors alone
 *  (NodeScaleWithNeed false) are pinned.  skipped without it.  a client-sync migration that changes the stations fails
 *  it on purpose */
int NodeStationsGoldenTest()
{
    const char* path(std::getenv("MKTSIM_EXPORT"));
    if ((path == nullptr) or (*path == 0)) {
        std::printf("  skipped:  MKTSIM_EXPORT is not set\n");
        return 0;
    }
    StaticData data;
    std::string err;
    if (!MktHarness::LoadExport(path, data, err)) {
        std::printf("  skipped:  %s\n", err.c_str());
        return 0;
    }
    if (data.lineStations.empty()) {
        std::printf("  skipped:  the export has no lineStations (an older generator)\n");
        return 0;
    }
    NodeStationParams sp;
    sp.scaleWithNeed = false;
    struct Expect { uint32_t hub; size_t count; std::vector<uint32_t> added; };
    const Expect expect[] = {
        { 60003760, 17, Ids({60002419, 60002065}) },                                  // Jita:  Sobaseki, Malkalen
        { 60008494, 6,  Ids({60008650, 60014632, 60002209, 60002212, 60002173}) },    // Amarr:  Sarum Prime, Kor-Azor Prime, Penirgman
        { 60011866, 9,  Ids({60010927, 60001225}) },                                  // Dodixie:  Ainaille, Nausschie
        { 60004588, 8,  Ids({60004744, 60013234}) },                                  // Rens:  Pator, Oremmulf
        { 60005686, 7,  Ids({60014806, 60004744, 60013225}) },                        // Hek:  Nakugard, Pator, Vullat
    };
    std::map<uint32_t, std::array<double, LineKind::Count>> claimed;
    double zero[2] = {0.0, 0.0};
    for (auto& e : expect) {
        const NodeData* node(nullptr);
        for (auto& n : data.nodes)
            if (n.stationID == e.hub)
                node = &n;
        T_CHECK(node != nullptr);
        if (node == nullptr)
            continue;
        std::vector<RankedStation> ranked(LineStations::Rank(*node, data.lineStations, data.systems, data.jumps, sp));
        NodeChoice c(LineStations::Choose(ranked, zero, sp, claimed));
        std::vector<uint32_t> added;
        for (auto i : c.chosen)
            if (!ranked[i].base)
                added.push_back(ranked[i].stationID);
        std::printf("  hub %u:  %zu stations (mfg %u, inv %u), %zu added\n", e.hub, c.count, c.stations[0], c.stations[1], added.size());
        T_CHECK_EQ(c.count, e.count);
        T_CHECK(added == e.added);
        T_CHECK((c.stations[LineKind::Mfg] >= 6) and (c.stations[LineKind::Inv] >= 2));
    }
    return MktTest::failures;
}
