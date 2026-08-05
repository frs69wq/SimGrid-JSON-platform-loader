#!/usr/bin/env python3
"""
STEP 4 — Full pairwise latency + reply-TTL matrix over a live slice.

WHY: FABNetv4 (L3 routed IP) does NOT necessarily forward over the physical link
that list_links reports. Some physical "chord" links are ignored and the traffic
rides the 1.2T ring instead. To make the SimGrid platform route like FABRIC we
must learn FABNet's ACTUAL path for each pair. Two independent instruments:

  * min(RTT)/2         one-way latency of the ACTUAL FABNet path
  * reply TTL          #sites on path = 64 - ttl   (Linux initial TTL 64; every
                       site's FABNet gateway decrements once). 2 => directly
                       forwarded; >2 => routed through (ttl-implied) transit PoPs.

A list_links pair is a TRUE FABNet-direct link  <=>  ttl==62 (2 sites on path)
AND min-RTT ~ 2*great-circle floor. Otherwise FABNet routes it multi-hop and we
must NOT model it as a direct SimGrid link.

    FAB_SLICE=fab-backbone python3 04_matrix_probe.py   -> writes matrix_<slice>.json
"""
import json
import os
import re
import sys

from fabrictestbed_extensions.fablib.fablib import FablibManager

HERE = os.path.dirname(os.path.abspath(__file__))
SLICE = os.environ.get("FAB_SLICE", "fab-backbone")
COUNT = int(os.environ.get("MX_COUNT", "10"))


def node_ip(n):
    return str(n.get_interface(network_name=f"FABNET_IPv4_{n.get_site()}").get_ip_addr())


def main():
    f = FablibManager()
    s = f.get_slice(SLICE)
    N = {n.get_site(): n for n in s.get_nodes()}
    IP = {}
    for site, n in N.items():
        try:
            IP[site] = node_ip(n)
        except Exception:
            pass
    sites = sorted(IP)
    print(f"[matrix] slice={SLICE} sites={sites}")

    out = {}  # "A__B" -> {min,avg,ttl,sites_on_path}
    for a in sites:
        # one ssh call: ping every other site from a, print pair|min|ttl
        script = []
        for b in sites:
            if a == b:
                continue
            script.append(
                f'R=$(ping -c {COUNT} -i 0.2 -W 2 {IP[b]} 2>/dev/null); '
                f'M=$(echo "$R" | sed -n "s#.*= [0-9.]*/\\([0-9.]*\\)/.*#\\1#p"); '  # avg placeholder
                f'MIN=$(echo "$R" | sed -n "s#.*= \\([0-9.]*\\)/.*#\\1#p"); '
                f'T=$(echo "$R" | grep -o "ttl=[0-9]*" | head -1 | cut -d= -f2); '
                f'echo "{a}__{b}|$MIN|$T"'
            )
        cmd = " ; ".join(script)
        res, _ = N[a].execute(cmd, quiet=True)
        for line in (res or "").splitlines():
            line = line.strip()
            m = re.match(r"([A-Z]+__[A-Z]+)\|([\d.]*)\|(\d*)", line)
            if not m:
                continue
            key, mn, ttl = m.group(1), m.group(2), m.group(3)
            rec = {"src": a, "min_rtt_ms": float(mn) if mn else None,
                   "reply_ttl": int(ttl) if ttl else None,
                   "sites_on_path": (64 - int(ttl)) if ttl else None,
                   "one_way_ms": round(float(mn) / 2, 4) if mn else None}
            out[key] = rec
        print(f"  probed from {a}: {sum(1 for k in out if k.startswith(a+'__'))} dsts")

    path = os.path.join(HERE, f"matrix_{SLICE}.json")
    json.dump(out, open(path, "w"), indent=2, sort_keys=True)
    print(f"[matrix] wrote {path}  ({len(out)} ordered pairs)")

    # quick view: for the list_links edges present, direct vs multi-hop
    topo = json.load(open(os.path.join(HERE, "us_topology.json")))
    print("\nlist_links edges present in this slice — FABNet verdict:")
    print(f"{'pair':14} {'one_way':>8} {'floor':>6} {'ratio':>5} {'sites':>5}  verdict")
    for l in topo["links"]:
        a, b = l["a"], l["b"]
        if a in sites and b in sites:
            k = f"{a}__{b}" if f"{a}__{b}" in out else f"{b}__{a}"
            r = out.get(k)
            if not r or r["one_way_ms"] is None:
                continue
            ow, fl, sp = r["one_way_ms"], l["floor_ms"], r["sites_on_path"]
            ratio = ow / fl if fl else 0
            verdict = "DIRECT" if sp == 2 else f"MULTIHOP({sp} sites)"
            print(f"{a+'-'+b:14} {ow:>8.3f} {fl:>6.2f} {ratio:>5.2f} {str(sp):>5}  {verdict}")


if __name__ == "__main__":
    main()
