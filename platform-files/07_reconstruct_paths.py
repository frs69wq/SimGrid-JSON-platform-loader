#!/usr/bin/env python3
"""
STEP 7 — Reconstruct FABNet's ACTUAL path for each measured pair, and find the
edge-cost metric that reproduces those paths (so the emitter can route like FABNet).

For every measured ordered pair (A,B) we know from the matrix:
   * measured one-way latency
   * sites_on_path = 64 - reply_ttl   (exact # of sites/hops)
We enumerate simple paths over the FABNet-DIRECT graph with the matching hop count
and pick the one whose additive latency is closest to measured => FABNet's path.

Then we test candidate routing metrics (min-hop, min-latency, capacity-preferring)
to see which reproduces the reconstructed paths. FABNet clearly prefers the 1.2T
ring, so a capacity-tiered cost (1200G cheap, 100G dearer) should win.
"""
import glob
import json
import os
from itertools import islice

HERE = os.path.dirname(os.path.abspath(__file__))


def load_mx():
    m = {}
    for p in sorted(glob.glob(os.path.join(HERE, "matrix_*.json"))):
        m.update(json.load(open(p)))
    return m


def simple_paths(adj, src, dst, maxlen):
    """all simple paths src->dst with <= maxlen nodes (DFS)."""
    out = []
    stack = [(src, [src])]
    while stack:
        u, p = stack.pop()
        if u == dst:
            out.append(p); continue
        if len(p) >= maxlen:
            continue
        for v in adj[u]:
            if v not in p:
                stack.append((v, p + [v]))
    return out


def main():
    topo = json.load(open(os.path.join(HERE, "us_topology.json")))
    mx = load_mx()
    nominal = {frozenset((l["a"], l["b"])): l["gbps"] for l in topo["links"]}

    # direct edges (sites_on_path==2) + latency
    lat = {}
    for l in topo["links"]:
        a, b = l["a"], l["b"]
        k = f"{a}__{b}" if f"{a}__{b}" in mx else (f"{b}__{a}" if f"{b}__{a}" in mx else None)
        if k and mx[k].get("sites_on_path") == 2 and mx[k].get("one_way_ms"):
            lat[frozenset((a, b))] = mx[k]["one_way_ms"]
    adj = {}
    for e in lat:
        for x in e:
            adj.setdefault(x, set())
        a, b = tuple(e); adj[a].add(b); adj[b].add(a)
    gbps = lambda e: nominal.get(e, 100)

    # candidate metrics
    def cost_hop(e): return 1.0
    def cost_lat(e): return lat[e]
    def cost_cap(e):  # capacity-tiered (prefer 1200G ring), tiny latency tie-break
        g = gbps(e)
        base = {1200: 1.0}.get(g, 10.0 if g >= 100 else 100.0)
        return base + 1e-4 * lat[e]

    import heapq
    def dij(cost, src):
        d = {src: 0.0}; prev = {}; pq = [(0.0, src)]
        while pq:
            dd, u = heapq.heappop(pq)
            if dd > d.get(u, 1e18): continue
            for v in adj[u]:
                nd = dd + cost(frozenset((u, v)))
                if nd < d.get(v, 1e18):
                    d[v] = nd; prev[v] = u; heapq.heappush(pq, (nd, v))
        return prev
    def route(prev, s, t):
        p = [t]
        while p[-1] != s:
            if p[-1] not in prev: return None
            p.append(prev[p[-1]])
        return p[::-1]

    metrics = {"min-hop": cost_hop, "min-lat": cost_lat, "cap-tier": cost_cap}
    agree = {m: 0 for m in metrics}
    total = 0

    print(f"{'pair':11} {'meas':>7} {'sit':>3}  {'FABNet(reconstructed)':32} {'cap-tier path':32}")
    nodes = sorted(adj)
    # Scope the metric comparison to the CORE (the 9 backbone sites measured pairwise).
    # Leaf uplinks are degree-1 and route trivially, so including them only inflates
    # the agreement — the "no metric works" argument is about the core's real choices.
    core = [n for n in ["STAR", "NEWY", "WASH", "ATLA", "DALL", "LOSA", "SALT", "KANS", "SEAT"] if n in adj]
    prevs = {m: {s: dij(metrics[m], s) for s in nodes} for m in metrics}
    mismatch_examples = []
    for s in core:
        for t in core:
            if s == t: continue
            k = f"{s}__{t}"
            if k not in mx or mx[k].get("one_way_ms") is None: continue
            meas = mx[k]["one_way_ms"]; sit = mx[k]["sites_on_path"]
            # reconstruct: paths with exactly `sit` nodes, closest additive latency
            cands = [p for p in simple_paths(adj, s, t, sit) if len(p) == sit]
            best = None
            for p in cands:
                L = sum(lat[frozenset((p[i], p[i+1]))] for i in range(len(p)-1))
                if best is None or abs(L - meas) < abs(best[1] - meas):
                    best = (p, L)
            recon = "-".join(best[0]) if best else "?"
            total += 1
            for m in metrics:
                r = route(prevs[m][s], s, t)
                if best and r == best[0]:
                    agree[m] += 1
            capp = "-".join(route(prevs["cap-tier"][s], s, t) or [])
            if best and capp != recon and len(mismatch_examples) < 12:
                mismatch_examples.append((f"{s}-{t}", meas, sit, recon, capp))
            if s < t:  # print half
                print(f"{s+'-'+t:11} {meas:>7.2f} {sit:>3}  {recon:32} {capp:32}")

    print(f"\nmetric agreement with reconstructed FABNet paths (of {total} ordered pairs):")
    for m in metrics:
        print(f"  {m:9} {agree[m]:3}/{total}  ({100*agree[m]/total:.0f}%)")
    if mismatch_examples:
        print("\ncap-tier disagreements (pair, meas, sites, FABNet, cap-tier):")
        for e in mismatch_examples:
            print("  ", e)


if __name__ == "__main__":
    main()
