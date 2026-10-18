 /**
  * @name SimLineCapacity.h
  *   market simulation:  the world's capacity (Prod::ICapacity, phase 3) on station lines (FacilityBook, PLAN 6.6).
  *   it replaces VirtualCapacity with World::SetCapacity before Build():  a manufacturer may start a job while it has a
  *   slot of its own left and one of its node's stations takes one more (MaxLineShare, MinFreeLinesPerStation);  the
  *   job holds a real line (or a station counter in mode UseRealAssemblyLines false) and pays the line's install and
  *   rent.  a node's stations are its industrial site until phase 4 has production sites:  its constellation's, topped
  *   up (SimLineStations.h);  the job runs at the one with the most room (job.stationID), its output still comes to the
  *   node (a job away from the node's station pays freight, RemoteLineFreight).  what the stations can't cover of the
  *   node's line need is served by counted lines of the node's own (VirtualLinesForShortfall:  job.virtualLine)
  *   the world keeps the jobs (mktSimJob) and hands their output over;  the book finds a job again by its actor,
  *   station, activity, start and end (the world gives it its jobID after Reserve, and a player may move it to
  *   another line meanwhile:  CurrentLine)
  *
  * @date:          15Oct26
  *
  */

#ifndef _EVE_SERVER_MARKET_SIM_LINE_CAPACITY_H__
#define _EVE_SERVER_MARKET_SIM_LINE_CAPACITY_H__

#include <array>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "market/sim/SimProduction.h"
#include "market/sim/SimFacilities.h"
#include "market/sim/SimLineStations.h"

namespace MktSim {

    class LineCapacity : public Prod::ICapacity {
    public:
        /* book:  the lines (the server's sSimFacilities, the harness' HarnessFacilities).  a job not booked yet (the
         *  planner's probe) is costed at probeInstall + probePerHour per started hour */
        LineCapacity(FacilityBook& book, double probeInstall, double probePerHour);

        /* where a node's jobs may run, fixed (the node's own station first;  the harness' older exports and the tests):  no
         *  need term, no shortfall, no freight */
        void SetNodeStations(uint32_t nodeID, const std::vector<uint32_t>& stationIDs);
        /* the node's ranked list (SimLineStations::Rank).  until Finalize the node works at the floors' prefix (need 0);
         *  Finalize sizes it from the line need the world gave (SetLineNeed) */
        void SetNodeRanking(uint32_t nodeID, const std::vector<RankedStation>& ranked);
        void SetStationParams(const NodeStationParams& p)  { m_sp = p; }
        const NodeStationParams& StationParams() const      { return m_sp; }
        // RemoteLineFreight:  a job away from its node's station pays m3 x jumps x isk per m3 and jump
        void SetFreight(bool on, double iskPerM3Jump)        { m_freight = on; m_freightPerM3Jump = iskPerM3Jump; }
        // RefineAtNodeStations:  a node's refining is metered over the refineries of its stations, its own first
        void SetRefineAtNodeStations(bool on)               { m_refineAtNode = on; }
        // where Finalize's lines go (the server's log, the harness' stdout)
        void SetLog(std::function<void(const std::string&)> log)  { m_log = log; }

        // the jumps from the node's station to one of its stations (0:  the node's own, or not known)
        uint16_t Jumps(uint32_t nodeID, uint32_t stationID) const;

        // per node and activity:  what it books (the GM's /simfacility node, the harness' --line-need and --actors)
        struct KindStats {
            uint32_t held           = 0;        // jobs on the node's lines now (stations and counted)
            uint32_t peak           = 0;
            uint32_t virtualHeld    = 0;        // of them, on the counted lines
            uint32_t virtualPeak    = 0;
            double   lineSeconds    = 0.0;      // the integral of held over time
            int64_t  since          = 0;        // the first booking's time
            int64_t  last           = 0;
            uint64_t freeShort      = 0;        // Free calls with slots left and no line
            uint64_t reserveShort   = 0;        // Reserve calls refused for want of a line
            double Mean() const                 { return (last > since) ? lineSeconds / (double)(last - since) : 0.0; }
        };
        struct NodeLines {
            bool     ranked         = false;    // false:  SetNodeStations' fixed list
            std::vector<RankedStation> list;    // ranked:  the whole ranked list
            NodeChoice choice;                  // what it works at (ranked)
            std::vector<uint32_t> stations;     // the stations it works at, its own first
            std::map<uint32_t, uint16_t> jumps;
            std::vector<uint32_t> refineries;   // of them, those with a refinery other than its own
            double   need[LineKind::Count] = {0.0, 0.0};
            uint32_t virtualLines[LineKind::Count] = {0, 0};
            uint32_t virtualUsed[LineKind::Count] = {0, 0};
            KindStats stats[LineKind::Count];
        };
        const NodeLines* Node(uint32_t nodeID) const;
        std::vector<uint32_t> NodeIDs() const;
        // the node's stations:  jumps, lines, sim cap, base or added, room now;  its need, shortfall and counters
        std::string NodeDetail(uint32_t nodeID, int64_t now);

        /* Prod::ICapacity */
        void     SetSlots(uint32_t actorID, uint32_t slots) override  { m_slots[actorID] = slots; }
        uint32_t Slots(uint32_t actorID) const override;
        // the actor's slots left, and what the node's stations take now
        uint32_t Free(uint32_t actorID, uint32_t nodeID, uint8_t activity, int64_t now) const override;
        /* books a line at the node's station with the most room (sets job.stationID and job.lineID;  job.end stretches with
         *  the line's time multiplier).  a job.stationID that isn't one of the node's (a production site) is kept.  false:
         *  no capacity */
        bool     Reserve(SimJob& job, int64_t now) override;
        // a job loaded after a restart holds its line again (checked with the book's VerifyRestored)
        void     Restore(const SimJob& job) override;
        // the job ended or was dropped:  its line is free
        void     Release(const SimJob& job) override;
        // isk:  the line's install and rent for the job's started hours (Isk::LineCost, to the station owner)
        double   JobCost(const SimJob& job) const override;
        // the book's refinery cap (FacilityParams::refineryM3PerHour per station)
        double   RefineAllowance(uint32_t stationID, double m3, int64_t now) override;
        // RefineAtNodeStations:  the node's station first, then its other stations with a refinery
        double   RefineAllowanceAtNode(uint32_t nodeID, uint32_t stationID, double m3, int64_t now) override;
        // RemoteLineFreight
        double   Freight(const SimJob& job, double m3) const override;
        // the world's calibrated line need (average busy lines), summed per node and activity
        void     SetLineNeed(uint32_t nodeID, uint8_t activity, double avgLines) override;
        // after Build:  each ranked node's stations from its need (nodeID order, for the shared stations' claims)
        void     Finalize() override;

        // the line the job holds now (a player may have moved it), for mktSimJob.assemblyLineID.  0:  none
        uint32_t CurrentLine(const SimJob& job) const;

    private:
        const std::vector<uint32_t>* StationsOf(uint32_t nodeID) const;
        uint32_t BookJob(const SimJob& job) const;
        void     Apply(NodeLines& n, const NodeChoice& c);
        void     Track(uint32_t nodeID, uint8_t activity, int delta, bool onVirtual, int64_t now) const;

        FacilityBook& m_book;
        double m_probeInstall;
        double m_probePerHour;
        std::map<uint32_t, uint32_t> m_slots;
        std::map<uint32_t, uint32_t> m_used;
        mutable std::map<uint32_t, NodeLines> m_nodes;     // (mutable:  Free counts its short calls)
        NodeStationParams m_sp;
        bool     m_freight;
        double   m_freightPerM3Jump;
        bool     m_refineAtNode;
        std::function<void(const std::string&)> m_log;
        mutable int64_t m_now;      // the latest time the world gave (Release has none)
    };

}

#endif  // _EVE_SERVER_MARKET_SIM_LINE_CAPACITY_H__
