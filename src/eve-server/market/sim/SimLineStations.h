 /**
  * @name SimLineStations.h
  *   market simulation:  the npc stations a node's industry works at (phase 5;  doc/code_and_design_notes/planning/
  *   market-sim/PLAN-amarr-production-stations.md).  a node keeps its constellation's stations with lines;  when they are
  *   fewer than a typical high-sec constellation has (NodeMinLineStations manufacturing, NodeMinResearchStations
  *   invention), or their sim lines don't cover the node's own calibrated line need x NodeLineHeadroom, the nearest
  *   stations with lines are added, by jumps (high-sec nodes through high-sec only), then the node's region, then
  *   stationID, at most NodeStationMaxJumps away and NodeMaxStations in all.  what the stations still can't cover is
  *   the shortfall:  the sim's own counted lines serve it (VirtualLinesForShortfall;  the owner's decision 2026-10-17,
  *   so EconomyScale 1.0 works).  the station policy (MaxLineShare, MinFreeLinesPerStation, players always win) is
  *   unchanged.  pure functions, no server dependencies (the server and the harness run the same code)
  *
  * @date:          17Oct26
  *
  */

#ifndef _EVE_SERVER_MARKET_SIM_LINE_STATIONS_H__
#define _EVE_SERVER_MARKET_SIM_LINE_STATIONS_H__

#include <array>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "market/sim/SimStaticData.h"

namespace MktSim {

    // the activities the sim books on station lines, as indices
    namespace LineKind {
        enum : uint8_t { Mfg = 0, Inv = 1, Count = 2 };
        // Activity::Manufacturing -> Mfg, Activity::Invention -> Inv, anything else Count
        uint8_t Of(uint8_t activity);
        const char* Name(uint8_t kind);
    }

    // <marketSim> NodeMinLineStations ... VirtualLinesForShortfall, MaxLineShare, MinFreeLinesPerStation
    struct NodeStationParams {
        uint32_t minStations[LineKind::Count] = {6, 2};    // NodeMinLineStations, NodeMinResearchStations (0:  constellation only)
        uint32_t maxJumps           = 5;        // NodeStationMaxJumps:  the farthest an added station may be
        uint32_t maxStations        = 60;       // NodeMaxStations:  a node's list grows to at most this (the base is kept)
        bool     scaleWithNeed      = true;     // NodeScaleWithNeed:  the node's own line need can raise its targets
        double   headroom           = 2.0;      // NodeLineHeadroom:  sim lines wanted = average need x this
        bool     sameBand           = true;     // NodeStationSameBand:  a high-sec node adds high-sec stations, through high-sec
        bool     virtualFill        = true;     // VirtualLinesForShortfall:  the sim's own counted lines serve the shortfall
        double   maxLineShare       = 0.3;      // the book's policy (FacilityParams), for a station's sim cap
        uint32_t minFreeLines       = 2;
    };

    // a station of a node's ranked list:  its base (constellation) stations first, then the candidates in order
    struct RankedStation {
        uint32_t stationID      = 0;
        uint16_t jumps          = 0;            // from the node's system (base:  any route;  added:  the band's route)
        bool     base           = false;
        bool     refinery       = false;
        uint16_t lines[LineKind::Count] = {0, 0};
        uint16_t simCap[LineKind::Count] = {0, 0};
    };

    // what a node works with, per activity
    struct NodeChoice {
        size_t   count          = 0;            // stations the node works at (the base included)
        std::vector<uint32_t> chosen;           // their indices in the ranked list, in its order
        uint32_t added          = 0;            // of them, beyond the base
        uint16_t farthest       = 0;            // jumps
        uint32_t stations[LineKind::Count] = {0, 0};       // with lines of the activity
        double   need[LineKind::Count] = {0.0, 0.0};       // average busy lines (the world's calibration)
        double   wanted[LineKind::Count] = {0.0, 0.0};     // need x headroom (0 without NodeScaleWithNeed)
        double   have[LineKind::Count] = {0.0, 0.0};       // sim lines the node counts (what earlier nodes claimed left out)
        double   shortfall[LineKind::Count] = {0.0, 0.0};  // wanted - have, >= 0
        uint32_t virtualLines[LineKind::Count] = {0, 0};   // counted lines serving the shortfall (VirtualLinesForShortfall)
    };

    namespace LineStations {
        // the sim's share of a station's lines of an activity:  min(floor(share x lines), lines - minFree), >= 0
        uint32_t SimCap(uint32_t lines, double maxShare, uint32_t minFree);

        /* a node's ranked list.  the base:  the node's own station (whatever its lines), then its constellation's stations
         *  with lines (its solar system's first, then by stationID);  fixed (NodeStations) replaces the constellation's.
         *  then every other station with lines at most maxJumps away (a high-sec node with sameBand:  high-sec stations
         *  reached through high-sec only), by jumps, the node's region first, then stationID.  systems without a
         *  constellation (an older export):  the base is the node's station alone */
        std::vector<RankedStation> Rank(const NodeData& node, const std::vector<LineStation>& stations,
                                        const std::map<uint32_t, MapSystem>& systems,
                                        const std::vector<std::pair<uint32_t, uint32_t>>& jumps,
                                        const NodeStationParams& p, const std::vector<uint32_t>* fixed = nullptr);

        /* the stations of ranked a node works at:  the base, then per activity (invention first, the scarcer) the next
         *  candidates in ranked order with lines of it while it has fewer stations with its lines than minStations or fewer
         *  sim lines than need x headroom, at most maxStations in all;  a station taken counts for both activities.  claimed (stationID -> sim lines per activity) holds what earlier nodes counted:
         *  a node counts only what is left, and claims what it wants of it.  need:  average busy lines per activity */
        NodeChoice Choose(const std::vector<RankedStation>& ranked, const double need[LineKind::Count],
                          const NodeStationParams& p, std::map<uint32_t, std::array<double, LineKind::Count>>& claimed);

        // NodeStations:  'nodeStationID:stationID,stationID,...;...' -> node station -> its stations (malformed parts skipped)
        std::map<uint32_t, std::vector<uint32_t>> ParseOverride(const std::string& text);

        // 'S stations (mfg M, inv I;  farthest J jumps;  +K beyond its constellation)' and the shortfall, one line
        std::string Summary(const NodeChoice& c);
    }

}

#endif  // _EVE_SERVER_MARKET_SIM_LINE_STATIONS_H__
