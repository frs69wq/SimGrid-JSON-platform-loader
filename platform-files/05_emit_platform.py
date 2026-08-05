#!/usr/bin/env python3
"""
STEP 5 — Emit the C++ SimGrid platform for the FABRIC US network, routed like FABNet.

Inputs
  us_topology.json     28 US sites, 33 physical list_links edges (+ nominal bw)
  matrix_*.json        measured min-RTT + reply-TTL for probed pairs (04 + 08)

Routing fidelity — the whole point of this study:
  * FABNetv4 (L3) does NOT forward over some physical chords (proven by reply-TTL
    hop count + latency). Those are DROPPED so SimGrid can't use them either.
  * FABNet's path choice is SPECIFIC (min-hop 89% / min-lat 85% / cap-tier 79% —
    no single metric reproduces it). So we DON'T pick with a metric: for every
    MEASURED pair we RECONSTRUCT FABNet's actual path from the data (the simple
    path whose #sites == 64-ttl and whose additive latency best matches min-RTT),
    and emit it as an EXPLICIT route. Leaf pairs (never all measured) COMPOSE:
    uplink + measured core path + downlink (proven correct: MICH->WASH observed
    = MICH-STAR-NEWY-WASH, exactly the composition).

Model
  root = add_netzone_full("fabric_us")
    per site : add_netzone_star(SITE) (ClusterZone -> NRTWsim get_host_list),
               ONE host = 1 core, gateway = that host.
    per FABNet-direct link : add_link(A__B,<nominal bw>)->set_latency(<measured one-way>)
    per site pair : explicit add_route with the reconstructed/composed link path.

    python3 05_emit_platform.py
"""
import glob
import heapq
import json
import os
from datetime import datetime, timezone

HERE = os.path.dirname(os.path.abspath(__file__))
GEN_DATE = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M UTC")

# The 9 sites measured pairwise (the backbone slice) = the routing "core".
CORE = ["STAR", "NEWY", "WASH", "ATLA", "DALL", "LOSA", "SALT", "KANS", "SEAT"]
# EDC has no orchestrator link; co-located with NCSA (same lat/lon). Nominal local link.
COLOCATE = [("EDC", "NCSA", 100, 0.05)]
HOST_SPEED = "1Gf"   # compute deliberately trivial


def bw_str(g):
    return f"{int(g)}Gbps" if float(g).is_integer() else f"{g}Gbps"


def load_mx():
    m = {}
    for p in sorted(glob.glob(os.path.join(HERE, "matrix_*.json"))):
        m.update(json.load(open(p)))
    return m


def simple_paths(adj, s, t, maxnodes):
    out, stack = [], [(s, [s])]
    while stack:
        u, p = stack.pop()
        if u == t:
            out.append(p); continue
        if len(p) >= maxnodes:
            continue
        for v in adj[u]:
            if v not in p:
                stack.append((v, p + [v]))
    return out


def main():
    topo = json.load(open(os.path.join(HERE, "us_topology.json")))
    smeta = topo["sites"]
    mx = load_mx()
    nominal = {frozenset((l["a"], l["b"])): l for l in topo["links"]}

    # 1) FABNet-direct edges (sites_on_path==2) + measured latency; drop ignored chords
    lat, srcflag = {}, {}
    ignored = []
    measured_pairs = {}   # (a,b) ordered present in matrix -> (one_way, sites)
    for k, r in mx.items():
        if r.get("one_way_ms") is None:
            continue
        a, b = k.split("__")
        measured_pairs[(a, b)] = (r["one_way_ms"], r.get("sites_on_path"))
    for l in topo["links"]:
        a, b = l["a"], l["b"]
        e = frozenset((a, b))
        r = mx.get(f"{a}__{b}") or mx.get(f"{b}__{a}")
        if r and r.get("sites_on_path") == 2 and r.get("one_way_ms"):
            lat[e] = r["one_way_ms"]; srcflag[e] = "MEASURED"
        elif r and r.get("sites_on_path") and r["sites_on_path"] > 2:
            ignored.append((a, b, r["one_way_ms"], r["sites_on_path"]))
        else:
            lat[e] = round(l["floor_ms"] * 1.45, 4); srcflag[e] = "ESTIMATED"
    for a, b, g, lt in COLOCATE:
        if a in smeta:
            lat[frozenset((a, b))] = lt; srcflag[frozenset((a, b))] = "COLOCATED"
            nominal[frozenset((a, b))] = {"gbps": g}

    # adjacency
    adj = {n: set() for n in smeta}
    for e in lat:
        a, b = tuple(e); adj[a].add(b); adj[b].add(a)

    def elat(u, v):
        return lat[frozenset((u, v))]

    # 2) reconstruct FABNet's path (node seq) for measured pairs.
    #    A ping's reply TTL times the RETURN path, and RTT (=> one-way avg) is
    #    symmetric, so the two directions' hop counts are measured INDEPENDENTLY and
    #    can differ by a hop (asymmetric routing round the SALT-STAR ring). Model the
    #    pair symmetrically by its SHORTER / higher-capacity path, so the choice does
    #    not depend on which (a,b) order we happen to read.
    def reconstruct(a, b):
        rab = measured_pairs.get((a, b)); rba = measured_pairs.get((b, a))
        if not rab and not rba:
            return None
        meas = (rab or rba)[0]
        for sit in sorted({r[1] for r in (rab, rba) if r and r[1]}):
            cands = [p for p in simple_paths(adj, a, b, sit) if len(p) == sit]
            if cands:
                return min(cands, key=lambda p: abs(
                    sum(elat(p[i], p[i + 1]) for i in range(len(p) - 1)) - meas))
        return None

    # min-hop (tie: fewer, then lower latency) fallback node seq
    def minhop(a, b):
        dist = {a: (0, 0.0)}; prev = {}; pq = [(0, 0.0, a)]
        while pq:
            h, d, u = heapq.heappop(pq)
            if (h, d) > dist.get(u, (1e9, 1e18)):
                continue
            for v in adj[u]:
                nd = (h + 1, d + elat(u, v))
                if nd < dist.get(v, (1e9, 1e18)):
                    dist[v] = nd; prev[v] = u; heapq.heappush(pq, (nd[0], nd[1], v))
        if b not in prev and b != a:
            return None
        seq = [b]
        while seq[-1] != a:
            seq.append(prev[seq[-1]])
        return seq[::-1]

    def core_seq(p, q):
        if p == q:
            return [p]
        return reconstruct(p, q) or minhop(p, q)

    # attachment: uplink node-chains from a leaf to CORE node(s)
    def attach(site):
        if site in CORE:
            return [[site]]
        # BFS out of leaf chains until hitting CORE; collect distinct core attach seqs
        res = []
        stack = [[site]]
        while stack:
            seq = stack.pop()
            u = seq[-1]
            for v in adj[u]:
                if v in seq:
                    continue
                if v in CORE:
                    res.append(seq + [v])
                elif len(seq) < 4:      # leaf-of-leaf chains (EDC->NCSA->STAR)
                    stack.append(seq + [v])
        return res or [[site]]

    def seq_lat(seq):
        return sum(elat(seq[i], seq[i + 1]) for i in range(len(seq) - 1))

    # 3) full path (node seq) for any pair, min-latency over attachment choices
    def full_path(a, b):
        # a directly-connected pair always routes over its single link
        if frozenset((a, b)) in lat:
            return [a, b]
        # otherwise reconstruct the measured path if we have it
        r = reconstruct(a, b)
        if r:
            return r
        best = None
        for ua in attach(a):
            for ub in attach(b):
                ca, cb = ua[-1], ub[-1]
                mid = core_seq(ca, cb)
                if mid is None:
                    continue
                seq = ua[:-1] + mid + ub[-2::-1]   # ua..ca + ca..cb + cb..b
                # guard against accidental node repeats
                if len(set(seq)) != len(seq):
                    continue
                L = seq_lat(seq)
                if best is None or L < best[1]:
                    best = (seq, L)
        return best[0] if best else None

    nodes = sorted(smeta)
    routes, unreachable = [], []
    for i, a in enumerate(nodes):
        for b in nodes[i + 1:]:
            seq = full_path(a, b)
            if not seq:
                unreachable.append((a, b)); continue
            hops = [f"{seq[k]}__{seq[k+1]}" for k in range(len(seq) - 1)]
            routes.append((a, b, hops))

    n_meas = sum(1 for e in lat if srcflag[e] == "MEASURED")
    n_est = sum(1 for e in lat if srcflag[e] == "ESTIMATED")

    # 4) emit
    o = []
    o.append("/* fabric_us.cpp - SimGrid S4U platform: FABRIC testbed US inter-site network,")
    o.append(" *                 ROUTED THE WAY FABNetv4 ACTUALLY ROUTES (measured).")
    o.append(f" * AUTO-GENERATED {GEN_DATE} by 05_emit_platform.py")
    o.append(" * Ground truth: live FABRIC orchestrator (list_sites/list_links) + ping min-RTT")
    o.append(" * & reply-TTL between directly-connected FABRIC VMs on FABNetv4.")
    o.append(f" *   sites  : {len(nodes)} US FABRIC sites (incl. Hawaii/HAWI); each a StarZone/")
    o.append(" *            ClusterZone with ONE host = 1 core (compute deliberately trivial).")
    o.append(f" *   links  : {len(lat)} FABNet-direct links; nominal bw, latency = measured one-way")
    o.append(f" *            ({n_meas} measured, {n_est} estimated).")
    if ignored:
        o.append(f" *   dropped: {len(ignored)} physical chords FABNet does NOT forward over (L3 takes the ring):")
        for a, b, ow, sp in ignored:
            o.append(f" *            {a}-{b}  (L3 routes {sp}-site, ~{ow:.1f}ms one-way)")
    o.append(f" *   routing: Full — {len(routes)} explicit pair routes; each core path RECONSTRUCTED")
    o.append(" *            from the measured matrix, each leaf path COMPOSED (uplink+core+downlink).")
    o.append(" * Rebuild: g++ -shared -fPIC -std=c++17 -o libfabric_us.so fabric_us.cpp -lsimgrid")
    o.append(" */")
    o.append("#include <simgrid/s4u.hpp>")
    o.append("#include <string>\n#include <vector>\n#include <unordered_map>")
    o.append("")
    o.append("namespace sg4 = simgrid::s4u;")
    o.append("")
    o.append('extern "C" void load_platform(const sg4::Engine& e);')
    o.append("void load_platform(const sg4::Engine& e)")
    o.append("{")
    o.append('  auto* root = e.get_netzone_root()->add_netzone_full("fabric_us");')
    o.append("  std::unordered_map<std::string, sg4::NetZone*> S;")
    o.append("  std::unordered_map<std::string, const sg4::Link*> L;")
    o.append("  auto site = [&](const std::string& n) {")
    o.append("    auto* z = root->add_netzone_star(n);")
    o.append(f'    auto* h = z->add_host(n + "-node0", "{HOST_SPEED}"); h->set_core_count(1);')
    o.append("    z->set_gateway(h); z->seal(); S[n] = z;")
    o.append("  };")
    o.append("  auto link = [&](const std::string& a, const std::string& b,")
    o.append("                  const std::string& bw, const std::string& lt) {")
    o.append('    auto* l = root->add_link(a + "__" + b, bw)->set_latency(lt);')
    o.append('    L[a + "__" + b] = l; L[b + "__" + a] = l;')
    o.append("  };")
    o.append("  auto route = [&](const std::string& a, const std::string& b,")
    o.append("                   const std::vector<std::string>& hops) {")
    o.append("    std::vector<sg4::LinkInRoute> r; for (auto& h : hops) r.emplace_back(L.at(h));")
    o.append("    root->add_route(S[a], S[b], r);")
    o.append("  };")
    o.append("")
    o.append("  // ---- sites (1 host / 1 core) ----")
    for s in nodes:
        o.append(f'  site("{s}");'.ljust(18) + f'// {smeta.get(s, {}).get("addr","")}')
    o.append("")
    o.append("  // ---- FABNet-direct links (nominal bw, measured one-way latency) ----")
    for e in sorted(lat, key=lambda e: (-nominal[e]["gbps"], sorted(e))):
        a, b = sorted(e)
        g = nominal[e]["gbps"]
        tag = "" if srcflag[e] == "MEASURED" else f"  [{srcflag[e]}]"
        o.append(f'  link("{a}", "{b}", "{bw_str(g)}", "{lat[e]:.4f}ms");'.ljust(50) + f'// {int(g)}G{tag}')
    o.append("")
    o.append(f"  // ---- explicit routes (Full): {len(routes)} pairs, measured-reconstructed / composed ----")
    for a, b, hops in routes:
        hs = ", ".join(f'"{h}"' for h in hops)
        o.append(f'  route("{a}", "{b}", {{{hs}}});')
    o.append("")
    o.append("  root->seal();")
    o.append("}")
    open(os.path.join(HERE, "fabric_us.cpp"), "w").write("\n".join(o) + "\n")
    print(f"[wrote] fabric_us.cpp  sites={len(nodes)} links={len(lat)} "
          f"({n_meas} meas,{n_est} est) dropped={len(ignored)} routes={len(routes)}")
    if unreachable:
        print(f"  !! UNREACHABLE: {len(unreachable)} e.g. {unreachable[:6]}")


if __name__ == "__main__":
    main()
