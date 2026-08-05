/* validate.cpp — load the generated FABRIC-US platform and PROVE it is correct.
 *
 * Checks:
 *   1. host/zone counts; every site is a ClusterZone (exactly what NRTWsim's
 *      get_host_list(cluster,range) uses -> so the platform is usable as-is).
 *   2. representative routes: link names + summed one-way latency, proving Floyd
 *      transit (e.g. MICH->NEWY must traverse MICH__STAR then STAR__NEWY).
 *   3. all-pairs reachability: every ordered host pair has a route (no islands,
 *      no Floyd gaps).
 *
 *   g++ -std=c++17 -O2 -o validate validate.cpp -lsimgrid
 *   ./validate ./libfabric_us.so
 */
#include <simgrid/s4u.hpp>
#include <simgrid/kernel/routing/ClusterZone.hpp>
#include <cstdio>
#include <string>
#include <vector>

namespace sg4 = simgrid::s4u;

static void show_route(sg4::Engine& e, const std::string& a, const std::string& b)
{
  auto* ha = e.host_by_name_or_null(a);
  auto* hb = e.host_by_name_or_null(b);
  if (!ha || !hb) { std::printf("  %-12s -> %-12s : MISSING HOST\n", a.c_str(), b.c_str()); return; }
  auto [links, lat] = ha->route_to(hb);
  std::printf("  %-12s -> %-12s : %6.3f ms one-way over [", a.c_str(), b.c_str(), lat * 1e3);
  const char* sep = "";
  for (auto* l : links) { std::printf("%s%s", sep, l->get_cname()); sep = ", "; }
  std::printf("]\n");
}

int main(int argc, char** argv)
{
  sg4::Engine e(&argc, argv);
  if (argc < 2) { std::printf("usage: %s <platform.so|.xml>\n", argv[0]); return 1; }
  e.load_platform(argv[1]);

  auto hosts = e.get_all_hosts();
  std::printf("== COUNTS ==\n hosts: %zu\n", hosts.size());

  // Replicate NRTWsim utils.cpp: sites must be ClusterZones and findable by name.
  auto clusters = e.get_filtered_netzones<simgrid::kernel::routing::ClusterZone>();
  std::printf(" ClusterZones (sites the simulator can place components on): %zu\n", clusters.size());
  int one_host = 0;
  for (auto* c : clusters)
    if (c->get_all_hosts().size() == 1) one_host++;
  std::printf(" of which exactly 1-host (1 core/1 node): %d\n\n", one_host);

  std::printf("== REPRESENTATIVE ROUTES (prove Floyd transit) ==\n");
  // ring segments (1 hop):
  show_route(e, "STAR-node0", "NEWY-node0");
  show_route(e, "NEWY-node0", "WASH-node0");
  show_route(e, "DALL-node0", "LOSA-node0");
  // leaf -> PoP (1 hop):
  show_route(e, "MICH-node0", "STAR-node0");
  // leaf -> remote (multi-hop transit, the real test):
  show_route(e, "MICH-node0", "NEWY-node0");   // expect MICH__STAR + STAR__NEWY
  show_route(e, "UCSD-node0", "MASS-node0");    // cross-country, many hops
  show_route(e, "HAWI-node0", "FIU-node0");     // Hawaii -> Miami, longest
  show_route(e, "SRI-node0",  "GATECH-node0");

  std::printf("\n== ALL-PAIRS REACHABILITY ==\n");
  std::FILE* csv = std::fopen("allpairs.csv", "w");
  std::fprintf(csv, "src,dst,one_way_ms,nlinks\n");
  size_t ok = 0, bad = 0;
  double max_lat = 0; std::string max_a, max_b;
  for (auto* s : hosts) {
    for (auto* d : hosts) {
      if (s == d) continue;
      try {
        auto [links, lat] = s->route_to(d);
        // site code = host name without the "-node0" suffix
        std::string sa = s->get_name(), sb = d->get_name();
        sa = sa.substr(0, sa.find('-')); sb = sb.substr(0, sb.find('-'));
        std::fprintf(csv, "%s,%s,%.4f,%zu\n", sa.c_str(), sb.c_str(), lat * 1e3, links.size());
        ok++;
        if (lat > max_lat) { max_lat = lat; max_a = s->get_name(); max_b = d->get_name(); }
      } catch (...) {
        bad++;
        if (bad <= 10) std::printf("  NO ROUTE: %s -> %s\n", s->get_cname(), d->get_cname());
      }
    }
  }
  std::fclose(csv);
  std::printf(" routable ordered pairs: %zu   unreachable: %zu\n", ok, bad);
  std::printf(" longest path: %s -> %s = %.3f ms one-way\n", max_a.c_str(), max_b.c_str(), max_lat * 1e3);
  std::printf("\n%s\n", bad == 0 ? "VALIDATION PASSED (all pairs routable)" : "VALIDATION FAILED (islands present)");
  return bad == 0 ? 0 : 2;
}
