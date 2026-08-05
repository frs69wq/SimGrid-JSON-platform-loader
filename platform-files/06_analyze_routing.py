#!/usr/bin/env python3
"""
STEP 6 — Does shortest-path over the FABNet-DIRECT graph reproduce FABNet's
actual measured routing?  (validates the platform's routing model)

Inputs: us_topology.json + every matrix_*.json produced by 04_matrix_probe.py.

  1. FABNet-direct edges  = list_links pairs measured with sites_on_path==2
     (ttl 62). Each carries its measured one-way latency.
  2. Over that graph, for EVERY measured ordered pair, compute the predicted
     path two ways:
        - min-HOP (what SimGrid Floyd does), tie-broken by lower latency
        - min-LATENCY (Dijkstra) — the natural "route like the WAN" choice
     and its predicted one-way latency = sum of direct-edge latencies.
  3. Compare predicted vs MEASURED (matrix) latency + hop count. If they agree,
     shortest-path-over-direct-graph == FABNet routing, so the SimGrid platform
     built from the direct edges will route like FABRIC.

Prints per-pair agreement + worst offenders + which edges are direct/ignored.
"""
import glob
import heapq
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))


def load_matrices():
    m = {}
    for p in glob.glob(os.path.join(HERE, "matrix_*.json")):
        m.update(json.load(open(p)))
    return m


def dijkstra(adj, src, weight):
    """weight(a,b)->cost. returns dist, prev."""
    dist = {src: 0.0}
    prev = {}
    pq = [(0.0, src)]
    while pq:
        d, u = heapq.heappop(pq)
        if d > dist.get(u, 1e18):
            continue
        for v in adj[u]:
            nd = d + weight(u, v)
            if nd < dist.get(v, 1e18):
                dist[v] = nd
                prev[v] = u
                heapq.heappush(pq, (nd, v))
    return dist, prev


def path(prev, src, dst):
    if dst != src and dst not in prev:
        return None
    p = [dst]
    while p[-1] != src:
        p.append(prev[p[-1]])
    return p[::-1]


def main():
    topo = json.load(open(os.path.join(HERE, "us_topology.json")))
    mx = load_matrices()

    # direct edge latency: for each list_links pair, if measured with 2 sites-on-path, it's direct
    direct = {}   # frozenset({a,b}) -> latency ms
    ignored = []  # (a,b, one_way, sites)
    for l in topo["links"]:
        a, b = l["a"], l["b"]
        k = f"{a}__{b}" if f"{a}__{b}" in mx else (f"{b}__{a}" if f"{b}__{a}" in mx else None)
        if not k:
            continue
        r = mx[k]
        if r.get("sites_on_path") == 2 and r.get("one_way_ms"):
            direct[frozenset((a, b))] = r["one_way_ms"]
        elif r.get("sites_on_path") and r["sites_on_path"] > 2:
            ignored.append((a, b, r["one_way_ms"], r["sites_on_path"]))

    # build adjacency
    nodes = set()
    for e in direct:
        nodes |= set(e)
    adj = {n: set() for n in nodes}
    for e in direct:
        a, b = tuple(e)
        adj[a].add(b)
        adj[b].add(a)

    latw = lambda u, v: direct[frozenset((u, v))]
    hopw = lambda u, v: 1.0 + 1e-6 * direct[frozenset((u, v))]  # min-hop, tie-break by latency

    print(f"FABNet-DIRECT edges: {len(direct)}   nodes: {len(nodes)}")
    print(f"FABNet-IGNORED physical chords: {[(a,b) for a,b,_,_ in ignored]}\n")

    # validate: predicted (min-latency) vs measured, for every measured pair among these nodes
    print(f"{'pair':13} {'meas_ms':>8} {'pred_ms':>8} {'d_ms':>6} {'meas_sit':>8} {'pred_hop':>8}  path(min-lat)")
    rows = []
    for src in sorted(nodes):
        distL, prevL = dijkstra(adj, src, latw)
        distH, prevH = dijkstra(adj, src, hopw)
        for dst in sorted(nodes):
            if dst == src:
                continue
            k = f"{src}__{dst}"
            if k not in mx or mx[k].get("one_way_ms") is None:
                continue
            meas = mx[k]["one_way_ms"]
            meas_sites = mx[k]["sites_on_path"]
            pL = path(prevL, src, dst)
            predL = distL.get(dst)
            pH = path(prevH, src, dst)
            if predL is None:
                continue
            rows.append((abs(predL - meas), src, dst, meas, predL, meas_sites, len(pL), pL))

    rows.sort(reverse=True)
    shown = 0
    for d, src, dst, meas, predL, msit, phop, pL in rows:
        flag = "  <-- MISMATCH" if d > 1.5 else ""
        if d > 1.5 or shown < 8:
            print(f"{src+'-'+dst:13} {meas:>8.3f} {predL:>8.3f} {d:>6.2f} {str(msit):>8} {phop:>8}  {'-'.join(pL)}{flag}")
            shown += 1
    # summary
    errs = [r[0] for r in rows]
    import statistics
    print(f"\npairs checked: {len(rows)}")
    print(f"latency abs err  mean={statistics.mean(errs):.3f}ms  median={statistics.median(errs):.3f}ms  max={max(errs):.3f}ms")
    print(f"pairs within 1.5ms: {sum(1 for e in errs if e<=1.5)}/{len(errs)}")


if __name__ == "__main__":
    main()
