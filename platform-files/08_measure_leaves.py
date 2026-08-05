#!/usr/bin/env python3
"""
STEP 8 — Measure every leaf's uplink latency by cross-slice FABNet ping.

The backbone slice (fab-backbone: the 7 ring PoPs + KANS + SEAT) stays UP as the
anchor. A leaf slice holds the degree-1 (and HAWI degree-2) edge sites. FABNetv4
is testbed-wide, so a leaf pings its PoP's backbone IP and the RTT is the direct
uplink (proven: MICH->STAR ttl=62, one-way 2.55ms). We record min-RTT + reply-TTL
(sites_on_path = 64-ttl; ==2 => the uplink is a true single FABNet hop).

Writes matrix_<leafslice>.json in the SAME schema 04 uses, so 05/07 pick it up.

    FAB_ANCHOR=fab-backbone FAB_SLICE=fab-leaves-a python3 08_measure_leaves.py
"""
import json
import os
import re
import sys

from fabrictestbed_extensions.fablib.fablib import FablibManager

HERE = os.path.dirname(os.path.abspath(__file__))
ANCHOR = os.environ.get("FAB_ANCHOR", "fab-backbone")
LEAFSL = os.environ.get("FAB_SLICE", "fab-leaves-a")
COUNT = int(os.environ.get("MX_COUNT", "30"))


def ipof(n):
    return str(n.get_interface(network_name=f"FABNET_IPv4_{n.get_site()}").get_ip_addr())


def main():
    topo = json.load(open(os.path.join(HERE, "us_topology.json")))
    # neighbor (PoP) of each site in the physical graph
    nbr = {}
    for l in topo["links"]:
        nbr.setdefault(l["a"], set()).add(l["b"])
        nbr.setdefault(l["b"], set()).add(l["a"])

    f = FablibManager()
    anchor = f.get_slice(ANCHOR)
    POP_IP = {n.get_site(): ipof(n) for n in anchor.get_nodes()}
    print(f"[anchor] {ANCHOR} PoPs: {sorted(POP_IP)}")

    leaves = f.get_slice(LEAFSL)
    LN = {}
    for n in leaves.get_nodes():
        try:
            st = n.get_reservation_state()
            if st != "Active":
                print(f"  [skip] {n.get_site()} state={st}")
                continue
            _ = ipof(n)  # ensure it has a FABNet IP
            LN[n.get_site()] = n
        except Exception as e:
            print(f"  [skip] {n.get_site()} ({e})")
    print(f"[leaves] {LEAFSL} Active: {sorted(LN)}")

    out = {}
    for site, node in LN.items():
        pops = [p for p in sorted(nbr.get(site, [])) if p in POP_IP]  # its uplink PoP(s)
        if not pops:
            print(f"  {site}: no PoP neighbor in anchor -- skip")
            continue
        for pop in pops:
            ip = POP_IP[pop]
            out_s, _ = node.execute(
                f'R=$(ping -c {COUNT} -i 0.2 -W 2 {ip} 2>/dev/null); '
                f'echo "$R" | sed -n "s#.*= \\([0-9.]*\\)/.*#MIN=\\1#p"; '
                f'echo "$R" | grep -o "ttl=[0-9]*" | head -1', quiet=True)
            mn = re.search(r"MIN=([\d.]+)", out_s or "")
            tt = re.search(r"ttl=(\d+)", out_s or "")
            if not mn:
                print(f"  {site}->{pop}: PING FAILED ({(out_s or '').strip()[:60]})")
                continue
            rtt = float(mn.group(1))
            ttl = int(tt.group(1)) if tt else None
            key = f"{site}__{pop}"
            out[key] = {"src": site, "min_rtt_ms": rtt, "reply_ttl": ttl,
                        "sites_on_path": (64 - ttl) if ttl else None,
                        "one_way_ms": round(rtt / 2, 4)}
            verdict = "DIRECT" if (ttl == 62) else f"{64-ttl if ttl else '?'} sites"
            print(f"  {site:6}->{pop:6} one_way={rtt/2:7.3f}ms  ttl={ttl}  {verdict}")

    path = os.path.join(HERE, f"matrix_{LEAFSL}.json")
    # merge if exists
    if os.path.exists(path):
        prev = json.load(open(path)); prev.update(out); out = prev
    json.dump(out, open(path, "w"), indent=2, sort_keys=True)
    print(f"[wrote] {path}  ({len(out)} leaf uplinks)")


if __name__ == "__main__":
    main()
