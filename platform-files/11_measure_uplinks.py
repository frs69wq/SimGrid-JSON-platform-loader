#!/usr/bin/env python3
"""
Measure the leaf uplinks FABRIC maintenance blocked (MASS-NEWY, EDUKY-STAR,
STAR-TACC) in ONE self-contained slice, then merge into matrix_fab-fix.json so
05_emit_platform.py upgrades those 3 links from estimated to measured.

Single-shot (no polling): run it once the testbed is OUT of maintenance. It
provisions STAR,NEWY + the 3 leaves together, so every uplink is an IN-SLICE ping
(no cross-slice / anchor needed), ensures FABNet is configured, pings each
directly-connected pair from us_topology, records min-RTT + reply-TTL, and tears
the slice down.

  source ~/fabric-env.sh && source ~/fabric-env/bin/activate
  python3 11_measure_uplinks.py
  python3 05_emit_platform.py && bash build_and_validate.sh   # then regenerate

Env: FAB_SLICE (fab-fix), FAB_SITES (STAR,NEWY,MASS,EDUKY,TACC), FAB_KEEP=1.
"""
import json
import os
import re
import sys

from fabrictestbed_extensions.fablib.fablib import FablibManager

HERE = os.path.dirname(os.path.abspath(__file__))
SLICE = os.environ.get("FAB_SLICE", "fab-fix")
SITES = [s.strip().upper() for s in os.environ.get("FAB_SITES", "STAR,NEWY,MASS,EDUKY,TACC").split(",") if s.strip()]
OUT = os.path.join(HERE, f"matrix_{SLICE}.json")


def ipof(n):
    return str(n.get_interface(network_name=f"FABNET_IPv4_{n.get_site()}").get_ip_addr())


def main():
    topo = json.load(open(os.path.join(HERE, "us_topology.json")))
    edges = [(l["a"], l["b"]) for l in topo["links"]]
    f = FablibManager()

    print(f"[create] {SLICE} sites={SITES} (self-contained, in-slice pings)")
    s = f.new_slice(name=SLICE)
    for site in SITES:
        s.add_node(name=f"n-{site}", site=site, cores=1, ram=2, disk=10, image="default_ubuntu_22").add_fabnet()
    try:
        s.submit()
    except Exception as e:
        if "maintenance" in str(e).lower():
            sys.exit("FABRIC is in maintenance (slice create disabled) — re-run when it clears.")
        raise

    s.update()
    N = {n.get_site(): n for n in s.get_nodes()}
    # ensure FABNet IPs (per-node config tolerates a Failed sibling in the slice)
    for site, n in N.items():
        if n.get_reservation_state() == "Active" and n.get_interface(
                network_name=f"FABNET_IPv4_{site}").get_ip_addr() is None:
            try:
                n.config()
            except Exception as e:
                print(f"  [{site}] config: {e}")
    IP = {}
    for site, n in N.items():
        try:
            if n.get_reservation_state() == "Active":
                IP[site] = ipof(n)
        except Exception:
            pass
    print(f"[ips] {IP}")

    out = json.load(open(OUT)) if os.path.exists(OUT) else {}
    for a, b in edges:
        if a not in IP or b not in IP:
            continue
        res, _ = N[a].execute(
            f'R=$(ping -c 40 -i 0.2 -W 2 {IP[b]} 2>/dev/null); '
            f'echo "$R" | sed -n "s#.*= \\([0-9.]*\\)/.*#MIN=\\1#p"; '
            f'echo "$R" | grep -o "ttl=[0-9]*" | head -1', quiet=True)
        m = re.search(r"MIN=([\d.]+)", res or ""); t = re.search(r"ttl=(\d+)", res or "")
        if not m:
            print(f"  {a}->{b}: PING FAILED"); continue
        ttl = int(t.group(1)) if t else None
        out[f"{a}__{b}"] = {"src": a, "min_rtt_ms": float(m.group(1)), "reply_ttl": ttl,
                            "sites_on_path": (64 - ttl) if ttl else None,
                            "one_way_ms": round(float(m.group(1)) / 2, 4)}
        print(f"  {a}->{b}: one_way={float(m.group(1))/2:.3f}ms ttl={ttl}")
    json.dump(out, open(OUT, "w"), indent=2, sort_keys=True)
    print(f"[wrote] {OUT} ({len(out)} pairs)")

    if not os.environ.get("FAB_KEEP"):
        try:
            s.delete(); print(f"[delete] {SLICE}")
        except Exception as e:
            print(f"[delete] {e}")


if __name__ == "__main__":
    main()
