#!/usr/bin/env python3
"""
STEP 1 — Discover the LIVE FABRIC topology from the orchestrator.

Captures, to JSON, the ground truth we build the SimGrid platform from:
  * every site         (name, location lat/long, state/country, host counts)
  * every inter-site link (endpoints, nominal capacity Gbps, layer, free bw)

FABRIC's topology is reservable/live, so the ONLY truthful source is the API at
query time (fablib.list_sites / list_links). Everything downstream (US filter,
adjacency graph, which pairs to ping, the platform .cpp) is derived from the two
JSON files this writes:  raw_sites.json  raw_links.json

    source ~/fabric-env.sh && source ~/fabric-env/bin/activate
    python3 01_discover_topology.py
"""
import json
import os
import sys

OUT = os.path.dirname(os.path.abspath(__file__))


def dump(obj, name):
    path = os.path.join(OUT, name)
    with open(path, "w") as fh:
        json.dump(obj, fh, indent=2, default=str)
    print(f"[wrote] {path}  ({len(obj)} records)")


def main():
    from fabrictestbed_extensions.fablib.fablib import FablibManager

    f = FablibManager()

    # ---- sites -----------------------------------------------------------
    sites = f.list_sites(output="list", quiet=True)
    print(f"[sites] {len(sites)} total")
    if sites:
        print("[sites] fields:", sorted(sites[0].keys()))
    dump(sites, "raw_sites.json")

    # ---- links -----------------------------------------------------------
    links = f.list_links(output="list", quiet=True)
    print(f"[links] {len(links)} total")
    if links:
        print("[links] fields:", sorted(links[0].keys()))
    dump(links, "raw_links.json")


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print("ERROR:", e, file=sys.stderr)
        raise
