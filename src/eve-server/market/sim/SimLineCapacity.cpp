 /**
  * @name SimLineCapacity.cpp
  *   market simulation:  the world's capacity on station lines (see SimLineCapacity.h)
  *
  * @date:          15Oct26
  *
  */

#include <algorithm>
#include <cstdio>

#include "market/sim/SimLineCapacity.h"

namespace MktSim {

LineCapacity::LineCapacity(FacilityBook& book, double probeInstall, double probePerHour)
: m_book(book),
  m_probeInstall(probeInstall),
  m_probePerHour(probePerHour),
  m_freight(false),
  m_freightPerM3Jump(0.0),
  m_refineAtNode(false),
  m_now(0)
{
}

void LineCapacity::SetNodeStations(uint32_t nodeID, const std::vector<uint32_t>& stationIDs)
{
    NodeLines& n(m_nodes[nodeID]);
    n = NodeLines();
    n.stations = stationIDs;
}

void LineCapacity::SetNodeRanking(uint32_t nodeID, const std::vector<RankedStation>& ranked)
{
    NodeLines& n(m_nodes[nodeID]);
    n = NodeLines();
    n.ranked = true;
    n.list = ranked;
    // the floors' choice until Finalize (the need is 0, nothing claimed)
    std::map<uint32_t, std::array<double, LineKind::Count>> none;
    double zero[LineKind::Count] = {0.0, 0.0};
    Apply(n, LineStations::Choose(n.list, zero, m_sp, none));
}

void LineCapacity::Apply(NodeLines& n, const NodeChoice& c)
{
    n.choice = c;
    n.stations.clear();
    n.jumps.clear();
    n.refineries.clear();
    for (auto i : c.chosen) {
        const RankedStation& r(n.list[i]);
        n.stations.push_back(r.stationID);
        n.jumps[r.stationID] = r.jumps;
        if (r.refinery and (n.stations.size() > 1))
            n.refineries.push_back(r.stationID);
    }
    for (uint8_t k = 0; k < LineKind::Count; ++k)
        n.virtualLines[k] = c.virtualLines[k];
}

void LineCapacity::SetLineNeed(uint32_t nodeID, uint8_t activity, double avgLines)
{
    uint8_t k(LineKind::Of(activity));
    std::map<uint32_t, NodeLines>::iterator itr = m_nodes.find(nodeID);
    if ((k >= LineKind::Count) or (itr == m_nodes.end()) or !(avgLines > 0.0))
        return;
    itr->second.need[k] += avgLines;
}

void LineCapacity::Finalize()
{
    // nodeID order:  a station an earlier node counted is counted by the next only for what is left
    std::map<uint32_t, std::array<double, LineKind::Count>> claimed;
    for (auto& cur : m_nodes) {
        NodeLines& n(cur.second);
        if (!n.ranked)
            continue;
        Apply(n, LineStations::Choose(n.list, n.need, m_sp, claimed));
        if (m_log)
            m_log("node " + std::to_string(cur.first) + " (station " + std::to_string(n.list.empty() ? 0 : n.list[0].stationID)
                  + ") works at " + LineStations::Summary(n.choice));
    }
}

const LineCapacity::NodeLines* LineCapacity::Node(uint32_t nodeID) const
{
    std::map<uint32_t, NodeLines>::const_iterator itr = m_nodes.find(nodeID);
    return (itr == m_nodes.end()) ? nullptr : &itr->second;
}

std::vector<uint32_t> LineCapacity::NodeIDs() const
{
    std::vector<uint32_t> out;
    for (auto& cur : m_nodes)
        out.push_back(cur.first);
    return out;
}

uint16_t LineCapacity::Jumps(uint32_t nodeID, uint32_t stationID) const
{
    const NodeLines* n(Node(nodeID));
    if (n == nullptr)
        return 0;
    std::map<uint32_t, uint16_t>::const_iterator itr = n->jumps.find(stationID);
    return (itr == n->jumps.end()) ? 0 : itr->second;
}

uint32_t LineCapacity::Slots(uint32_t actorID) const
{
    std::map<uint32_t, uint32_t>::const_iterator itr = m_slots.find(actorID);
    return (itr == m_slots.end()) ? 0 : itr->second;
}

const std::vector<uint32_t>* LineCapacity::StationsOf(uint32_t nodeID) const
{
    std::map<uint32_t, NodeLines>::const_iterator itr = m_nodes.find(nodeID);
    return ((itr == m_nodes.end()) or itr->second.stations.empty()) ? nullptr : &itr->second.stations;
}

uint32_t LineCapacity::BookJob(const SimJob& job) const
{
    if (job.virtualLine)
        return 0;
    uint32_t stationID(job.stationID);
    if (stationID == 0) {
        const std::vector<uint32_t>* stations(StationsOf(job.nodeID));
        stationID = (stations != nullptr) ? stations->front() : 0;
    }
    return m_book.FindJobFor(job.actorID, stationID, job.activity, job.start, job.end, job.lineID);
}

void LineCapacity::Track(uint32_t nodeID, uint8_t activity, int delta, bool onVirtual, int64_t now) const
{
    uint8_t k(LineKind::Of(activity));
    std::map<uint32_t, NodeLines>::iterator itr = m_nodes.find(nodeID);
    if ((k >= LineKind::Count) or (itr == m_nodes.end()))
        return;
    KindStats& s(itr->second.stats[k]);
    if (s.since == 0)
        s.since = s.last = now;
    if (now > s.last) {
        s.lineSeconds += (double)s.held * (double)(now - s.last);
        s.last = now;
    }
    if (delta > 0) {
        s.held += (uint32_t)delta;
        if (onVirtual)
            s.virtualHeld += (uint32_t)delta;
    } else {
        s.held -= std::min<uint32_t>(s.held, (uint32_t)-delta);
        if (onVirtual)
            s.virtualHeld -= std::min<uint32_t>(s.virtualHeld, (uint32_t)-delta);
    }
    s.peak = std::max(s.peak, s.held);
    s.virtualPeak = std::max(s.virtualPeak, s.virtualHeld);
}

uint32_t LineCapacity::Free(uint32_t actorID, uint32_t nodeID, uint8_t activity, int64_t now) const
{
    m_now = std::max(m_now, now);
    std::map<uint32_t, uint32_t>::const_iterator u = m_used.find(actorID);
    uint32_t used((u == m_used.end()) ? 0 : u->second), slots(Slots(actorID));
    if (slots <= used)
        return 0;
    std::map<uint32_t, NodeLines>::iterator itr = m_nodes.find(nodeID);
    if ((itr == m_nodes.end()) or itr->second.stations.empty())
        return 0;
    NodeLines& n(itr->second);
    uint32_t room(0);
    for (auto stationID : n.stations)
        room += m_book.Available(stationID, activity, now);
    uint8_t k(LineKind::Of(activity));
    if (k < LineKind::Count) {
        if (n.virtualLines[k] > n.virtualUsed[k])
            room += n.virtualLines[k] - n.virtualUsed[k];
        if (room == 0)
            ++n.stats[k].freeShort;
    }
    return std::min(slots - used, room);
}

bool LineCapacity::Reserve(SimJob& job, int64_t now)
{
    m_now = std::max(m_now, now);
    std::map<uint32_t, uint32_t>::const_iterator u = m_used.find(job.actorID);
    if (((u == m_used.end()) ? 0 : u->second) >= Slots(job.actorID))
        return false;
    job.virtualLine = false;
    // the node's station with the most room (the first of equals), or the job's own production site
    uint32_t stationID(0);
    const std::vector<uint32_t>* stations(StationsOf(job.nodeID));
    bool ofNode((job.stationID == 0) or ((stations != nullptr)
                and (std::find(stations->begin(), stations->end(), job.stationID) != stations->end())));
    if (ofNode and (stations != nullptr)) {
        uint32_t best(0);
        for (auto id : *stations) {
            uint32_t room(m_book.Available(id, job.activity, now));
            if (room > best) {
                best = room;
                stationID = id;
            }
        }
    } else if (!ofNode) {
        stationID = job.stationID;
    }
    uint8_t k(LineKind::Of(job.activity));
    std::map<uint32_t, NodeLines>::iterator nItr = m_nodes.find(job.nodeID);
    NodeLines* n((nItr == m_nodes.end()) ? nullptr : &nItr->second);
    // the stations are full:  the node's counted lines for the shortfall
    auto onVirtual = [&]() -> bool {
        if (!ofNode or (n == nullptr) or (stations == nullptr) or (k >= LineKind::Count) or (n->virtualUsed[k] >= n->virtualLines[k])) {
            if ((n != nullptr) and (k < LineKind::Count))
                ++n->stats[k].reserveShort;
            return false;
        }
        ++n->virtualUsed[k];
        job.virtualLine = true;
        job.stationID = stations->front();
        job.lineID = 0;
        if (job.start <= 0) {
            job.end += now - job.start;
            job.start = now;
        }
        ++m_used[job.actorID];
        Track(job.nodeID, job.activity, 1, true, now);
        return true;
    };
    if (stationID == 0)
        return onVirtual();
    JobRequest req;
    req.actorID = job.actorID;
    req.activity = job.activity;
    req.stationID = stationID;
    req.outputTypeID = job.typeID;
    req.runs = std::max<uint32_t>(1, job.runs);
    req.unitsPerRun = job.qtyOut / (Units)req.runs;
    // the book's job starts when the world's does (the world starts its jobs now)
    if (job.start <= 0) {
        job.end += now - job.start;
        job.start = now;
    }
    req.seconds = std::max<int64_t>(1, job.end - job.start);
    FacilityJob booked(m_book.Reserve(req, job.start));
    if (booked.jobID == 0)
        return onVirtual();
    job.stationID = stationID;
    job.lineID = booked.lineID;
    job.end = booked.end;
    ++m_used[job.actorID];
    Track(job.nodeID, job.activity, 1, false, now);
    return true;
}

void LineCapacity::Restore(const SimJob& job)
{
    ++m_used[job.actorID];
    if (job.virtualLine) {
        // on the node's counted lines (what its stations couldn't cover when it started)
        uint8_t k(LineKind::Of(job.activity));
        std::map<uint32_t, NodeLines>::iterator itr = m_nodes.find(job.nodeID);
        if ((k < LineKind::Count) and (itr != m_nodes.end()))
            ++itr->second.virtualUsed[k];
        Track(job.nodeID, job.activity, 1, true, std::max(m_now, job.start));
        return;
    }
    FacilityJob j;
    j.actorID = job.actorID;
    j.activity = job.activity;
    const std::vector<uint32_t>* stations(StationsOf(job.nodeID));
    j.stationID = (job.stationID != 0) ? job.stationID : ((stations != nullptr) ? stations->front() : 0);
    j.lineID = m_book.Params().realLines ? job.lineID : 0;
    j.outputTypeID = job.typeID;
    j.runs = job.runs;
    j.qtyOut = job.qtyOut;
    j.start = job.start;
    j.end = job.end;
    // what the line had before the sim isn't kept:  0 (free) is all a player's quote needs
    m_book.LoadJob(j, false);
    Track(job.nodeID, job.activity, 1, false, std::max(m_now, job.start));
}

void LineCapacity::Release(const SimJob& job)
{
    if (job.virtualLine) {
        uint8_t k(LineKind::Of(job.activity));
        std::map<uint32_t, NodeLines>::iterator itr = m_nodes.find(job.nodeID);
        if ((k < LineKind::Count) and (itr != m_nodes.end()) and (itr->second.virtualUsed[k] > 0))
            --itr->second.virtualUsed[k];
    } else {
        uint32_t id(BookJob(job));
        if (id != 0)
            m_book.Release(id, m_now);
    }
    Track(job.nodeID, job.activity, -1, job.virtualLine, m_now);
    std::map<uint32_t, uint32_t>::iterator u = m_used.find(job.actorID);
    if ((u != m_used.end()) and (u->second > 0))
        --u->second;
}

double LineCapacity::JobCost(const SimJob& job) const
{
    uint32_t id(BookJob(job));
    const FacilityJob* booked((id != 0) ? m_book.FindJob(id) : nullptr);
    if (booked != nullptr)
        return (double)booked->cost / 100.0;
    // the planner's probe, and a job on the counted lines:  what a line would cost
    return (double)Capacity::JobCost(m_probeInstall, m_probePerHour, std::max<int64_t>(0, job.end - job.start)) / 100.0;
}

double LineCapacity::Freight(const SimJob& job, double m3) const
{
    if (!m_freight or job.virtualLine or !(m3 > 0.0))
        return 0.0;
    return m3 * (double)Jumps(job.nodeID, job.stationID) * m_freightPerM3Jump;
}

double LineCapacity::RefineAllowance(uint32_t stationID, double m3, int64_t now)
{
    m_now = std::max(m_now, now);
    return m_book.RefineAllowance(stationID, m3, now);
}

double LineCapacity::RefineAllowanceAtNode(uint32_t nodeID, uint32_t stationID, double m3, int64_t now)
{
    double got(RefineAllowance(stationID, m3, now));
    if (!m_refineAtNode or (got >= m3))
        return got;
    const NodeLines* n(Node(nodeID));
    if (n == nullptr)
        return got;
    // yield and take stay the node station's (the plan's single site);  only the throughput is spread
    for (auto id : n->refineries) {
        if ((id == stationID) or (got >= m3))
            continue;
        got += m_book.RefineAllowance(id, m3 - got, now);
    }
    return std::min(got, m3);
}

uint32_t LineCapacity::CurrentLine(const SimJob& job) const
{
    if (job.virtualLine)
        return 0;
    uint32_t id(BookJob(job));
    const FacilityJob* booked((id != 0) ? m_book.FindJob(id) : nullptr);
    return (booked != nullptr) ? booked->lineID : job.lineID;
}

std::string LineCapacity::NodeDetail(uint32_t nodeID, int64_t now)
{
    std::map<uint32_t, NodeLines>::iterator itr = m_nodes.find(nodeID);
    if (itr == m_nodes.end())
        return "node " + std::to_string(nodeID) + ":  no stations\n";
    NodeLines& n(itr->second);
    char buf[512];
    std::string out;
    if (n.ranked) {
        out += "node " + std::to_string(nodeID) + " works at " + LineStations::Summary(n.choice) + "\n";
        for (auto i : n.choice.chosen) {
            const RankedStation& r(n.list[i]);
            std::snprintf(buf, sizeof(buf), "  station %u  %s  %2u jumps  mfg %u lines (sim %u, room %u)  inv %u lines (sim %u, room %u)%s\n",
                          r.stationID, r.base ? "base " : "added", r.jumps, r.lines[LineKind::Mfg], r.simCap[LineKind::Mfg],
                          m_book.Available(r.stationID, Activity::Manufacturing, now), r.lines[LineKind::Inv], r.simCap[LineKind::Inv],
                          m_book.Available(r.stationID, Activity::Invention, now), r.refinery ? "  refinery" : "");
            out += buf;
        }
    } else {
        out += "node " + std::to_string(nodeID) + " works at " + std::to_string(n.stations.size()) + " fixed stations\n";
    }
    for (uint8_t k = 0; k < LineKind::Count; ++k) {
        const KindStats& s(n.stats[k]);
        std::snprintf(buf, sizeof(buf), "  %s:  %u counted lines (%u used);  held %u now, peak %u, mean %.1f (counted:  peak %u);"
                      "  short Free %llu, Reserve %llu\n", LineKind::Name(k), n.virtualLines[k], n.virtualUsed[k], s.held, s.peak,
                      s.Mean(), s.virtualPeak, (unsigned long long)s.freeShort, (unsigned long long)s.reserveShort);
        out += buf;
    }
    return out;
}

}
