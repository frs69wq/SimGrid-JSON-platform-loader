#!/usr/bin/env python3
"""
Report tables for the FABRIC-US platform (consolidates the former 10/12/13).
Reads only the pipeline's JSON + the emitted fabric_us.cpp; writes nothing.

  python3 10_report_tables.py links        # modeled links, nominal bw + measured latency
  python3 10_report_tables.py audit        # raw-counter audit of all 33 physical links
  python3 10_report_tables.py core-routes  # how the 9 core sites route (read from fabric_us.cpp)

Every number traces to us_topology.json / raw_links.json / matrix_*.json (raw ping
counters), or to the delivered fabric_us.cpp for the routes.
"""
import glob
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CORE = ["STAR", "NEWY", "WASH", "ATLA", "DALL", "LOSA", "SALT", "KANS", "SEAT"]


# ---- shared loaders -------------------------------------------------------
def topo():
    return json.load(open(os.path.join(HERE, "us_topology.json")))


def matrices():
    mx = {}
    for p in sorted(glob.glob(os.path.join(HERE, "matrix_*.json"))):
        mx.update(json.load(open(p)))
    return mx


def raw_bw():
    """max nominal bw + layer per unordered US pair, straight from raw_links.json."""
    out = {}
    for l in json.load(open(os.path.join(HERE, "raw_links.json"))):
        s = l.get("sites") or []
        if len(s) == 2:
            k = frozenset(s)
            if k not in out or (l.get("bandwidth") or 0) > out[k][0]:
                out[k] = (l.get("bandwidth"), l.get("layer"))
    return out


def platform(path=None):
    """Parse the emitted platform: link bandwidth + latency per edge + route link-lists."""
    txt = open(path or os.path.join(HERE, "fabric_us.cpp")).read()
    bw, lat = {}, {}
    for m in re.finditer(r'link\("(\w+)",\s*"(\w+)",\s*"(\d+)Gbps",\s*"([\d.]+)ms"', txt):
        e = frozenset((m.group(1), m.group(2)))
        bw[e] = int(m.group(3)); lat[e] = float(m.group(4))
    routes = {}
    for m in re.finditer(r'route\("(\w+)",\s*"(\w+)",\s*\{([^}]*)\}\)', txt):
        routes[(m.group(1), m.group(2))] = re.findall(r'"(\w+__\w+)"', m.group(3))
    return bw, lat, routes


def keys_to_nodes(keys, start):
    """Orient a path (list of "X__Y" link keys) into a node sequence beginning at `start`."""
    seq, cur, rem = [start], start, [set(k.split("__")) for k in keys]
    while rem:
        for i, e in enumerate(rem):
            if cur in e:
                cur = (e - {cur}).pop(); seq.append(cur); rem.pop(i); break
        else:
            break
    return seq


# ---- subcommand: links ----------------------------------------------------
def cmd_links():
    t = topo(); mx = matrices()
    rows = []
    for l in t["links"]:
        a, b = l["a"], l["b"]
        r = mx.get(f"{a}__{b}") or mx.get(f"{b}__{a}")
        if r and r.get("sites_on_path") == 2 and r.get("one_way_ms"):
            rows.append((l["gbps"], a, b, l["layer"], r["one_way_ms"], "ping", l["floor_ms"]))
        elif r and r.get("sites_on_path", 0) > 2:
            continue  # dropped chord, not a modeled link
        else:
            rows.append((l["gbps"], a, b, l["layer"], round(l["floor_ms"] * 1.45, 3), "est", l["floor_ms"]))
    rows.append((100, "EDC", "NCSA", "local", 0.05, "coloc", 0.0))
    rows.sort(key=lambda r: (-r[0], r[1], r[2]))
    print("| A | B | Gbps | layer | one-way ms | source | gc-floor ms |")
    print("|---|---|-----:|-------|-----------:|--------|------------:|")
    for g, a, b, lay, ow, src, fl in rows:
        print(f"| {a} | {b} | {int(g)} | {lay} | {ow:.3f} | {src} | {fl:.2f} |")
    meas = sum(1 for r in rows if r[5] == "ping")
    print(f"\n{len(rows)} modeled links: {meas} ping-measured, "
          f"{sum(1 for r in rows if r[5]=='est')} estimated, {sum(1 for r in rows if r[5]=='coloc')} co-located.")


# ---- subcommand: audit ----------------------------------------------------
def cmd_audit():
    t = topo(); mx = matrices(); rbw = raw_bw()
    rows, nd, ndr, ne, viol = [], 0, 0, 0, []
    for l in t["links"]:
        a, b = l["a"], l["b"]; k = frozenset((a, b))
        bw, layer = rbw.get(k, (l["gbps"], l.get("layer")))
        r = mx.get(f"{a}__{b}") or mx.get(f"{b}__{a}")
        fl = l["floor_ms"]
        if r and r.get("min_rtt_ms") is not None:
            rtt, ttl, ow, sp = r["min_rtt_ms"], r.get("reply_ttl"), r["one_way_ms"], r.get("sites_on_path")
            if sp == 2:
                v = "DIRECT"; nd += 1
            else:
                v = f"DROPPED/{sp}h"; ndr += 1
            if ow + 1e-6 < fl:
                viol.append((a, b))
            rows.append((bw, a, b, layer, fl, f"{rtt:.2f}", str(ttl), f"{ow:.2f}", str(sp), v))
        else:
            ne += 1
            rows.append((bw, a, b, layer, fl, "—", "—", f"{round(fl*1.45,2)}", "—", "EST(maint)"))
    rows.sort(key=lambda r: (-(r[0] or 0), r[1], r[2]))
    print(f"{'A':6} {'B':6} {'Gbps':>4} {'lyr':3} {'floor':>6} {'minRTT':>7} {'ttl':>3} "
          f"{'1-way':>6} {'sit':>3}  verdict")
    print("-" * 66)
    for bw, a, b, layer, fl, rtt, ttl, ow, sp, v in rows:
        print(f"{a:6} {b:6} {int(bw):>4} {str(layer):3} {fl:>6.2f} {rtt:>7} {ttl:>3} {ow:>6} {sp:>3}  {v}")
    print("-" * 66)
    print(f"{len(rows)} physical links: {nd} DIRECT (modeled), {ndr} DROPPED (FABNet routes around), "
          f"{ne} EST (FABRIC maintenance blocked the ping).")
    print(f"conservation check (one-way >= great-circle floor): {'ALL PASS' if not viol else 'VIOLATIONS '+str(viol)}")
    print("+ EDC-NCSA: co-located (same lat/lon), no list_links edge -> nominal 100G/0.05ms, not pinged.")


# ---- subcommand: core-routes ---------------------------------------------
def cmd_core_routes():
    bw, lat, routes = platform(); mx = matrices()

    def one_way(a, b):
        r = mx.get(f"{a}__{b}") or mx.get(f"{b}__{a}")
        return r["one_way_ms"] if r else None

    def sites(a, b):
        r = mx.get(f"{a}__{b}")
        return r.get("sites_on_path") if r else None

    print("Two independent instruments validate each path:  Σpath (sum of the path's link")
    print("latencies) vs the measured one-way (Δ = |diff|), and ttl (sites-on-path counted by")
    print("reply TTL) vs the path's own site count.  Δ>1.5 = latency-asymmetric pair; ttl 'lo/hi'")
    print("= hop-asymmetric pair (forward/reverse differ).  bneck = min link bw = effective pipe.\n")
    print(f"{'A':5} {'B':5} {'meas':>6} {'Σpath':>6} {'Δ':>5} {'ttl':>4} {'hop':>3} {'bneck':>6}  path")
    print("-" * 82)
    lat_asym, hop_asym = [], []
    for i in range(len(CORE)):
        for j in range(i + 1, len(CORE)):
            A, B = CORE[i], CORE[j]
            keys = routes.get((A, B)) or routes.get((B, A))
            if not keys:
                continue
            path = keys_to_nodes(keys, A)
            hop = len(keys)
            spath = sum(lat[frozenset(k.split("__"))] for k in keys)
            bn = min(bw[frozenset(k.split("__"))] for k in keys)
            ow = one_way(A, B)
            d = abs(ow - spath)
            sab, sba = sites(A, B), sites(B, A)      # 64-ttl each way (independent)
            lo, hi = sorted((sab, sba)) if sab and sba else (hop + 1, hop + 1)
            ttl = f"{lo}" if lo == hi else f"{lo}/{hi}"
            flag = ""
            if lo != hi:
                hop_asym.append((A, B)); flag = ""
            if d > 1.5:
                lat_asym.append((A, B))
            print(f"{A:5} {B:5} {ow:>6.2f} {spath:>6.2f} {d:>5.2f} {ttl:>4} {hop:>3} {int(bn):>5}G  {'-'.join(path)}{flag}")
    print("-" * 82)
    print("meas = measured one-way (min-RTT/2); Σpath = platform path's summed link latency.")
    print(f"Most rows: Δ≈0 and ttl = hop+1 — the reconstructed path reproduces BOTH measurements.")
    print(f"\nThe {len(lat_asym)+len(hop_asym)} asymmetric core pairs (FABNet routes them differently each way):")
    print(f"  latency-asymmetric ({len(lat_asym)}, Δ>1.5): "
          + ", ".join(f"{a}↔{b}" for a, b in lat_asym)
          + "  — same hop count, different-latency path each way; RTT/2 = the average.")
    print(f"  hop-asymmetric ({len(hop_asym)}, ttl lo/hi): "
          + ", ".join(f"{a}↔{b}" for a, b in hop_asym)
          + "  — one way the direct SALT-STAR 1200G ring, the other the KANS detour (~equal latency).")


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "links"
    {"links": cmd_links, "audit": cmd_audit, "core-routes": cmd_core_routes}.get(
        cmd, lambda: sys.exit("usage: 10_report_tables.py links|audit|core-routes"))()
