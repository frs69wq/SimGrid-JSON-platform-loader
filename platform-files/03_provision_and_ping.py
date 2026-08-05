#!/usr/bin/env python3
"""
STEP 3 — Provision a batch of FABRIC sites and PING every directly-connected pair.

One 1-core VM per site, each on FABNetv4 (routed L3). FABNetv4 is testbed-wide,
so a node at site A can ping the FABNet IP of a node at site B and the RTT rides
the REAL FABRIC backbone path A->B. For a DIRECTLY-connected pair that path is the
single physical link, so:

        one_way_latency  ~=  min(RTT) / 2         (min => propagation, not queuing)

We only ping pairs that are edges in us_topology.json (the live orchestrator graph),
so every number is the latency of a real FABRIC link. Results append/merge into
ping_results.json keyed by the sorted pair, so batches accumulate.

    source ~/fabric-env.sh && source ~/fabric-env/bin/activate
    FAB_SLICE=fab-backbone FAB_SITES=STAR,NEWY,WASH,ATLA,DALL,LOSA,SALT,KANS,SEAT \
        python3 03_provision_and_ping.py all         # create + wait + ping + save (keeps slice)
    python3 03_provision_and_ping.py ping            # (re)ping the live slice
    python3 03_provision_and_ping.py delete          # tear the slice down

Env: FAB_SLICE (slice name), FAB_SITES (comma list), FAB_CORES/RAM/DISK (1/2/10),
     PING_COUNT (default 40), FAB_KEEP=1 to keep slice after 'all'.
"""
import json
import os
import re
import sys
import time

from fabrictestbed_extensions.fablib.fablib import FablibManager

HERE = os.path.dirname(os.path.abspath(__file__))
SLICE = os.environ.get("FAB_SLICE", "fab-usnet")
SITES = [s.strip().upper() for s in os.environ.get("FAB_SITES", "").split(",") if s.strip()]
CORES = int(os.environ.get("FAB_CORES", "1"))
RAM = int(os.environ.get("FAB_RAM", "2"))
DISK = int(os.environ.get("FAB_DISK", "10"))
IMAGE = "default_ubuntu_22"
PING_COUNT = int(os.environ.get("PING_COUNT", "40"))
RESULTS = os.path.join(HERE, "ping_results.json")


def fm():
    return FablibManager()


def topo():
    return json.load(open(os.path.join(HERE, "us_topology.json")))


def node_ip(node):
    return node.get_interface(network_name=f"FABNET_IPv4_{node.get_site()}").get_ip_addr()


def load_results():
    if os.path.exists(RESULTS):
        return json.load(open(RESULTS))
    return {}


def save_results(r):
    json.dump(r, open(RESULTS, "w"), indent=2, sort_keys=True)


# ----------------------------------------------------------------- create
def cmd_create():
    f = fm()
    if not SITES:
        sys.exit("set FAB_SITES=STAR,NEWY,...")
    print(f"[create] slice={SLICE} sites={SITES} flavor={CORES}c/{RAM}G/{DISK}G")
    s = f.new_slice(name=SLICE)
    for site in SITES:
        n = s.add_node(name=f"n-{site}", site=site, cores=CORES, ram=RAM, disk=DISK, image=IMAGE)
        n.add_fabnet()
    print("[create] submitting (5-20 min for a multi-site slice) ...")
    s.submit()
    print("[create] slice is Active")
    cmd_ips()


def cmd_ips():
    f = fm()
    s = f.get_slice(SLICE)
    print(f"[ips] slice={SLICE} state={s.get_state()}")
    for n in s.get_nodes():
        try:
            print(f"   {n.get_site():7} {n.get_name():10} {n.get_reservation_state():12} ip={node_ip(n)}")
        except Exception as e:
            print(f"   {n.get_site():7} {n.get_name():10} ERR {e}")


# ----------------------------------------------------------------- ping
def parse_rtt(out):
    """Return (min,avg,max,mdev) ms from a Linux `ping` summary, or None."""
    m = re.search(r"= ([\d.]+)/([\d.]+)/([\d.]+)/([\d.]+) ms", out)
    if m:
        return tuple(float(x) for x in m.groups())
    return None


def parse_loss(out):
    m = re.search(r"(\d+)% packet loss", out)
    return int(m.group(1)) if m else None


def cmd_ping():
    f = fm()
    s = f.get_slice(SLICE)
    t = topo()
    # sites actually present + Active in this slice, with an IP
    present = {}
    for n in s.get_nodes():
        try:
            ip = node_ip(n)
            if ip:
                present[n.get_site()] = (n, str(ip))
        except Exception:
            pass
    print(f"[ping] live sites in slice: {sorted(present)}")

    # edges from the graph with BOTH endpoints present
    pairs = []
    for l in t["links"]:
        a, b = l["a"], l["b"]
        if a in present and b in present:
            pairs.append((a, b, l))
    print(f"[ping] measurable directly-connected pairs in this batch: {len(pairs)}")

    results = load_results()
    for a, b, l in pairs:
        na, ipa = present[a]
        nb, ipb = present[b]
        # ping b from a
        cmd = f"ping -c {PING_COUNT} -i 0.2 -W 2 {ipb}"
        out, err = na.execute(cmd, quiet=True)
        rtt = parse_rtt(out or "")
        loss = parse_loss(out or "")
        key = "__".join(sorted((a, b)))
        if rtt:
            rmin, ravg, rmax, rmdev = rtt
            rec = {
                "a": a, "b": b, "src": a, "dst": b, "dst_ip": ipb,
                "gbps": l["gbps"], "layer": l["layer"], "km": l["km"], "floor_ms": l["floor_ms"],
                "rtt_min_ms": rmin, "rtt_avg_ms": ravg, "rtt_max_ms": rmax, "rtt_mdev_ms": rmdev,
                "one_way_ms": round(rmin / 2.0, 4), "loss_pct": loss, "count": PING_COUNT,
                "slice": SLICE,
            }
            results[key] = rec
            print(f"   {a:6}<->{b:6} {l['gbps']:>4}G  min/avg={rmin:.3f}/{ravg:.3f} ms "
                  f"loss={loss}%  -> one_way={rec['one_way_ms']:.3f} ms  (floor {l['floor_ms']:.3f})")
            save_results(results)  # save incrementally
        else:
            print(f"   {a:6}<->{b:6} PING FAILED loss={loss} out={ (out or '')[-120:]!r}")
    save_results(results)
    print(f"[ping] saved -> {RESULTS}  ({len(results)} links measured so far)")


def cmd_trace():
    """Best-effort traceroute for a couple of pairs to prove the path is the direct link."""
    f = fm()
    s = f.get_slice(SLICE)
    t = topo()
    present = {}
    for n in s.get_nodes():
        try:
            present[n.get_site()] = (n, node_ip(n))
        except Exception:
            pass
    # install traceroute best-effort on one node, trace each connected pair from it
    for a, b, l in [(x["a"], x["b"], x) for x in t["links"] if x["a"] in present and x["b"] in present]:
        na, ipa = present[a]
        nb, ipb = present[b]
        na.execute("which traceroute || sudo apt-get -y -q install traceroute >/dev/null 2>&1", quiet=True)
        out, _ = na.execute(f"traceroute -n -w 2 -q 1 {ipb} 2>/dev/null | head -8", quiet=True)
        print(f"--- traceroute {a} -> {b} ({ipb}) ---\n{out}")


def cmd_delete():
    f = fm()
    try:
        s = f.get_slice(SLICE)
        s.delete()
        print(f"[delete] {SLICE} deleted")
    except Exception as e:
        print(f"[delete] {e}")


def cmd_all():
    cmd_create()
    time.sleep(5)
    cmd_ping()
    if not os.environ.get("FAB_KEEP"):
        cmd_delete()
    else:
        print("[all] FAB_KEEP set -> leaving slice up")


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "ips"
    {"create": cmd_create, "ips": cmd_ips, "ping": cmd_ping, "trace": cmd_trace,
     "delete": cmd_delete, "all": cmd_all}.get(cmd, lambda: sys.exit(f"unknown cmd {cmd}"))()
