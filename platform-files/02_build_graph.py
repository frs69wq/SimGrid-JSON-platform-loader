#!/usr/bin/env python3
"""
STEP 2 — Derive the US inter-site GRAPH from the raw orchestrator dump.

Reads raw_sites.json + raw_links.json (from 01), then:
  * keeps only US sites  (-170 < longitude < -60  ->  continental US + Hawaii)
  * keeps only inter-site links whose BOTH endpoints are US sites
  * de-duplicates parallel links between the same pair (keep max nominal bw)
  * computes great-circle km per link  ->  a PHYSICAL FLOOR for one-way latency
       one_way_fiber_ms  = km / 204.19    (light in fiber ~ 2/3 c = 204,190 km/s)
    (real fiber wanders, so measured ping >= this floor; a ping below it is a bug)
  * classifies each site by DEGREE (ring PoP vs edge/leaf)

Writes us_topology.json = { sites:{name:{lat,lon,addr}}, links:[{a,b,gbps,layer,km,floor_ms}] }
which is the single source the ping campaign (03) and the platform emitter (05) consume.
"""
import json
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))


def haversine_km(a, b):
    R = 6371.0
    la1, lo1, la2, lo2 = map(math.radians, [a[0], a[1], b[0], b[1]])
    d = math.sin((la2 - la1) / 2) ** 2 + math.cos(la1) * math.cos(la2) * math.sin((lo2 - lo1) / 2) ** 2
    return 2 * R * math.asin(math.sqrt(d))


def is_us(loc):
    if not loc or len(loc) != 2:
        return False
    lat, lon = loc
    return (-170 < lon < -60) and (15 < lat < 72)


def main():
    sites = json.load(open(os.path.join(HERE, "raw_sites.json")))
    links = json.load(open(os.path.join(HERE, "raw_links.json")))

    us = {}
    for s in sites:
        if is_us(s.get("location")):
            us[s["name"]] = {
                "lat": s["location"][0],
                "lon": s["location"][1],
                "addr": s.get("address", ""),
                "cores_cap": s.get("cores_capacity"),
                "cores_avail": s.get("cores_available"),
            }
    print(f"US sites: {len(us)}")
    print("  " + ", ".join(sorted(us)))
    nonus = sorted({s["name"] for s in sites} - set(us))
    print(f"Excluded (non-US): {nonus}")

    # collapse parallel links, keep max bw + remember layer
    best = {}
    for l in links:
        pair = l.get("sites") or []
        if len(pair) != 2:
            continue
        a, b = pair
        if a not in us or b not in us:
            continue
        key = tuple(sorted((a, b)))
        bw = l.get("bandwidth") or 0
        if key not in best or bw > best[key]["gbps"]:
            best[key] = {"gbps": bw, "layer": l.get("layer"),
                         "avail": l.get("available_bandwidth")}

    graph_links = []
    deg = {k: 0 for k in us}
    for (a, b), meta in sorted(best.items(), key=lambda kv: -kv[1]["gbps"]):
        km = haversine_km((us[a]["lat"], us[a]["lon"]), (us[b]["lat"], us[b]["lon"]))
        floor_ms = km / 204.19
        graph_links.append({"a": a, "b": b, "gbps": meta["gbps"], "layer": meta["layer"],
                            "avail": meta["avail"], "km": round(km, 1),
                            "floor_ms": round(floor_ms, 3)})
        deg[a] += 1
        deg[b] += 1

    print(f"\nUS inter-site links: {len(graph_links)}")
    print(f"{'A':6} {'B':6} {'Gbps':>5} {'layer':5} {'km':>7} {'floor_ms':>9}")
    print("-" * 46)
    for l in graph_links:
        print(f"{l['a']:6} {l['b']:6} {l['gbps']:>5} {l['layer'] or '?':5} {l['km']:>7.1f} {l['floor_ms']:>9.3f}")

    print("\nDegree (candidate ring PoPs have high degree):")
    for name in sorted(deg, key=lambda n: -deg[n]):
        tag = "  <-- hub/PoP" if deg[name] >= 3 else ("  (leaf)" if deg[name] == 1 else "")
        print(f"  {name:6} deg={deg[name]}{tag}")

    isolated = [n for n in us if deg[n] == 0]
    if isolated:
        print(f"\n!! isolated US sites (no US link): {isolated}")

    out = {"sites": us, "links": graph_links, "degree": deg}
    json.dump(out, open(os.path.join(HERE, "us_topology.json"), "w"), indent=2)
    print(f"\n[wrote] us_topology.json  ({len(us)} sites, {len(graph_links)} links)")


if __name__ == "__main__":
    main()
