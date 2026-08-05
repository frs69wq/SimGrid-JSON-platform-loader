# FABRIC US inter-site network → SimGrid C++ platform

A SimGrid S4U **C++ platform file** (`fabric_us.cpp`) that models the **FABRIC
testbed's US wide-area network**: every US site, the inter-site links at their
nominal bandwidth, and per-link **one-way latency measured by ping** between
directly-connected FABRIC VMs — **routed the way FABNetv4 actually routes**, not
the way the physical link map suggests.

```
   DELIVERABLE:  fabric_us.cpp  ->  g++ -shared -fPIC -std=c++17 -o libfabric_us.so ... -lsimgrid
                 loaded by the NRTWsim simulator exactly like platforms/libplatform.so
```

WHAT IT CONTAINS
════════════════════════════════════════════════════════════════════════════
```
 28 US FABRIC sites (incl. Hawaii)   each = a StarZone/ClusterZone, ONE host, 1 core
                                      (compute is deliberately trivial — the network
                                       is the subject of this study)
 links                               FABNet-direct inter-site links only, at nominal
                                      FABRIC bandwidth (1200G ring / 100G / 40G / 10G),
                                      latency = measured ping one-way (min-RTT / 2)
 routing = Full                      an explicit route for every site pair, each path
                                      RECONSTRUCTED from the measurement (not guessed
                                      by a shortest-path metric) so it matches FABNet
```

Each site is a `ClusterZone` named by its FABRIC code (`STAR`, `WASH`, `HAWI`, …)
so the simulator's `get_host_list(cluster, node_range)` (`simulator/src/utils.cpp`)
places workflow components on it unchanged.

THE TOPOLOGY (live orchestrator, `fablib.list_links`, 2026-08-02)
════════════════════════════════════════════════════════════════════════════
```
 28 US sites, 33 physical inter-site links. The 7 PoPs form a CLOSED ring of
 1.2-Tbps segments (ring edges labeled with the ping one-way ms); KANS is a 100 G
 mid-continent aggregator (a chord, NOT on the ring); every other site is a leaf
 hung off one PoP. EVERY line below is a real FABNet-direct link in fabric_us.cpp.

               MICH   INDI   NCSA—EDC   EDUKY   TACC
                  \     \     |     /     /
                   `------- STAR -------'
                        15.4 / | \ 10.9              ring segment = 1200 G, L1
                            /  |  \                  (label = measured one-way ms)
                           /(KANS)  \
    UTAH ———— SALT ———————  100 G agg  ——————— NEWY ———— MASS
                │          chords into ring:      │  \——— PRIN
            6.1 │          STAR 5.5  DALL 4.5      │ 2.5 \ RUTG
                │          SALT 9.9  (+leaf GPN)   │
 UCSD ——— LOSA —              GPN 0.14           WASH ———— MAX
 SRI  ———/ │                                       │  \——— PSC
 SEAT ———/ │ 17.5                             6.4  │   \—— CLEM
 HAWI ——/  │                                       │    \_ GATECH
  (40 G)  DALL ————————————————————————————————— ATLA ———— FIU
                              8.3

  HAWI (Honolulu, 40 G) is dual-homed: HAWI—LOSA 24.6 ms and HAWI—SEAT 36.5 ms;
  SEAT is drawn off LOSA (12.3 ms) — in raw list_links SEAT is degree-3 (LOSA 100G,
  SALT 100G, HAWI 40G), but the SALT—SEAT chord is one of the 3 DROPPED below, so
  FABNet reaches SEAT via LOSA. LOSA is also HAWI's faster path.
  (Every edge above was re-derived from list_links: `python3 10_report_tables.py links`.)

  DROPPED — 3 physical chords that EXIST in list_links but FABNet L3 does NOT use
  (it routes them over the ring instead; see next section):
       STAR—WASH (→ via NEWY)   ATLA—STAR (→ via WASH-NEWY)   SALT—SEAT (→ via LOSA)
```
Per-link measured latencies for every edge: `python3 10_report_tables.py links`.

THE KEY FINDING — physical link map ≠ FABNet routing
════════════════════════════════════════════════════════════════════════════
The orchestrator's `list_links` reports the **L1 physical** links. But
**FABNetv4 (the routed L3 IP dataplane the experiments actually use) does NOT
forward over some of those physical links** — it takes the 1.2 T ring instead.
Modeling the physical map verbatim would put a fast direct hop where FABNet
really takes a slow 2-3 hop detour. Proven with **two independent instruments**
that agree:

```
 instrument 1: latency      one-way = min(RTT)/2
 instrument 2: reply TTL     #sites on path = 64 - ttl  (each site's FABNet
                             gateway decrements TTL once; Linux starts at 64)

 physical link   one-way   ttl  #sites   FABNet actually routes …        verdict
 ─────────────   ───────   ───  ──────   ───────────────────────────    ───────────
 NEWY-WASH        2.54 ms   62     2      NEWY─WASH                       uses link ✓
 DALL-LOSA       17.49 ms   62     2      DALL─LOSA                       uses link ✓
 KANS-STAR        5.50 ms   62     2      KANS─STAR                       uses link ✓
 STAR-WASH       13.44 ms   61     3      STAR─NEWY─WASH  (via ring)      IGNORES link ✗
 SALT-SEAT       18.37 ms   61     3      SALT─LOSA─SEAT  (via ring)      IGNORES link ✗
 ATLA-STAR       19.78 ms   60     4      ATLA─WASH─NEWY─STAR (via ring)  IGNORES link ✗
```

`19.78 ≈ ATLA-WASH(6.37)+WASH-NEWY(2.54)+NEWY-STAR(10.94)=19.84`, and the TTL says
"4 sites" — both instruments independently say the same detour. So the platform
**drops those 3 chords** and routes those pairs over the ring, exactly like FABNet.

Why it matters for the simulator: routing `ATLA→STAR` over the (dropped) 100 G
chord vs the real 1200 G ring is a **12× bandwidth error**, not just a latency one.

METHOD — provision → ping → classify → reconstruct → emit → validate
════════════════════════════════════════════════════════════════════════════
```
 01_discover_topology.py   fablib list_sites/list_links     -> raw_sites/links.json
 02_build_graph.py         US filter + great-circle floors  -> us_topology.json
 03_provision_and_ping.py  provision a batch of 1-core VMs on FABNetv4, ping every
                           directly-connected pair (min-RTT/2 = one-way)
 04_matrix_probe.py        full pairwise min-RTT + reply-TTL -> matrix_<slice>.json
 08_measure_leaves.py      cross-slice ping leaf→PoP (backbone kept up as anchor)
 06_analyze_routing.py     does shortest-path over the direct graph reproduce FABNet?
 07_reconstruct_paths.py   recover FABNet's real path per pair from (latency, ttl);
                           show no fixed metric (min-hop/lat/cap) reproduces it
 05_emit_platform.py       -> fabric_us.cpp   (drops ignored chords; explicit routes)
 validate.cpp / build_and_validate.sh   load .so, prove ClusterZones + all-pairs route
 09_validate_routing.py    SimGrid all-pairs latency vs MEASURED FABNet matrix
 10_report_tables.py       links | audit | core-routes  (this report's tables/appendices)
 11_measure_uplinks.py     one-shot re-measure of the 3 maintenance-blocked leaf uplinks
```

Provisioning is real FABRIC (per `~/.claude` policy: use the token first). One
1-core/2 GB VM per site, each with `add_fabnet()` (FABNetv4, routed L3). Latency =
`min(RTT)/2` over ≥30 ICMP packets (min ⇒ propagation, not queuing). Batches:

```
 slice          sites                                    role
 fab-backbone   STAR NEWY WASH ATLA DALL LOSA SALT KANS SEAT   the 9-site routing CORE
                (all 36 core pairs measured pairwise -> full FABNet core routing map)
 fab-leaves     the 18 edge sites, cross-slice ping to their PoP in fab-backbone
```

ROUTING MODEL — reconstructed from measurement, not guessed by a metric
════════════════════════════════════════════════════════════════════════════
A SimGrid platform is (links) + (a route for every A→B pair). The links are settled
(the FABNet-direct edges above). The hard part is the ROUTES: for a non-adjacent pair
like MICH→WASH, which physical links does the packet actually traverse? Get this wrong
and BOTH the latency AND the bandwidth bottleneck are wrong. We refuse to guess it —
we read it out of the measurements.

STEP A — why we can't just use SimGrid's built-in routing
────────────────────────────────────────────────────────────────────────────
SimGrid's Floyd/Dijkstra zones pick the path with the FEWEST LINKS (their cost is
`link_list_.size()`, FloydZone.cpp:92) — a pure hop count, blind to bandwidth. That
mis-routes FABRIC because the ring (1200 G) and the KANS shortcut (100 G) often have
the SAME hop count, and hop count can't tell them apart:

```
   ATLA ─► STAR : two 3-hop paths exist
        ring   ATLA─WASH─NEWY─STAR   all 1200 G   ← FABNet uses THIS (measured)
        KANS   ATLA─DALL─KANS─STAR   100 G links
   min-hop tie → SimGrid may pick KANS → models a 100 G pipe where FABRIC has 1200 G
                                        = a 12× BANDWIDTH error, plus wrong latency
```

STEP B — why no single metric works either
────────────────────────────────────────────────────────────────────────────
We tested three fixed metrics against the 72 measured core paths (07_reconstruct_paths.py).
NONE reproduces FABNet, because FABNet's own choice is inconsistent — it prefers the
1200 G ring for some pairs and the 100 G KANS shortcut for others:

```
   metric        agrees with measured FABNet path
   min-hop            64/72   (89 %)
   min-latency        61/72   (85 %)
   capacity-tiered    57/72   (79 %)

   the killer counter-pair (opposite choices, so ONE rule can't get both):
     ATLA→STAR  measured 19.78 ms, 4 sites → ATLA─WASH─NEWY─STAR   (the 1200 G ring)
     DALL→STAR  measured 10.00 ms, 3 sites → DALL─KANS─STAR        (the 100 G shortcut)
```

STEP C — reconstruct the path from the TWO things ping already told us
────────────────────────────────────────────────────────────────────────────
Every ping gives two INDEPENDENT measurements of the real path, and together they
usually pin down exactly one path in the FABNet-direct graph:

```
   (1) hop count   #sites on path = 64 − reply_TTL      (exact; kills most candidates)
   (2) latency     one-way        = min(RTT)/2          (picks among the survivors)

   reconstruct(A,B):
     enumerate every simple path A…B in the direct graph with #sites == (64−ttl)
     choose the one whose Σ(measured per-link latency) is closest to the measured one-way
```

Worked example — ATLA→STAR (measured 19.78 ms, reply_TTL 60 ⇒ 64−60 = 4 sites):

```
   candidate 4-site paths          Σ per-link latency        |Σ − 19.78|
   ─────────────────────────       ──────────────────        ───────────
   ATLA-WASH-NEWY-STAR             6.37+2.54+10.94 = 19.85       0.07   ◄── chosen
   ATLA-DALL-KANS-STAR             8.34+4.54+ 5.50 = 18.38       1.40
   → FABNet path = ATLA-WASH-NEWY-STAR  (the 1200 G ring). Bottleneck now correct.

   Contrast DALL→STAR (10.00 ms, TTL 61 ⇒ 3 sites): the ONLY 3-site path is
   DALL-KANS-STAR (4.54+5.50 = 10.04 ≈ 10.00) → the 100 G shortcut. Opposite of ATLA,
   and we got both right — because the DATA, not a rule, chose each one.
```

STEP D — leaf pairs: COMPOSE (a leaf is degree-1, so its path is forced)
────────────────────────────────────────────────────────────────────────────
A leaf L has exactly one uplink to its PoP, so every route in/out of L must start
with that uplink. We build leaf routes by composition, reusing the measured core path:

```
   path(L, D) =  [L—PoP_L uplink]  +  core_path(PoP_L, PoP_D)  +  [PoP_D—D downlink]
                     measured            reconstructed (Step C)        measured

   MICH→WASH =  MICH-STAR (2.55)  +  STAR-NEWY-WASH (core)  =  MICH-STAR-NEWY-WASH
   and the LIVE cross-slice ping MICH→WASH came back ttl=60 (4 sites), 15.99 ms one-way
   — i.e. exactly MICH-STAR-NEWY-WASH. The composition is not assumed, it was OBSERVED.
```
The two awkward west-coast sites — SEAT (two PoPs) and HAWI (dual-homed) — are
handled explicitly, and differently, because the DATA says they are different:

```
                         (ring ⇄ mainland)
                               │
                   LOSA ──12.3── SEAT          SALT─SEAT physical chord is DROPPED
                    │  ╲          │             (measured 3 sites: SEAT⇄SALT really
              24.6  │   ╲        │ 36.5          goes SEAT-LOSA-SALT), so SEAT's only
                    │    ╲______ │               working uplink is LOSA.
                    └───── HAWI ─┘              HAWI has TWO real uplinks (both ttl=62).

 SEAT — "connected to two PoPs" (LOSA 100 G + SALT 100 G) is true in list_links, but
   SALT-SEAT is one of the 3 DROPPED chords, so in the FABNet-effective graph SEAT is
   SINGLE-homed to LOSA. SEAT was measured pairwise (it is a core node), so it is routed
   by RECONSTRUCTION (Step C), never composition — and never over the dropped chord:
        SEAT→SALT = SEAT-LOSA-SALT       (18.37 ms, 3 sites)  ← the ring, not SALT-SEAT
        SEAT→STAR = SEAT-LOSA-SALT-STAR  (33.70 ms, 4 sites)

 HAWI — GENUINELY dual-homed: HAWI-LOSA (24.56 ms) and HAWI-SEAT (36.51 ms) are BOTH
   real direct links (both measured ttl=62). `attach(HAWI)` therefore returns two
   candidate uplinks; composition builds the route through EACH and keeps the lower-
   latency one. LOSA wins for every mainland target (24.56 < 36.51, and SEAT itself
   only exits to the mainland via LOSA), so the SEAT uplink is used ONLY for HAWI⇄SEAT:
        HAWI→SEAT = HAWI-SEAT                    (direct, 36.51 ms)   ← its one use
        HAWI→STAR = HAWI-LOSA-SALT-STAR          (via LOSA)
        HAWI→WASH = HAWI-LOSA-DALL-ATLA-WASH     (via LOSA)

 RAW EVIDENCE — verified against the raw orchestrator dump + raw ping counters, not
 the derived files (reproduce: read raw_links.json / matrix_*.json):

   SEAT's physical wires (raw list_links endpoints, actual switch ports):
     SEAT:HundredGigE0/0/0/22.3000 <-> LOSA:HundredGigE0/0/0/23.3000   100 G  L1
     SEAT:HundredGigE0/0/0/23.3000 <-> SALT:HundredGigE0/0/0/21.3000   100 G  L1  ← 2nd PoP
     SEAT:HundredGigE0/0/0/19.3380 <-> HAWI:HundredGigE0/0/0/23.3380    40 G  L2
     (SEAT also lands TOKYO at 100 G — it is the trans-Pacific hub; TOKY is non-US,
      excluded from this platform)

   raw ping counters (min_RTT and reply_ttl are what the pings actually returned;
   one-way = RTT/2, sites = 64 − ttl are the only arithmetic):
     pair        min_RTT(ms)  reply_ttl   one-way    sites   →  meaning
     LOSA-SEAT     24.557        62         12.28       2       DIRECT (SEAT's real uplink)
     SEAT-SALT     36.736        61         18.37       3       3 gateways ⇒ SEAT-LOSA-SALT,
                                                                so the SALT-SEAT wire is DROPPED
     HAWI-LOSA     49.123        62         24.56       2       DIRECT
     HAWI-SEAT     73.023        62         36.51       2       DIRECT  ⇒ HAWI is dual-homed

   The one datum that settles both cases is the reply TTL: SEAT→SALT comes back with
   ttl 61 (the packet crossed 3 site gateways, not 2), proving FABNet did NOT use the
   direct SALT-SEAT wire; HAWI→LOSA and HAWI→SEAT both come back ttl 62 (2 gateways),
   proving both are genuine single hops.
```

(Also handled: EDC is a leaf-of-a-leaf, EDC→NCSA→STAR; a directly-connected pair is
just its single link; the only fallback, min-hop, is reserved for a core segment we
never measured — none occurred here.)

STEP E — emit
────────────────────────────────────────────────────────────────────────────
```
   root = add_netzone_full("fabric_us")     Full routing = one EXPLICIT route per pair,
   378 site pairs → 378 add_route(A, B, {ordered link list})
   Deterministic: SimGrid stores our link list verbatim and never re-picks — so the
   simulated path is byte-for-byte the reconstructed FABNet path (no hidden tie-break).
```

WHAT THIS BUYS (and its one honest limit)
────────────────────────────────────────────────────────────────────────────
```
   ✓ latency:   93 % of measured pairs within 1.5 ms (median 0.027 ms)   [next section]
   ✓ hop count: 97 % identical to FABNet
   ✓ bandwidth: each pair's bottleneck link = the one FABNet really crosses (1200 vs 100 G)
   ✗ asymmetry: 6 core site-pairs route differently each way (Appendix B) — 3 latency-
     asymmetric (the ≤4.5 ms Δ) + 3 hop-asymmetric (equal latency). A symmetric model
     picks one path per pair; the ≤4.5 ms residual is the only cost.
```

RESULTS — measured link latencies (ping, one-way = min-RTT/2)
════════════════════════════════════════════════════════════════════════════
```
 31 modeled FABNet-direct links: 27 ping-measured, 3 estimated (leaf uplinks that
 FABRIC failed to provision/route: MASS-NEWY, EDUKY-STAR, STAR-TACC), 1 co-located
 (EDC-NCSA). 3 physical chords DROPPED (FABNet routes them over the ring).

 CORE RING (1200G, L1)              one-way ms   great-circle floor ms
   NEWY-WASH                            2.54          1.67
   ATLA-WASH                            6.37          4.22
   LOSA-SALT                            6.12          4.56
   ATLA-DALL                            8.34          5.68
   NEWY-STAR                           10.94          5.83
   SALT-STAR                           15.37          9.71
   DALL-LOSA                           17.49          9.75
 KANS aggregator (100G)   DALL-KANS 4.54  KANS-STAR 5.50  KANS-SALT 9.93  GPN-KANS 0.14
 SEAT / west (100G)       LOSA-SEAT 12.28  LOSA-UCSD 1.15  SALT-UTAH 0.11
 Hawaii (40G)             HAWI-LOSA 24.56  HAWI-SEAT 36.51   (LOSA is HAWI's faster uplink)
 east leaves (100/40/10G) MICH-STAR 2.55  INDI-STAR 2.31  NCSA-STAR 1.23  NEWY-RUTG 0.51
                          NEWY-PRIN 0.67  FIU-ATLA 6.28  MAX-WASH 0.87  PSC-WASH 3.07
                          CLEM-WASH 8.18  GATECH-WASH 7.00  SRI-LOSA 4.07
```
(Full machine-readable table: `python3 10_report_tables.py links`. Latency runs 1.1–2.4×
the great-circle floor on the long hops (higher on short metro links, where fixed
switching/serialization dominates over propagation) — always ABOVE the floor; a ping
below it would be a bug, and none is.)

RESULTS — routing fidelity (does the platform route like FABNet?)
════════════════════════════════════════════════════════════════════════════
`09_validate_routing.py` compares SimGrid's computed all-pairs latency (from
`./validate`) against the **measured** FABNet matrix, on the 88 pairs actually
pinged:
```
                                 platform vs measured FABNet
   latency abs error   mean 0.30 ms   median 0.027 ms   max 4.54 ms
   within 1.5 ms       82 / 88  (93 %)
   hop-count match     85 / 88  (97 %)   <- same path length as FABNet took
   all-pairs routable  756 / 756         28 ClusterZones, all 1-host/1-core
```
The 6 ordered pairs >1.5 ms are 3 site-pairs (ATLA↔SALT, DALL↔SALT, LOSA↔KANS) with
**proven latency-asymmetric routing** (the first of the two kinds in Appendix B), not
model error:
```
   DALL<->SALT measured 19.01 ms  =  (DALL-KANS-SALT 14.47 + DALL-LOSA-SALT 23.60)/2
   forward and reverse take DIFFERENT paths; RTT/2 averages them. A static,
   symmetric per-link-latency platform models one direction (the faster). This is
   an inherent limit of any link-latency model, and is documented, not hidden.
```



HOW TO REPLICATE
════════════════════════════════════════════════════════════════════════════
```bash
source ~/fabric-env.sh && source ~/fabric-env/bin/activate
cd studies/fabric/platform-files

python3 01_discover_topology.py            # live sites + links -> raw_*.json
python3 02_build_graph.py                  # -> us_topology.json (28 sites, 33 links)

FAB_SLICE=fab-backbone \
 FAB_SITES=STAR,NEWY,WASH,ATLA,DALL,LOSA,SALT,KANS,SEAT \
 python3 03_provision_and_ping.py create   # provision the core (keep it up)
FAB_SLICE=fab-backbone python3 04_matrix_probe.py     # core matrix (latency+TTL)

FAB_SLICE=fab-leaves \
 FAB_SITES=MICH,INDI,NCSA,EDUKY,TACC,MAX,PSC,CLEM,GATECH,MASS,PRIN,RUTG,FIU,UTAH,UCSD,SRI,GPN,HAWI \
 python3 03_provision_and_ping.py create
FAB_ANCHOR=fab-backbone FAB_SLICE=fab-leaves python3 08_measure_leaves.py

python3 05_emit_platform.py                # -> fabric_us.cpp
bash build_and_validate.sh                 # compile .so + structural proof + allpairs.csv
python3 09_validate_routing.py             # SimGrid latency vs measured FABNet

FAB_SLICE=fab-backbone python3 03_provision_and_ping.py delete   # tear down
FAB_SLICE=fab-leaves   python3 03_provision_and_ping.py delete
```

FILES
════════════════════════════════════════════════════════════════════════════
```
 fabric_us.cpp            ★ the deliverable — the C++ platform
 validate.cpp             loads + proves the platform (ClusterZones, all-pairs, CSV)
 build_and_validate.sh    compile both + run validate
 01..09_*.py              the pipeline (discover→graph→provision/ping→matrix→
                          analyze→reconstruct→emit→validate)
 10_report_tables.py      links | audit | core-routes tables (this report's appendices)
 11_measure_uplinks.py    one-shot re-measure of the 3 maintenance-blocked leaf uplinks
 us_topology.json         28 US sites + 33 physical links (+ great-circle floors)
 raw_sites/links.json     the live orchestrator dump
 matrix_*.json            measured min-RTT + reply-TTL per pair
 ping_results.json        high-count adjacent-pair latencies
 allpairs.csv             SimGrid's computed all-pairs latency (from validate)
 REPORT.md                this file
```

LIMITATIONS / CAVEATS (stated, not hidden)
════════════════════════════════════════════════════════════════════════════
```
 * bandwidth is NOMINAL (list_links), not measured — as requested. It is the
   FABRIC link rate (1200/100/40/10 G), the right ceiling for a simulator; actual
   throughput is lower (VM vCPU + shared NIC bound), see the dataman/sst studies.
 * 3 of 31 links are ESTIMATED (floor×1.45), not pinged: MASS-NEWY, EDUKY-STAR,
   STAR-TACC. Root cause: FABRIC entered a **testbed-wide maintenance window**
   (orchestrator "Create, Modify and Renew Slice(s) are disabled") — the same
   reason those leaf VMs failed to boot/route mid-run and a retry could not
   re-provision. NOT a method gap and NOT a permanent site outage. All 3 are
   degree-1 leaf uplinks (affect only that leaf's own traffic). Re-measure when
   maintenance lifts: `python3 11_measure_uplinks.py` (one self-contained slice), then re-run
   05_emit_platform.py — or the manual commands in the resume section.
 * asymmetric routing: 6 core site-pairs route differently each way (Appendix B: 3
   latency-asymmetric, ≤4.5 ms Δ; 3 hop-asymmetric, equal latency). A static symmetric
   model picks one path per pair — the ≤4.5 ms residual is the only cost.
 * compute is deliberately trivial (1 host, 1 core, 1Gf, no disk) — the network is
   the subject. Add disks/cores per site later if a workflow needs them.
 * only the FABNet L3 dataplane was measured (what ADIOS/DataMan/SST use). A
   dedicated-NIC L2 circuit would pin traffic to the physical link and could differ.
```

APPENDIX A — raw-counter audit of all 33 physical links (auditable)
════════════════════════════════════════════════════════════════════════════
Every physical `list_links` edge, with the RAW evidence and the only arithmetic:
bw/layer are raw from `list_links`; `minRTT` (round-trip) and `ttl` (reply TTL) are
what ping actually returned; `1-way = minRTT/2`; `sit = 64 − ttl`; `floor` = great-
circle km / 204.19 (light in fiber). Verdict: **DIRECT** (ttl 62 ⇒ modeled as a
direct link) · **DROPPED/Nh** (ttl<62 ⇒ FABNet routes around the physical chord over
N sites) · **EST** (no ping — FABRIC maintenance). Regenerate: `python3 10_report_tables.py audit`.

```
A      B      Gbps lyr  floor  minRTT ttl  1-way sit  verdict
------------------------------------------------------------------
ATLA   DALL   1200 L1    5.68   16.68  62   8.34   2  DIRECT
ATLA   WASH   1200 L1    4.22   12.74  62   6.37   2  DIRECT
DALL   LOSA   1200 L1    9.75   34.99  62  17.49   2  DIRECT
LOSA   SALT   1200 L1    4.56   12.24  62   6.12   2  DIRECT
NEWY   STAR   1200 L1    5.83   21.88  62  10.94   2  DIRECT
NEWY   WASH   1200 L1    1.67    5.08  62   2.54   2  DIRECT
SALT   STAR   1200 L1    9.71   30.75  62  15.37   2  DIRECT
ATLA   FIU     100 L1    4.75   12.56  62   6.28   2  DIRECT
ATLA   STAR    100 L1    4.89   39.55  60  19.78   4  DROPPED/4h
DALL   KANS    100 L1    3.57    9.08  62   4.54   2  DIRECT
GPN    KANS    100 L1    0.04    0.28  62   0.14   2  DIRECT
INDI   STAR    100 L1    1.57    4.63  62   2.31   2  DIRECT
KANS   SALT    100 L1    7.30   19.86  62   9.93   2  DIRECT
KANS   STAR    100 L1    3.15   11.00  62   5.50   2  DIRECT
LOSA   SEAT    100 L1    7.57   24.56  62  12.28   2  DIRECT
LOSA   UCSD    100 L1    0.78    2.30  62   1.15   2  DIRECT
MASS   NEWY    100 L2    0.98       —   —   1.42   —  EST(maint)
MICH   STAR    100 L1    1.79    5.10  62   2.55   2  DIRECT
NCSA   STAR    100 L1    1.17    2.47  62   1.23   2  DIRECT
NEWY   PRIN    100 L1    0.33    1.33  62   0.67   2  DIRECT
NEWY   RUTG    100 L1    0.22    1.02  62   0.51   2  DIRECT
SALT   SEAT    100 L1    5.50   36.74  61  18.37   3  DROPPED/3h
SALT   UTAH    100 L1    0.03    0.21  62   0.11   2  DIRECT
STAR   WASH    100 L1    4.87   26.88  61  13.44   3  DROPPED/3h
CLEM   WASH     40 L2    3.40   16.37  62   8.18   2  DIRECT
HAWI   LOSA     40 L2   20.15   49.12  62  24.56   2  DIRECT
HAWI   SEAT     40 L2   21.11   73.02  62  36.51   2  DIRECT
MAX    WASH     40 L2    0.12    1.74  62   0.87   2  DIRECT
PSC    WASH     40 L2    1.35    6.13  62   3.07   2  DIRECT
EDUKY  STAR     10 L2    2.75       —   —   3.98   —  EST(maint)
GATECH WASH     10 L2    4.21   13.99  62   7.00   2  DIRECT
LOSA   SRI      10 L2    2.54    8.14  62   4.07   2  DIRECT
STAR   TACC     10 L2    7.68       —   —  11.14   —  EST(maint)
------------------------------------------------------------------
33 physical links: 27 DIRECT (modeled), 3 DROPPED (FABNet routes around), 3 EST.
conservation check (one-way >= great-circle floor): ALL PASS
+ EDC-NCSA: co-located (same lat/lon), no list_links edge -> nominal 100G/0.05ms, not pinged.
```
Reading it: the 3 DROPPED rows are the only ones with ttl≠62 — their packets crossed
3–4 site gateways, i.e. FABNet took the ring, not the physical chord. Everything else
is ttl=62 (a genuine single hop) and becomes a direct link in `fabric_us.cpp`. All 30
measured one-way values sit above their great-circle floor (no unphysically-fast pings).

APPENDIX B — core routing: how the 9 backbone sites reach each other (validated)
════════════════════════════════════════════════════════════════════════════
The path `fabric_us.cpp` routes between every pair of the 9 core PoPs, with TWO
independent checks that it is the REAL measured FABNet path, not a guess:
  - Σpath vs meas (Δ): the path's summed link-latencies vs the measured one-way;
    Δ≈0 means the reconstructed path reproduces the latency measurement.
  - ttl vs hop: sites-on-path counted by the reply TTL (an INDEPENDENT instrument)
    vs the path's own site count; agreement confirms the hop count. A single ttl value
    = both directions measured the same; "lo/hi" (e.g. 2/3) = the two directions
    measured DIFFERENT hop counts (a hop-asymmetric pair — the platform uses the lo path).
bneck = min link bandwidth on the path = the effective pipe (a path that dips onto a
100 G edge is a 100 G pipe even if the rest is the 1200 G ring). Read straight from
the emitted platform. Regenerate: `python3 10_report_tables.py core-routes`.

```
A     B       meas  Σpath     Δ  ttl hop  bneck  path
----------------------------------------------------------------------------------
STAR  NEWY   10.94  10.94  0.00    2   1  1200G  STAR-NEWY
STAR  WASH   13.44  13.48  0.04    3   2  1200G  STAR-NEWY-WASH
STAR  ATLA   19.77  19.85  0.08    4   3  1200G  STAR-NEWY-WASH-ATLA
STAR  DALL   10.00  10.04  0.04    3   2   100G  STAR-KANS-DALL
STAR  LOSA   21.46  21.50  0.04  3/4   2  1200G  STAR-SALT-LOSA
STAR  SALT   15.38  15.37  0.00  2/3   1  1200G  STAR-SALT
STAR  KANS    5.49   5.50  0.00    2   1   100G  STAR-KANS
STAR  SEAT   33.70  33.77  0.08  4/5   3   100G  STAR-SALT-LOSA-SEAT
NEWY  WASH    2.54   2.54  0.00    2   1  1200G  NEWY-WASH
NEWY  ATLA    8.88   8.91  0.03    3   2  1200G  NEWY-WASH-ATLA
NEWY  DALL   20.92  20.98  0.06    4   3   100G  NEWY-STAR-KANS-DALL
NEWY  LOSA   32.36  32.43  0.07    4   3  1200G  NEWY-STAR-SALT-LOSA
NEWY  SALT   26.28  26.31  0.03    3   2  1200G  NEWY-STAR-SALT
NEWY  KANS   16.42  16.44  0.02    3   2   100G  NEWY-STAR-KANS
NEWY  SEAT   44.60  44.71  0.11    5   4   100G  NEWY-STAR-SALT-LOSA-SEAT
WASH  ATLA    6.37   6.37  0.00    2   1  1200G  WASH-ATLA
WASH  DALL   14.67  14.71  0.04    3   2  1200G  WASH-ATLA-DALL
WASH  LOSA   32.14  32.20  0.07    4   3  1200G  WASH-ATLA-DALL-LOSA
WASH  SALT   28.77  28.85  0.09    4   3  1200G  WASH-NEWY-STAR-SALT
WASH  KANS   19.19  19.25  0.06    4   3   100G  WASH-ATLA-DALL-KANS
WASH  SEAT   44.37  44.48  0.11    5   4   100G  WASH-ATLA-DALL-LOSA-SEAT
ATLA  DALL    8.34   8.34  0.00    2   1  1200G  ATLA-DALL
ATLA  LOSA   25.81  25.84  0.03    3   2  1200G  ATLA-DALL-LOSA
ATLA  SALT   27.33  22.81  4.52    4   3   100G  ATLA-DALL-KANS-SALT
ATLA  KANS   12.86  12.88  0.02    3   2   100G  ATLA-DALL-KANS
ATLA  SEAT   38.05  38.11  0.06    4   3   100G  ATLA-DALL-LOSA-SEAT
DALL  LOSA   17.49  17.49  0.00    2   1  1200G  DALL-LOSA
DALL  SALT   19.01  14.47  4.54    3   2   100G  DALL-KANS-SALT
DALL  KANS    4.54   4.54  0.00    2   1   100G  DALL-KANS
DALL  SEAT   29.73  29.77  0.04    3   2   100G  DALL-LOSA-SEAT
LOSA  SALT    6.12   6.12  0.00    2   1  1200G  LOSA-SALT
LOSA  KANS   19.01  16.05  2.96    3   2   100G  LOSA-SALT-KANS
LOSA  SEAT   12.28  12.28  0.00    2   1   100G  LOSA-SEAT
SALT  KANS    9.93   9.93  0.00    2   1   100G  SALT-KANS
SALT  SEAT   18.37  18.40  0.03    3   2   100G  SALT-LOSA-SEAT
KANS  SEAT   34.25  34.31  0.06    4   3   100G  KANS-DALL-LOSA-SEAT
----------------------------------------------------------------------------------
meas = measured one-way (min-RTT/2); Σpath = platform path's summed link latency.
Most rows: Δ≈0 and ttl = hop+1 — the reconstructed path reproduces BOTH measurements.

The 6 asymmetric core pairs (FABNet routes them differently each way):
  latency-asymmetric (3, Δ>1.5): ATLA↔SALT, DALL↔SALT, LOSA↔KANS  — same hop count, different-latency path each way; RTT/2 = the average.
  hop-asymmetric (3, ttl lo/hi): STAR↔LOSA, STAR↔SALT, STAR↔SEAT  — one way the direct SALT-STAR 1200G ring, the other the KANS detour (~equal latency).
```

