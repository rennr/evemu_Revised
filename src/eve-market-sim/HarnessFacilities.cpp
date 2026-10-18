 /**
  * @name HarnessFacilities.cpp
  *   the harness' station lines (see HarnessFacilities.h)
  *
  * @date:          15Oct26
  *
  */

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "HarnessFacilities.h"

using namespace MktSim;

HarnessFacilities::HarnessFacilities(const FacilityParams& params)
: m_nextLineID(1),
  m_costInstall(1000.0),
  m_costPerHour(333.0),
  m_ownerID(1000002),
  m_playerBusy(0.0)
{
    SetParams(params);
    m_default[LineActivity::Manufacturing] = 50;
    m_default[LineActivity::ResearchTime] = 20;
    m_default[LineActivity::ResearchMaterial] = 20;
    m_default[LineActivity::Copying] = 10;
    m_default[LineActivity::Invention] = 20;
}

void HarnessFacilities::SetStationLines(uint32_t stationID, uint8_t activity, uint32_t lines)
{
    m_lines[Key(stationID, activity)] = lines;
    InvalidateLines(stationID);
}

void HarnessFacilities::SetDefaultLines(uint8_t activity, uint32_t lines)
{
    m_default[activity] = lines;
    InvalidateLines();
}

void HarnessFacilities::SetLineCost(double costInstall, double costPerHour, uint32_t ownerID)
{
    m_costInstall = costInstall;
    m_costPerHour = costPerHour;
    m_ownerID = ownerID;
    InvalidateLines();
}

bool HarnessFacilities::LoadLines(uint32_t stationID, uint8_t activity, int64_t now, std::vector<FacilityLine>& into)
{
    Key key(stationID, activity);
    std::map<Key, uint32_t>::const_iterator itr = m_lines.find(key);
    uint32_t count(0);
    if (itr != m_lines.end()) {
        count = itr->second;
    } else {
        std::map<uint8_t, uint32_t>::const_iterator dItr = m_default.find(activity);
        count = (dItr == m_default.end()) ? 0 : dItr->second;
    }
    if (count == 0)
        return false;

    // a station's lines keep their numbers (room for 1000 per station and activity)
    std::map<Key, uint32_t>::iterator fItr = m_firstLineID.find(key);
    if (fItr == m_firstLineID.end()) {
        fItr = m_firstLineID.insert(std::make_pair(key, m_nextLineID)).first;
        m_nextLineID += 1000;
    }
    // the players' lines are the first ones, busy until the next full hour
    uint32_t busy((uint32_t)std::lround(std::min(1.0, std::max(0.0, m_playerBusy)) * count));
    int64_t hourEnd((now - (now % 3600)) + 3600);
    for (uint32_t k = 0; (k < count) and (k < 1000); ++k) {
        FacilityLine line;
        line.lineID = fItr->second + k;
        line.ownerID = m_ownerID;
        line.freeAt = (k < busy) ? hourEnd : 0;
        line.raw = line.freeAt;
        line.timeMultiplier = 1.0;
        line.costInstall = m_costInstall;
        line.costPerHour = m_costPerHour;
        into.push_back(line);
    }
    return true;
}

namespace {
    /* Crucible:  the npc stations with manufacturing lines in the constellation of each hub, as
     *  FactoryDB::StationsWithLinesNear lists them (the hub, its solar system's, the rest);  50 lines each */
    struct HubStations { uint32_t hub; std::vector<uint32_t> stations; };
    const std::vector<HubStations>& CrucibleHubStations()
    {
        static const std::vector<HubStations> hubs = {
            { 60003760, { 60003760, 60002953, 60002959, 60003055, 60003757, 60004423, 60002950, 60002956, 60003052, 60003058,
                          60003754, 60003763, 60003769, 60014659, 60014935 } },                 // Jita:  15 stations
            { 60008494, { 60008494 } },                                                         // Amarr:  1
            { 60011866, { 60011866, 60011863, 60011869, 60011872, 60011977, 60011983, 60015013 } },  // Dodixie:  7
            { 60004588, { 60004588, 60005725, 60005728, 60005731, 60014779, 60015041 } },       // Rens:  5 (none at the hub)
            { 60005686, { 60005686, 60005683, 60005689, 60014770 } },                           // Hek:  4
        };
        return hubs;
    }

    // phase 7:  scenario overrides (SetHarnessLineShare);  <= 0 / < 0:  the default
    double   s_maxLineShare = 0.0;
    int      s_minFreeLines = -1;
    // the node stations' keys (HarnessNodeStations);  -1:  the default
    NodeStationParams s_nodeStations;
    int      s_remoteFreight = -1;
    int      s_refineAtNode = -1;
    bool     s_lineLog = false;

    FacilityParams HarnessFacilityParams(const Params& p)
    {
        FacilityParams fp;          // the server's defaults (<marketSim> MaxLineShare, MinFreeLinesPerStation, ...)
        fp.maxJobHours = p.maxJobHours;
        if (s_maxLineShare > 0.0)
            fp.maxLineShare = s_maxLineShare;
        if (s_minFreeLines >= 0)
            fp.minFreeLines = (uint32_t)s_minFreeLines;
        return fp;
    }
}

NodeStationParams& HarnessNodeStations()
{
    return s_nodeStations;
}

void SetHarnessNodeExtras(int remoteLineFreight, int refineAtNodeStations)
{
    if (remoteLineFreight >= 0)
        s_remoteFreight = remoteLineFreight;
    if (refineAtNodeStations >= 0)
        s_refineAtNode = refineAtNodeStations;
}

void SetHarnessLineLog(bool on)
{
    s_lineLog = on;
}

void SetHarnessLineShare(double maxLineShare, int minFreeLines)
{
    if (maxLineShare > 0.0)
        s_maxLineShare = maxLineShare;
    if (minFreeLines >= 0)
        s_minFreeLines = minFreeLines;
}

HarnessCapacity::HarnessCapacity(const StaticData& data, const Params& p)
: m_lines(HarnessFacilityParams(p)),
  m_capacity(m_lines, p.lineInstallCost, p.lineCostPerHour)
{
    m_lines.SetLineCost(p.lineInstallCost, p.lineCostPerHour, 1000002);
    if (data.lineStations.empty()) {
        // an older export or the synthetic world:  Crucible's hub constellations, fixed
        m_lines.SetStationLines(60004588, LineActivity::Manufacturing, 0);
        for (auto& n : data.nodes) {
            std::vector<uint32_t> stations(1, n.stationID);
            for (auto& h : CrucibleHubStations())
                if (h.hub == n.stationID)
                    stations = h.stations;
            m_capacity.SetNodeStations(n.nodeID, stations);
        }
        return;
    }
    // the real stations:  their own lines only
    for (auto& d : { LineActivity::Manufacturing, LineActivity::ResearchTime, LineActivity::ResearchMaterial,
                     LineActivity::Copying, LineActivity::Invention })
        m_lines.SetDefaultLines(d, 0);
    for (auto& st : data.lineStations) {
        m_lines.SetStationLines(st.stationID, LineActivity::Manufacturing, st.mfgLines);
        m_lines.SetStationLines(st.stationID, LineActivity::Invention, st.invLines);
    }
    NodeStationParams sp(s_nodeStations);
    FacilityParams fp(m_lines.Params());
    sp.maxLineShare = fp.maxLineShare;
    sp.minFreeLines = fp.minFreeLines;
    m_capacity.SetStationParams(sp);
    m_capacity.SetFreight(s_remoteFreight != 0, p.freightPerM3Jump);
    m_capacity.SetRefineAtNodeStations(s_refineAtNode != 0);
    if (s_lineLog)
        m_capacity.SetLog([](const std::string& line) { std::printf("  %s\n", line.c_str()); });
    for (auto& n : data.nodes)
        m_capacity.SetNodeRanking(n.nodeID, LineStations::Rank(n, data.lineStations, data.systems, data.jumps, sp));
}
