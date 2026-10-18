 /**
  * @name SimLineStations.cpp
  *   market simulation:  the stations a node's industry works at (see SimLineStations.h)
  *
  * @date:          17Oct26
  *
  */

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <set>
#include <unordered_map>

#include "market/sim/SimFacilities.h"
#include "market/sim/SimLineStations.h"
#include "market/sim/SimLogistics.h"

namespace MktSim {

uint8_t LineKind::Of(uint8_t activity)
{
    if (activity == Activity::Manufacturing)
        return Mfg;
    if (activity == Activity::Invention)
        return Inv;
    return Count;
}

const char* LineKind::Name(uint8_t kind)
{
    return (kind == Mfg) ? "manufacturing" : ((kind == Inv) ? "invention" : "?");
}

uint32_t LineStations::SimCap(uint32_t lines, double maxShare, uint32_t minFree)
{
    uint32_t cap(Capacity::ShareCap(lines, maxShare));
    uint32_t keep((lines > minFree) ? lines - minFree : 0);
    return std::min(cap, keep);
}

namespace {
    typedef std::unordered_map<uint32_t, std::vector<uint32_t>> Graph;

    Graph MakeGraph(const std::vector<std::pair<uint32_t, uint32_t>>& jumps)
    {
        Graph g;
        for (auto& j : jumps) {
            g[j.first].push_back(j.second);
            g[j.second].push_back(j.first);
        }
        // deterministic whatever the input's order
        for (auto& cur : g)
            std::sort(cur.second.begin(), cur.second.end());
        return g;
    }

    /* jumps from 'from' to every system reached within maxJumps.  highOnly:  only high-sec systems are entered (the start
     *  counts whatever its band) */
    std::unordered_map<uint32_t, uint16_t> Reach(const Graph& g, const std::map<uint32_t, MapSystem>& systems, uint32_t from,
                                                 uint32_t maxJumps, bool highOnly)
    {
        std::unordered_map<uint32_t, uint16_t> dist;
        dist[from] = 0;
        std::deque<uint32_t> queue(1, from);
        while (!queue.empty()) {
            uint32_t at(queue.front());
            queue.pop_front();
            uint16_t d(dist[at]);
            if (d >= maxJumps)
                continue;
            Graph::const_iterator itr = g.find(at);
            if (itr == g.end())
                continue;
            for (auto next : itr->second) {
                if (dist.find(next) != dist.end())
                    continue;
                if (highOnly) {
                    std::map<uint32_t, MapSystem>::const_iterator s = systems.find(next);
                    if ((s == systems.end()) or (Logi::Band(s->second.security) != SecBand::High))
                        continue;
                }
                dist[next] = (uint16_t)(d + 1);
                queue.push_back(next);
            }
        }
        return dist;
    }

    RankedStation Entry(const LineStation& s, uint16_t jumps, bool base, const NodeStationParams& p)
    {
        RankedStation r;
        r.stationID = s.stationID;
        r.jumps = jumps;
        r.base = base;
        r.refinery = s.refinery;
        r.lines[LineKind::Mfg] = s.mfgLines;
        r.lines[LineKind::Inv] = s.invLines;
        for (uint8_t k = 0; k < LineKind::Count; ++k)
            r.simCap[k] = (uint16_t)LineStations::SimCap(r.lines[k], p.maxLineShare, p.minFreeLines);
        return r;
    }
}

std::vector<RankedStation> LineStations::Rank(const NodeData& node, const std::vector<LineStation>& stations,
                                              const std::map<uint32_t, MapSystem>& systems,
                                              const std::vector<std::pair<uint32_t, uint32_t>>& jumps,
                                              const NodeStationParams& p, const std::vector<uint32_t>* fixed)
{
    std::map<uint32_t, const LineStation*> byID;
    for (auto& s : stations)
        byID[s.stationID] = &s;
    std::map<uint32_t, MapSystem>::const_iterator home = systems.find(node.systemID);
    uint32_t constellation((home == systems.end()) ? 0 : home->second.constellationID);
    float security((home == systems.end()) ? node.security : home->second.security);
    Graph g(MakeGraph(jumps));
    // any route (the base's distances, for the freight) and the band's route (the candidates)
    std::unordered_map<uint32_t, uint16_t> any(Reach(g, systems, node.systemID, 0xFFFF, false));
    bool highOnly(p.sameBand and (Logi::Band(security) == SecBand::High));
    std::unordered_map<uint32_t, uint16_t> band(highOnly ? Reach(g, systems, node.systemID, p.maxJumps, true)
                                                         : Reach(g, systems, node.systemID, p.maxJumps, false));
    auto jumpsAny = [&](uint32_t systemID) -> uint16_t {
        std::unordered_map<uint32_t, uint16_t>::const_iterator itr = any.find(systemID);
        return (itr == any.end()) ? 0 : itr->second;
    };

    std::vector<RankedStation> out;
    std::set<uint32_t> taken;
    // the node's own station, whatever its lines (Rens VI-8 has none)
    {
        std::map<uint32_t, const LineStation*>::const_iterator itr = byID.find(node.stationID);
        LineStation own;
        own.stationID = node.stationID;
        own.systemID = node.systemID;
        RankedStation r(Entry((itr != byID.end()) ? *itr->second : own, 0, true, p));
        out.push_back(r);
        taken.insert(node.stationID);
    }
    // the base:  the constellation's stations with lines (or the operator's list)
    std::vector<const LineStation*> base;
    if (fixed != nullptr) {
        for (auto id : *fixed) {
            std::map<uint32_t, const LineStation*>::const_iterator itr = byID.find(id);
            if ((itr != byID.end()) and (taken.insert(id).second))
                base.push_back(itr->second);
        }
    } else if (constellation != 0) {
        for (auto& s : stations)
            if ((s.constellationID == constellation) and ((s.mfgLines > 0) or (s.invLines > 0)) and (taken.find(s.stationID) == taken.end()))
                base.push_back(&s);
        std::stable_sort(base.begin(), base.end(), [&](const LineStation* a, const LineStation* b) {
            bool ha(a->systemID == node.systemID), hb(b->systemID == node.systemID);
            if (ha != hb)
                return ha;
            return a->stationID < b->stationID;
        });
        for (auto s : base)
            taken.insert(s->stationID);
    }
    for (auto s : base)
        out.push_back(Entry(*s, jumpsAny(s->systemID), true, p));
    if (fixed != nullptr)
        return out;         // the operator's list is the node's, exactly

    // the candidates
    struct Cand { const LineStation* s; uint16_t jumps; };
    std::vector<Cand> cands;
    for (auto& s : stations) {
        if ((taken.find(s.stationID) != taken.end()) or ((s.mfgLines == 0) and (s.invLines == 0)))
            continue;
        std::unordered_map<uint32_t, uint16_t>::const_iterator d = band.find(s.systemID);
        if ((d == band.end()) or (d->second > p.maxJumps))
            continue;
        if (highOnly) {
            std::map<uint32_t, MapSystem>::const_iterator sys = systems.find(s.systemID);
            if ((sys == systems.end()) or (Logi::Band(sys->second.security) != SecBand::High))
                continue;
        }
        cands.push_back(Cand{&s, d->second});
    }
    std::sort(cands.begin(), cands.end(), [&](const Cand& a, const Cand& b) {
        if (a.jumps != b.jumps)
            return a.jumps < b.jumps;
        bool ra(a.s->regionID == node.regionID), rb(b.s->regionID == node.regionID);
        if (ra != rb)
            return ra;
        return a.s->stationID < b.s->stationID;
    });
    for (auto& c : cands)
        out.push_back(Entry(*c.s, c.jumps, false, p));
    return out;
}

NodeChoice LineStations::Choose(const std::vector<RankedStation>& ranked, const double need[LineKind::Count],
                                const NodeStationParams& p, std::map<uint32_t, std::array<double, LineKind::Count>>& claimed)
{
    NodeChoice c;
    for (uint8_t k = 0; k < LineKind::Count; ++k) {
        c.need[k] = std::max(0.0, need[k]);
        c.wanted[k] = p.scaleWithNeed ? c.need[k] * std::max(0.0, p.headroom) : 0.0;
    }
    auto satisfied = [&](uint8_t k) -> bool {
        return (c.stations[k] >= p.minStations[k]) and (c.have[k] + 1e-9 >= c.wanted[k]);
    };
    std::vector<bool> in(ranked.size(), false);
    auto take = [&](size_t i) {
        const RankedStation& r(ranked[i]);
        std::array<double, LineKind::Count>& cl(claimed[r.stationID]);
        in[i] = true;
        ++c.count;
        if (!r.base)
            ++c.added;
        c.farthest = std::max(c.farthest, r.jumps);
        for (uint8_t k = 0; k < LineKind::Count; ++k) {
            if (r.lines[k] == 0)
                continue;
            ++c.stations[k];
            double avail(std::max(0.0, (double)r.simCap[k] - cl[k]));
            cl[k] += std::min(avail, std::max(0.0, c.wanted[k] - c.have[k]));
            c.have[k] += avail;
        }
    };
    // the base, whatever it holds
    for (size_t i = 0; i < ranked.size(); ++i)
        if (ranked[i].base)
            take(i);
    /* then per activity, the scarcer first (research stations are a quarter of the stations with lines, and most of
     *  them have manufacturing lines too):  the next candidates with lines of it while it is short */
    const uint8_t order[LineKind::Count] = { LineKind::Inv, LineKind::Mfg };
    for (auto k : order) {
        for (size_t i = 0; i < ranked.size(); ++i) {
            if (satisfied(k) or (c.count >= p.maxStations))
                break;
            const RankedStation& r(ranked[i]);
            if (in[i] or (r.lines[k] == 0))
                continue;
            double avail(std::max(0.0, (double)r.simCap[k] - claimed[r.stationID][k]));
            if ((c.stations[k] < p.minStations[k]) or (avail > 0.0))
                take(i);
        }
    }
    for (size_t i = 0; i < ranked.size(); ++i)
        if (in[i])
            c.chosen.push_back((uint32_t)i);
    for (uint8_t k = 0; k < LineKind::Count; ++k) {
        c.shortfall[k] = std::max(0.0, c.wanted[k] - c.have[k]);
        if (c.shortfall[k] < 1e-9)
            c.shortfall[k] = 0.0;
        c.virtualLines[k] = p.virtualFill ? (uint32_t)std::ceil(c.shortfall[k] - 1e-9) : 0;
    }
    return c;
}

std::map<uint32_t, std::vector<uint32_t>> LineStations::ParseOverride(const std::string& text)
{
    std::map<uint32_t, std::vector<uint32_t>> out;
    size_t at(0);
    while (at < text.size()) {
        size_t end(text.find(';', at));
        if (end == std::string::npos)
            end = text.size();
        std::string part(text.substr(at, end - at));
        at = end + 1;
        size_t colon(part.find(':'));
        if (colon == std::string::npos)
            continue;
        uint32_t node((uint32_t)std::strtoul(part.substr(0, colon).c_str(), nullptr, 10));
        if (node == 0)
            continue;
        std::vector<uint32_t>& list(out[node]);
        size_t p(colon + 1);
        while (p < part.size()) {
            size_t comma(part.find(',', p));
            if (comma == std::string::npos)
                comma = part.size();
            uint32_t id((uint32_t)std::strtoul(part.substr(p, comma - p).c_str(), nullptr, 10));
            if (id != 0)
                list.push_back(id);
            p = comma + 1;
        }
    }
    return out;
}

std::string LineStations::Summary(const NodeChoice& c)
{
    char buf[512];
    std::snprintf(buf, sizeof(buf), "%u stations (mfg %u, inv %u;  farthest %u jumps;  +%u beyond its constellation)",
                  (uint32_t)c.count, c.stations[LineKind::Mfg], c.stations[LineKind::Inv], c.farthest, c.added);
    std::string out(buf);
    for (uint8_t k = 0; k < LineKind::Count; ++k) {
        std::snprintf(buf, sizeof(buf), ";  %s need %.1f, wants %.1f sim lines, has %.0f", LineKind::Name(k), c.need[k], c.wanted[k], c.have[k]);
        out += buf;
        if (c.shortfall[k] > 0.0) {
            std::snprintf(buf, sizeof(buf), " (short %.1f%s)", c.shortfall[k],
                          (c.virtualLines[k] > 0) ? (", " + std::to_string(c.virtualLines[k]) + " virtual lines serve it").c_str() : ", not served");
            out += buf;
        }
    }
    return out;
}

}
