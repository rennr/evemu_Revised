 /**
  * @name HarnessFacilities.h
  *   the harness' stand-in for the server's station lines (SimFacilityMgr):  the same FacilityBook (policy, jobs, player
  *   guarantees) on synthetic lines in memory, so the offline harness runs with the capacity limits of PLAN 6.6
  *
  * @date:          15Oct26
  *
  */

#ifndef _EVE_MARKET_SIM_HARNESS_FACILITIES_H__
#define _EVE_MARKET_SIM_HARNESS_FACILITIES_H__

#include <map>
#include <utility>
#include <vector>

#include "market/sim/SimFacilities.h"
#include "market/sim/SimLineCapacity.h"
#include "market/sim/SimWorld.h"

class HarnessFacilities : public MktSim::FacilityBook {
public:
    /* stations get the lines of a typical Crucible npc station by default:  50 manufacturing, and at research stations
     *  20 PE, 20 ME, 10 copying and 20 invention lines (ramAssemblyLines) */
    explicit HarnessFacilities(const MktSim::FacilityParams& params);

    // lines of an activity at a station (0:  none, as at Rens).  stations not set get the default of the activity
    void SetStationLines(uint32_t stationID, uint8_t activity, uint32_t lines);
    void SetDefaultLines(uint8_t activity, uint32_t lines);
    // what a line costs and who is paid (every line)
    void SetLineCost(double costInstall, double costPerHour, uint32_t ownerID);
    // this share of every station's lines is busy with players' jobs (renewed each hour), to see the sim keep its limits
    void SetPlayerBusyShare(double share)               { m_playerBusy = share; }

protected:
    bool LoadLines(uint32_t stationID, uint8_t activity, int64_t now, std::vector<MktSim::FacilityLine>& into) override;

private:
    typedef std::pair<uint32_t, uint8_t> Key;
    std::map<Key, uint32_t> m_lines;
    std::map<uint8_t, uint32_t> m_default;
    std::map<Key, uint32_t> m_firstLineID;
    uint32_t m_nextLineID;
    double   m_costInstall;
    double   m_costPerHour;
    uint32_t m_ownerID;
    double   m_playerBusy;
};

/* phase 7 (scenario keys MaxLineShare, MinFreeLinesPerStation):  every HarnessCapacity made afterwards uses these instead
 *  of FacilityParams' defaults.  <= 0 / < 0 keeps a default */
void SetHarnessLineShare(double maxLineShare, int minFreeLines);

/* phase 5 (scenario keys NodeMinLineStations ... RefineAtNodeStations, the flags --node-stations and --node-max-jumps):
 *  every HarnessCapacity made afterwards uses these (MaxLineShare and MinFreeLinesPerStation come from
 *  SetHarnessLineShare).  freight and refining at the node's stations:  RemoteLineFreight, RefineAtNodeStations */
MktSim::NodeStationParams& HarnessNodeStations();
void SetHarnessNodeExtras(int remoteLineFreight, int refineAtNodeStations);
// --line-need / --actors:  Finalize's lines are printed
void SetHarnessLineLog(bool on);

/* the world's capacity in the harness (World::SetCapacity before Build), as the server books it.  with the export's
 *  lineStations:  every node's stations by SimLineStations (its constellation's, topped up, sized from the world's line
 *  need), each station with its real manufacturing and invention lines (no synthetic ones).  without them (an older
 *  export, the synthetic world):  the hubs' constellation stations of Crucible as fixed lists (50 manufacturing lines
 *  each, none at Rens VI-8, the synthetic research lines everywhere), a node at another station there only */
class HarnessCapacity {
public:
    HarnessCapacity(const MktSim::StaticData& data, const MktSim::Params& p);

    MktSim::LineCapacity* Capacity()                    { return &m_capacity; }
    HarnessFacilities& Lines()                          { return m_lines; }

private:
    HarnessFacilities    m_lines;
    MktSim::LineCapacity m_capacity;
};

#endif  // _EVE_MARKET_SIM_HARNESS_FACILITIES_H__
