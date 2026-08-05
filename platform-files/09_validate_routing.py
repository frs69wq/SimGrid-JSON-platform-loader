#!/usr/bin/env python3
"""
STEP 9 — Prove the emitted platform routes like FABNet: compare SimGrid's
all-pairs latency (allpairs.csv, written by ./validate) against the MEASURED
FABNet matrix. Two independent checks per measured pair:
   * one-way latency:  SimGrid vs measured min-RTT/2
   * hop count:        SimGrid #links vs (64 - reply_ttl - 1) sites-1

    ./build_and_validate.sh          # produces allpairs.csv
    python3 09_validate_routing.py
"""
import csv
import glob
import json
import os
import statistics

HERE = os.path.dirname(os.path.abspath(__file__))


def main():
    sg = {}
    with open(os.path.join(HERE, "allpairs.csv")) as fh:
        for r in csv.DictReader(fh):
            sg[(r["src"], r["dst"])] = (float(r["one_way_ms"]), int(r["nlinks"]))

    mx = {}
    for p in sorted(glob.glob(os.path.join(HERE, "matrix_*.json"))):
        mx.update(json.load(open(p)))

    rows = []
    for k, r in mx.items():
        if r.get("one_way_ms") is None:
            continue
        a, b = k.split("__")
        if (a, b) not in sg:
            continue
        sglat, sgh = sg[(a, b)]
        meas = r["one_way_ms"]
        meas_h = (r["sites_on_path"] - 1) if r.get("sites_on_path") else None
        rows.append((abs(sglat - meas), a, b, meas, sglat, meas_h, sgh))

    rows.sort(reverse=True)
    print(f"{'pair':13} {'measured':>9} {'simgrid':>9} {'d_ms':>6} {'m_hop':>5} {'sg_hop':>6}")
    for d, a, b, meas, sglat, mh, sh in rows[:14]:
        flag = "  <-- lat" if d > 1.5 else ""
        hflag = "  <-- HOP" if (mh is not None and mh != sh) else ""
        print(f"{a+'-'+b:13} {meas:>9.3f} {sglat:>9.3f} {d:>6.2f} {str(mh):>5} {sh:>6}{flag}{hflag}")

    errs = [r[0] for r in rows]
    hop_ok = sum(1 for r in rows if r[5] is not None and r[5] == r[6])
    hop_tot = sum(1 for r in rows if r[5] is not None)
    print(f"\nmeasured pairs cross-checked: {len(rows)}")
    print(f"latency abs err : mean={statistics.mean(errs):.3f}  median={statistics.median(errs):.3f}  max={max(errs):.3f} ms")
    print(f"within 1.5 ms   : {sum(1 for e in errs if e<=1.5)}/{len(errs)} ({100*sum(1 for e in errs if e<=1.5)/len(errs):.0f}%)")
    print(f"hop-count match : {hop_ok}/{hop_tot} ({100*hop_ok/hop_tot:.0f}%)  <- same path length as FABNet")


if __name__ == "__main__":
    main()
