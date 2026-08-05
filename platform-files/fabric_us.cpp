/* fabric_us.cpp - SimGrid S4U platform: FABRIC testbed US inter-site network,
 *                 ROUTED THE WAY FABNetv4 ACTUALLY ROUTES (measured).
 * AUTO-GENERATED 2026-08-05 03:49 UTC by 05_emit_platform.py
 * Ground truth: live FABRIC orchestrator (list_sites/list_links) + ping min-RTT
 * & reply-TTL between directly-connected FABRIC VMs on FABNetv4.
 *   sites  : 28 US FABRIC sites (incl. Hawaii/HAWI); each a StarZone/
 *            ClusterZone with ONE host = 1 core (compute deliberately trivial).
 *   links  : 31 FABNet-direct links; nominal bw, latency = measured one-way
 *            (27 measured, 3 estimated).
 *   dropped: 3 physical chords FABNet does NOT forward over (L3 takes the ring):
 *            ATLA-STAR  (L3 routes 4-site, ~19.8ms one-way)
 *            SALT-SEAT  (L3 routes 3-site, ~18.4ms one-way)
 *            STAR-WASH  (L3 routes 3-site, ~13.4ms one-way)
 *   routing: Full — 378 explicit pair routes; each core path RECONSTRUCTED
 *            from the measured matrix, each leaf path COMPOSED (uplink+core+downlink).
 * Rebuild: g++ -shared -fPIC -std=c++17 -o libfabric_us.so fabric_us.cpp -lsimgrid
 */
#include <simgrid/s4u.hpp>
#include <string>
#include <vector>
#include <unordered_map>

namespace sg4 = simgrid::s4u;

extern "C" void load_platform(const sg4::Engine& e);
void load_platform(const sg4::Engine& e)
{
  auto* root = e.get_netzone_root()->add_netzone_full("fabric_us");
  std::unordered_map<std::string, sg4::NetZone*> S;
  std::unordered_map<std::string, const sg4::Link*> L;
  auto site = [&](const std::string& n) {
    auto* z = root->add_netzone_star(n);
    auto* h = z->add_host(n + "-node0", "1Gf"); h->set_core_count(1);
    z->set_gateway(h); z->seal(); S[n] = z;
  };
  auto link = [&](const std::string& a, const std::string& b,
                  const std::string& bw, const std::string& lt) {
    auto* l = root->add_link(a + "__" + b, bw)->set_latency(lt);
    L[a + "__" + b] = l; L[b + "__" + a] = l;
  };
  auto route = [&](const std::string& a, const std::string& b,
                   const std::vector<std::string>& hops) {
    std::vector<sg4::LinkInRoute> r; for (auto& h : hops) r.emplace_back(L.at(h));
    root->add_route(S[a], S[b], r);
  };

  // ---- sites (1 host / 1 core) ----
  site("ATLA");   // 180 Peachtree,Atlanta, GA 30303
  site("CLEM");   // 340 Computer Court,Anderson,SC 29625
  site("DALL");   // 1950 N Stemmons Fwy,Dallas TX 75207
  site("EDC");    // 1725 S Oak St.,Champaign, IL 61820
  site("EDUKY");  // 301 Hilltop Avenue,Lexington, KY 40506
  site("FIU");    // 11001 SW 14th St,Miami,FL 33199
  site("GATECH"); // 760 West Peachtree Street NW,Atlanta, GA  30308
  site("GPN");    // 5115 Oak Street,Kansas City,MO 64112
  site("HAWI");   // 2520 Correa Road,Honolulu, Hi 96822
  site("INDI");   // 535 West Michigan Street,Indianapolis, IN 46202
  site("KANS");   // 1100 Walnut Street,Kansas City,MO 64106
  site("LOSA");   // 818 West 7th Street,Los Angeles, CA 90017
  site("MASS");   // 100 Bigelow Street,Holyoke, MA 01040
  site("MAX");    // 4161 Fieldhouse Drive,College Park,MD 20742
  site("MICH");   // 2530 Draper Dr,Ann Arbor, MI 48109
  site("NCSA");   // 1725 S Oak St.,Champaign, IL 61820
  site("NEWY");   // 32 Sixth Avenue,New York, NY 10013
  site("PRIN");   //  151, Forrestal Road, Plainsboro Township, Middlesex County, New Jersey, 08537
  site("PSC");    // 4350 Northern Pike,Monroeville, PA 15146
  site("RUTG");   // 120 Avenue E,Piscataway, New Jersey
  site("SALT");   // 572 Delong Street,Salt Lake City, UT 84104
  site("SEAT");   // 2001 6th Ave,Seattle, WA 98121
  site("SRI");    // 333 Ravenswood Avenue,Menlo Park, CA 94025
  site("STAR");   // 710 North Lakeshore Drive, 60611
  site("TACC");   // 10100 Burnet Rd,Austin, TX 78758
  site("UCSD");   // 10100 Hopkins Drive,CA 92093
  site("UTAH");   // 875 South West Temple,Salt Lake City, UT  84101
  site("WASH");   // 1755 Old Meadow Road, 22102

  // ---- FABNet-direct links (nominal bw, measured one-way latency) ----
  link("ATLA", "DALL", "1200Gbps", "8.3410ms");   // 1200G
  link("ATLA", "WASH", "1200Gbps", "6.3680ms");   // 1200G
  link("DALL", "LOSA", "1200Gbps", "17.4945ms");  // 1200G
  link("LOSA", "SALT", "1200Gbps", "6.1210ms");   // 1200G
  link("NEWY", "STAR", "1200Gbps", "10.9390ms");  // 1200G
  link("NEWY", "WASH", "1200Gbps", "2.5415ms");   // 1200G
  link("SALT", "STAR", "1200Gbps", "15.3740ms");  // 1200G
  link("ATLA", "FIU", "100Gbps", "6.2825ms");     // 100G
  link("DALL", "KANS", "100Gbps", "4.5385ms");    // 100G
  link("EDC", "NCSA", "100Gbps", "0.0500ms");     // 100G  [COLOCATED]
  link("GPN", "KANS", "100Gbps", "0.1375ms");     // 100G
  link("INDI", "STAR", "100Gbps", "2.3140ms");    // 100G
  link("KANS", "SALT", "100Gbps", "9.9295ms");    // 100G
  link("KANS", "STAR", "100Gbps", "5.4985ms");    // 100G
  link("LOSA", "SEAT", "100Gbps", "12.2785ms");   // 100G
  link("LOSA", "UCSD", "100Gbps", "1.1485ms");    // 100G
  link("MASS", "NEWY", "100Gbps", "1.4195ms");    // 100G  [ESTIMATED]
  link("MICH", "STAR", "100Gbps", "2.5495ms");    // 100G
  link("NCSA", "STAR", "100Gbps", "1.2345ms");    // 100G
  link("NEWY", "PRIN", "100Gbps", "0.6670ms");    // 100G
  link("NEWY", "RUTG", "100Gbps", "0.5125ms");    // 100G
  link("SALT", "UTAH", "100Gbps", "0.1070ms");    // 100G
  link("CLEM", "WASH", "40Gbps", "8.1845ms");     // 40G
  link("HAWI", "LOSA", "40Gbps", "24.5615ms");    // 40G
  link("HAWI", "SEAT", "40Gbps", "36.5115ms");    // 40G
  link("MAX", "WASH", "40Gbps", "0.8705ms");      // 40G
  link("PSC", "WASH", "40Gbps", "3.0665ms");      // 40G
  link("EDUKY", "STAR", "10Gbps", "3.9846ms");    // 10G  [ESTIMATED]
  link("GATECH", "WASH", "10Gbps", "6.9965ms");   // 10G
  link("LOSA", "SRI", "10Gbps", "4.0685ms");      // 10G
  link("STAR", "TACC", "10Gbps", "11.1432ms");    // 10G  [ESTIMATED]

  // ---- explicit routes (Full): 378 pairs, measured-reconstructed / composed ----
  route("ATLA", "CLEM", {"ATLA__WASH", "WASH__CLEM"});
  route("ATLA", "DALL", {"ATLA__DALL"});
  route("ATLA", "EDC", {"ATLA__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__NCSA", "NCSA__EDC"});
  route("ATLA", "EDUKY", {"ATLA__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__EDUKY"});
  route("ATLA", "FIU", {"ATLA__FIU"});
  route("ATLA", "GATECH", {"ATLA__WASH", "WASH__GATECH"});
  route("ATLA", "GPN", {"ATLA__DALL", "DALL__KANS", "KANS__GPN"});
  route("ATLA", "HAWI", {"ATLA__DALL", "DALL__LOSA", "LOSA__HAWI"});
  route("ATLA", "INDI", {"ATLA__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__INDI"});
  route("ATLA", "KANS", {"ATLA__DALL", "DALL__KANS"});
  route("ATLA", "LOSA", {"ATLA__DALL", "DALL__LOSA"});
  route("ATLA", "MASS", {"ATLA__WASH", "WASH__NEWY", "NEWY__MASS"});
  route("ATLA", "MAX", {"ATLA__WASH", "WASH__MAX"});
  route("ATLA", "MICH", {"ATLA__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__MICH"});
  route("ATLA", "NCSA", {"ATLA__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__NCSA"});
  route("ATLA", "NEWY", {"ATLA__WASH", "WASH__NEWY"});
  route("ATLA", "PRIN", {"ATLA__WASH", "WASH__NEWY", "NEWY__PRIN"});
  route("ATLA", "PSC", {"ATLA__WASH", "WASH__PSC"});
  route("ATLA", "RUTG", {"ATLA__WASH", "WASH__NEWY", "NEWY__RUTG"});
  route("ATLA", "SALT", {"ATLA__DALL", "DALL__KANS", "KANS__SALT"});
  route("ATLA", "SEAT", {"ATLA__DALL", "DALL__LOSA", "LOSA__SEAT"});
  route("ATLA", "SRI", {"ATLA__DALL", "DALL__LOSA", "LOSA__SRI"});
  route("ATLA", "STAR", {"ATLA__WASH", "WASH__NEWY", "NEWY__STAR"});
  route("ATLA", "TACC", {"ATLA__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__TACC"});
  route("ATLA", "UCSD", {"ATLA__DALL", "DALL__LOSA", "LOSA__UCSD"});
  route("ATLA", "UTAH", {"ATLA__DALL", "DALL__KANS", "KANS__SALT", "SALT__UTAH"});
  route("ATLA", "WASH", {"ATLA__WASH"});
  route("CLEM", "DALL", {"CLEM__WASH", "WASH__ATLA", "ATLA__DALL"});
  route("CLEM", "EDC", {"CLEM__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__NCSA", "NCSA__EDC"});
  route("CLEM", "EDUKY", {"CLEM__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__EDUKY"});
  route("CLEM", "FIU", {"CLEM__WASH", "WASH__ATLA", "ATLA__FIU"});
  route("CLEM", "GATECH", {"CLEM__WASH", "WASH__GATECH"});
  route("CLEM", "GPN", {"CLEM__WASH", "WASH__ATLA", "ATLA__DALL", "DALL__KANS", "KANS__GPN"});
  route("CLEM", "HAWI", {"CLEM__WASH", "WASH__ATLA", "ATLA__DALL", "DALL__LOSA", "LOSA__HAWI"});
  route("CLEM", "INDI", {"CLEM__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__INDI"});
  route("CLEM", "KANS", {"CLEM__WASH", "WASH__ATLA", "ATLA__DALL", "DALL__KANS"});
  route("CLEM", "LOSA", {"CLEM__WASH", "WASH__ATLA", "ATLA__DALL", "DALL__LOSA"});
  route("CLEM", "MASS", {"CLEM__WASH", "WASH__NEWY", "NEWY__MASS"});
  route("CLEM", "MAX", {"CLEM__WASH", "WASH__MAX"});
  route("CLEM", "MICH", {"CLEM__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__MICH"});
  route("CLEM", "NCSA", {"CLEM__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__NCSA"});
  route("CLEM", "NEWY", {"CLEM__WASH", "WASH__NEWY"});
  route("CLEM", "PRIN", {"CLEM__WASH", "WASH__NEWY", "NEWY__PRIN"});
  route("CLEM", "PSC", {"CLEM__WASH", "WASH__PSC"});
  route("CLEM", "RUTG", {"CLEM__WASH", "WASH__NEWY", "NEWY__RUTG"});
  route("CLEM", "SALT", {"CLEM__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__SALT"});
  route("CLEM", "SEAT", {"CLEM__WASH", "WASH__ATLA", "ATLA__DALL", "DALL__LOSA", "LOSA__SEAT"});
  route("CLEM", "SRI", {"CLEM__WASH", "WASH__ATLA", "ATLA__DALL", "DALL__LOSA", "LOSA__SRI"});
  route("CLEM", "STAR", {"CLEM__WASH", "WASH__NEWY", "NEWY__STAR"});
  route("CLEM", "TACC", {"CLEM__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__TACC"});
  route("CLEM", "UCSD", {"CLEM__WASH", "WASH__ATLA", "ATLA__DALL", "DALL__LOSA", "LOSA__UCSD"});
  route("CLEM", "UTAH", {"CLEM__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__SALT", "SALT__UTAH"});
  route("CLEM", "WASH", {"CLEM__WASH"});
  route("DALL", "EDC", {"DALL__KANS", "KANS__STAR", "STAR__NCSA", "NCSA__EDC"});
  route("DALL", "EDUKY", {"DALL__KANS", "KANS__STAR", "STAR__EDUKY"});
  route("DALL", "FIU", {"DALL__ATLA", "ATLA__FIU"});
  route("DALL", "GATECH", {"DALL__ATLA", "ATLA__WASH", "WASH__GATECH"});
  route("DALL", "GPN", {"DALL__KANS", "KANS__GPN"});
  route("DALL", "HAWI", {"DALL__LOSA", "LOSA__HAWI"});
  route("DALL", "INDI", {"DALL__KANS", "KANS__STAR", "STAR__INDI"});
  route("DALL", "KANS", {"DALL__KANS"});
  route("DALL", "LOSA", {"DALL__LOSA"});
  route("DALL", "MASS", {"DALL__KANS", "KANS__STAR", "STAR__NEWY", "NEWY__MASS"});
  route("DALL", "MAX", {"DALL__ATLA", "ATLA__WASH", "WASH__MAX"});
  route("DALL", "MICH", {"DALL__KANS", "KANS__STAR", "STAR__MICH"});
  route("DALL", "NCSA", {"DALL__KANS", "KANS__STAR", "STAR__NCSA"});
  route("DALL", "NEWY", {"DALL__KANS", "KANS__STAR", "STAR__NEWY"});
  route("DALL", "PRIN", {"DALL__KANS", "KANS__STAR", "STAR__NEWY", "NEWY__PRIN"});
  route("DALL", "PSC", {"DALL__ATLA", "ATLA__WASH", "WASH__PSC"});
  route("DALL", "RUTG", {"DALL__KANS", "KANS__STAR", "STAR__NEWY", "NEWY__RUTG"});
  route("DALL", "SALT", {"DALL__KANS", "KANS__SALT"});
  route("DALL", "SEAT", {"DALL__LOSA", "LOSA__SEAT"});
  route("DALL", "SRI", {"DALL__LOSA", "LOSA__SRI"});
  route("DALL", "STAR", {"DALL__KANS", "KANS__STAR"});
  route("DALL", "TACC", {"DALL__KANS", "KANS__STAR", "STAR__TACC"});
  route("DALL", "UCSD", {"DALL__LOSA", "LOSA__UCSD"});
  route("DALL", "UTAH", {"DALL__KANS", "KANS__SALT", "SALT__UTAH"});
  route("DALL", "WASH", {"DALL__ATLA", "ATLA__WASH"});
  route("EDC", "EDUKY", {"EDC__NCSA", "NCSA__STAR", "STAR__EDUKY"});
  route("EDC", "FIU", {"EDC__NCSA", "NCSA__STAR", "STAR__NEWY", "NEWY__WASH", "WASH__ATLA", "ATLA__FIU"});
  route("EDC", "GATECH", {"EDC__NCSA", "NCSA__STAR", "STAR__NEWY", "NEWY__WASH", "WASH__GATECH"});
  route("EDC", "GPN", {"EDC__NCSA", "NCSA__STAR", "STAR__KANS", "KANS__GPN"});
  route("EDC", "HAWI", {"EDC__NCSA", "NCSA__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__HAWI"});
  route("EDC", "INDI", {"EDC__NCSA", "NCSA__STAR", "STAR__INDI"});
  route("EDC", "KANS", {"EDC__NCSA", "NCSA__STAR", "STAR__KANS"});
  route("EDC", "LOSA", {"EDC__NCSA", "NCSA__STAR", "STAR__SALT", "SALT__LOSA"});
  route("EDC", "MASS", {"EDC__NCSA", "NCSA__STAR", "STAR__NEWY", "NEWY__MASS"});
  route("EDC", "MAX", {"EDC__NCSA", "NCSA__STAR", "STAR__NEWY", "NEWY__WASH", "WASH__MAX"});
  route("EDC", "MICH", {"EDC__NCSA", "NCSA__STAR", "STAR__MICH"});
  route("EDC", "NCSA", {"EDC__NCSA"});
  route("EDC", "NEWY", {"EDC__NCSA", "NCSA__STAR", "STAR__NEWY"});
  route("EDC", "PRIN", {"EDC__NCSA", "NCSA__STAR", "STAR__NEWY", "NEWY__PRIN"});
  route("EDC", "PSC", {"EDC__NCSA", "NCSA__STAR", "STAR__NEWY", "NEWY__WASH", "WASH__PSC"});
  route("EDC", "RUTG", {"EDC__NCSA", "NCSA__STAR", "STAR__NEWY", "NEWY__RUTG"});
  route("EDC", "SALT", {"EDC__NCSA", "NCSA__STAR", "STAR__SALT"});
  route("EDC", "SEAT", {"EDC__NCSA", "NCSA__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__SEAT"});
  route("EDC", "SRI", {"EDC__NCSA", "NCSA__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__SRI"});
  route("EDC", "STAR", {"EDC__NCSA", "NCSA__STAR"});
  route("EDC", "TACC", {"EDC__NCSA", "NCSA__STAR", "STAR__TACC"});
  route("EDC", "UCSD", {"EDC__NCSA", "NCSA__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__UCSD"});
  route("EDC", "UTAH", {"EDC__NCSA", "NCSA__STAR", "STAR__SALT", "SALT__UTAH"});
  route("EDC", "WASH", {"EDC__NCSA", "NCSA__STAR", "STAR__NEWY", "NEWY__WASH"});
  route("EDUKY", "FIU", {"EDUKY__STAR", "STAR__NEWY", "NEWY__WASH", "WASH__ATLA", "ATLA__FIU"});
  route("EDUKY", "GATECH", {"EDUKY__STAR", "STAR__NEWY", "NEWY__WASH", "WASH__GATECH"});
  route("EDUKY", "GPN", {"EDUKY__STAR", "STAR__KANS", "KANS__GPN"});
  route("EDUKY", "HAWI", {"EDUKY__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__HAWI"});
  route("EDUKY", "INDI", {"EDUKY__STAR", "STAR__INDI"});
  route("EDUKY", "KANS", {"EDUKY__STAR", "STAR__KANS"});
  route("EDUKY", "LOSA", {"EDUKY__STAR", "STAR__SALT", "SALT__LOSA"});
  route("EDUKY", "MASS", {"EDUKY__STAR", "STAR__NEWY", "NEWY__MASS"});
  route("EDUKY", "MAX", {"EDUKY__STAR", "STAR__NEWY", "NEWY__WASH", "WASH__MAX"});
  route("EDUKY", "MICH", {"EDUKY__STAR", "STAR__MICH"});
  route("EDUKY", "NCSA", {"EDUKY__STAR", "STAR__NCSA"});
  route("EDUKY", "NEWY", {"EDUKY__STAR", "STAR__NEWY"});
  route("EDUKY", "PRIN", {"EDUKY__STAR", "STAR__NEWY", "NEWY__PRIN"});
  route("EDUKY", "PSC", {"EDUKY__STAR", "STAR__NEWY", "NEWY__WASH", "WASH__PSC"});
  route("EDUKY", "RUTG", {"EDUKY__STAR", "STAR__NEWY", "NEWY__RUTG"});
  route("EDUKY", "SALT", {"EDUKY__STAR", "STAR__SALT"});
  route("EDUKY", "SEAT", {"EDUKY__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__SEAT"});
  route("EDUKY", "SRI", {"EDUKY__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__SRI"});
  route("EDUKY", "STAR", {"EDUKY__STAR"});
  route("EDUKY", "TACC", {"EDUKY__STAR", "STAR__TACC"});
  route("EDUKY", "UCSD", {"EDUKY__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__UCSD"});
  route("EDUKY", "UTAH", {"EDUKY__STAR", "STAR__SALT", "SALT__UTAH"});
  route("EDUKY", "WASH", {"EDUKY__STAR", "STAR__NEWY", "NEWY__WASH"});
  route("FIU", "GATECH", {"FIU__ATLA", "ATLA__WASH", "WASH__GATECH"});
  route("FIU", "GPN", {"FIU__ATLA", "ATLA__DALL", "DALL__KANS", "KANS__GPN"});
  route("FIU", "HAWI", {"FIU__ATLA", "ATLA__DALL", "DALL__LOSA", "LOSA__HAWI"});
  route("FIU", "INDI", {"FIU__ATLA", "ATLA__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__INDI"});
  route("FIU", "KANS", {"FIU__ATLA", "ATLA__DALL", "DALL__KANS"});
  route("FIU", "LOSA", {"FIU__ATLA", "ATLA__DALL", "DALL__LOSA"});
  route("FIU", "MASS", {"FIU__ATLA", "ATLA__WASH", "WASH__NEWY", "NEWY__MASS"});
  route("FIU", "MAX", {"FIU__ATLA", "ATLA__WASH", "WASH__MAX"});
  route("FIU", "MICH", {"FIU__ATLA", "ATLA__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__MICH"});
  route("FIU", "NCSA", {"FIU__ATLA", "ATLA__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__NCSA"});
  route("FIU", "NEWY", {"FIU__ATLA", "ATLA__WASH", "WASH__NEWY"});
  route("FIU", "PRIN", {"FIU__ATLA", "ATLA__WASH", "WASH__NEWY", "NEWY__PRIN"});
  route("FIU", "PSC", {"FIU__ATLA", "ATLA__WASH", "WASH__PSC"});
  route("FIU", "RUTG", {"FIU__ATLA", "ATLA__WASH", "WASH__NEWY", "NEWY__RUTG"});
  route("FIU", "SALT", {"FIU__ATLA", "ATLA__DALL", "DALL__KANS", "KANS__SALT"});
  route("FIU", "SEAT", {"FIU__ATLA", "ATLA__DALL", "DALL__LOSA", "LOSA__SEAT"});
  route("FIU", "SRI", {"FIU__ATLA", "ATLA__DALL", "DALL__LOSA", "LOSA__SRI"});
  route("FIU", "STAR", {"FIU__ATLA", "ATLA__WASH", "WASH__NEWY", "NEWY__STAR"});
  route("FIU", "TACC", {"FIU__ATLA", "ATLA__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__TACC"});
  route("FIU", "UCSD", {"FIU__ATLA", "ATLA__DALL", "DALL__LOSA", "LOSA__UCSD"});
  route("FIU", "UTAH", {"FIU__ATLA", "ATLA__DALL", "DALL__KANS", "KANS__SALT", "SALT__UTAH"});
  route("FIU", "WASH", {"FIU__ATLA", "ATLA__WASH"});
  route("GATECH", "GPN", {"GATECH__WASH", "WASH__ATLA", "ATLA__DALL", "DALL__KANS", "KANS__GPN"});
  route("GATECH", "HAWI", {"GATECH__WASH", "WASH__ATLA", "ATLA__DALL", "DALL__LOSA", "LOSA__HAWI"});
  route("GATECH", "INDI", {"GATECH__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__INDI"});
  route("GATECH", "KANS", {"GATECH__WASH", "WASH__ATLA", "ATLA__DALL", "DALL__KANS"});
  route("GATECH", "LOSA", {"GATECH__WASH", "WASH__ATLA", "ATLA__DALL", "DALL__LOSA"});
  route("GATECH", "MASS", {"GATECH__WASH", "WASH__NEWY", "NEWY__MASS"});
  route("GATECH", "MAX", {"GATECH__WASH", "WASH__MAX"});
  route("GATECH", "MICH", {"GATECH__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__MICH"});
  route("GATECH", "NCSA", {"GATECH__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__NCSA"});
  route("GATECH", "NEWY", {"GATECH__WASH", "WASH__NEWY"});
  route("GATECH", "PRIN", {"GATECH__WASH", "WASH__NEWY", "NEWY__PRIN"});
  route("GATECH", "PSC", {"GATECH__WASH", "WASH__PSC"});
  route("GATECH", "RUTG", {"GATECH__WASH", "WASH__NEWY", "NEWY__RUTG"});
  route("GATECH", "SALT", {"GATECH__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__SALT"});
  route("GATECH", "SEAT", {"GATECH__WASH", "WASH__ATLA", "ATLA__DALL", "DALL__LOSA", "LOSA__SEAT"});
  route("GATECH", "SRI", {"GATECH__WASH", "WASH__ATLA", "ATLA__DALL", "DALL__LOSA", "LOSA__SRI"});
  route("GATECH", "STAR", {"GATECH__WASH", "WASH__NEWY", "NEWY__STAR"});
  route("GATECH", "TACC", {"GATECH__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__TACC"});
  route("GATECH", "UCSD", {"GATECH__WASH", "WASH__ATLA", "ATLA__DALL", "DALL__LOSA", "LOSA__UCSD"});
  route("GATECH", "UTAH", {"GATECH__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__SALT", "SALT__UTAH"});
  route("GATECH", "WASH", {"GATECH__WASH"});
  route("GPN", "HAWI", {"GPN__KANS", "KANS__SALT", "SALT__LOSA", "LOSA__HAWI"});
  route("GPN", "INDI", {"GPN__KANS", "KANS__STAR", "STAR__INDI"});
  route("GPN", "KANS", {"GPN__KANS"});
  route("GPN", "LOSA", {"GPN__KANS", "KANS__SALT", "SALT__LOSA"});
  route("GPN", "MASS", {"GPN__KANS", "KANS__STAR", "STAR__NEWY", "NEWY__MASS"});
  route("GPN", "MAX", {"GPN__KANS", "KANS__DALL", "DALL__ATLA", "ATLA__WASH", "WASH__MAX"});
  route("GPN", "MICH", {"GPN__KANS", "KANS__STAR", "STAR__MICH"});
  route("GPN", "NCSA", {"GPN__KANS", "KANS__STAR", "STAR__NCSA"});
  route("GPN", "NEWY", {"GPN__KANS", "KANS__STAR", "STAR__NEWY"});
  route("GPN", "PRIN", {"GPN__KANS", "KANS__STAR", "STAR__NEWY", "NEWY__PRIN"});
  route("GPN", "PSC", {"GPN__KANS", "KANS__DALL", "DALL__ATLA", "ATLA__WASH", "WASH__PSC"});
  route("GPN", "RUTG", {"GPN__KANS", "KANS__STAR", "STAR__NEWY", "NEWY__RUTG"});
  route("GPN", "SALT", {"GPN__KANS", "KANS__SALT"});
  route("GPN", "SEAT", {"GPN__KANS", "KANS__DALL", "DALL__LOSA", "LOSA__SEAT"});
  route("GPN", "SRI", {"GPN__KANS", "KANS__SALT", "SALT__LOSA", "LOSA__SRI"});
  route("GPN", "STAR", {"GPN__KANS", "KANS__STAR"});
  route("GPN", "TACC", {"GPN__KANS", "KANS__STAR", "STAR__TACC"});
  route("GPN", "UCSD", {"GPN__KANS", "KANS__SALT", "SALT__LOSA", "LOSA__UCSD"});
  route("GPN", "UTAH", {"GPN__KANS", "KANS__SALT", "SALT__UTAH"});
  route("GPN", "WASH", {"GPN__KANS", "KANS__DALL", "DALL__ATLA", "ATLA__WASH"});
  route("HAWI", "INDI", {"HAWI__LOSA", "LOSA__SALT", "SALT__STAR", "STAR__INDI"});
  route("HAWI", "KANS", {"HAWI__LOSA", "LOSA__SALT", "SALT__KANS"});
  route("HAWI", "LOSA", {"HAWI__LOSA"});
  route("HAWI", "MASS", {"HAWI__LOSA", "LOSA__SALT", "SALT__STAR", "STAR__NEWY", "NEWY__MASS"});
  route("HAWI", "MAX", {"HAWI__LOSA", "LOSA__DALL", "DALL__ATLA", "ATLA__WASH", "WASH__MAX"});
  route("HAWI", "MICH", {"HAWI__LOSA", "LOSA__SALT", "SALT__STAR", "STAR__MICH"});
  route("HAWI", "NCSA", {"HAWI__LOSA", "LOSA__SALT", "SALT__STAR", "STAR__NCSA"});
  route("HAWI", "NEWY", {"HAWI__LOSA", "LOSA__SALT", "SALT__STAR", "STAR__NEWY"});
  route("HAWI", "PRIN", {"HAWI__LOSA", "LOSA__SALT", "SALT__STAR", "STAR__NEWY", "NEWY__PRIN"});
  route("HAWI", "PSC", {"HAWI__LOSA", "LOSA__DALL", "DALL__ATLA", "ATLA__WASH", "WASH__PSC"});
  route("HAWI", "RUTG", {"HAWI__LOSA", "LOSA__SALT", "SALT__STAR", "STAR__NEWY", "NEWY__RUTG"});
  route("HAWI", "SALT", {"HAWI__LOSA", "LOSA__SALT"});
  route("HAWI", "SEAT", {"HAWI__SEAT"});
  route("HAWI", "SRI", {"HAWI__LOSA", "LOSA__SRI"});
  route("HAWI", "STAR", {"HAWI__LOSA", "LOSA__SALT", "SALT__STAR"});
  route("HAWI", "TACC", {"HAWI__LOSA", "LOSA__SALT", "SALT__STAR", "STAR__TACC"});
  route("HAWI", "UCSD", {"HAWI__LOSA", "LOSA__UCSD"});
  route("HAWI", "UTAH", {"HAWI__LOSA", "LOSA__SALT", "SALT__UTAH"});
  route("HAWI", "WASH", {"HAWI__LOSA", "LOSA__DALL", "DALL__ATLA", "ATLA__WASH"});
  route("INDI", "KANS", {"INDI__STAR", "STAR__KANS"});
  route("INDI", "LOSA", {"INDI__STAR", "STAR__SALT", "SALT__LOSA"});
  route("INDI", "MASS", {"INDI__STAR", "STAR__NEWY", "NEWY__MASS"});
  route("INDI", "MAX", {"INDI__STAR", "STAR__NEWY", "NEWY__WASH", "WASH__MAX"});
  route("INDI", "MICH", {"INDI__STAR", "STAR__MICH"});
  route("INDI", "NCSA", {"INDI__STAR", "STAR__NCSA"});
  route("INDI", "NEWY", {"INDI__STAR", "STAR__NEWY"});
  route("INDI", "PRIN", {"INDI__STAR", "STAR__NEWY", "NEWY__PRIN"});
  route("INDI", "PSC", {"INDI__STAR", "STAR__NEWY", "NEWY__WASH", "WASH__PSC"});
  route("INDI", "RUTG", {"INDI__STAR", "STAR__NEWY", "NEWY__RUTG"});
  route("INDI", "SALT", {"INDI__STAR", "STAR__SALT"});
  route("INDI", "SEAT", {"INDI__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__SEAT"});
  route("INDI", "SRI", {"INDI__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__SRI"});
  route("INDI", "STAR", {"INDI__STAR"});
  route("INDI", "TACC", {"INDI__STAR", "STAR__TACC"});
  route("INDI", "UCSD", {"INDI__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__UCSD"});
  route("INDI", "UTAH", {"INDI__STAR", "STAR__SALT", "SALT__UTAH"});
  route("INDI", "WASH", {"INDI__STAR", "STAR__NEWY", "NEWY__WASH"});
  route("KANS", "LOSA", {"KANS__SALT", "SALT__LOSA"});
  route("KANS", "MASS", {"KANS__STAR", "STAR__NEWY", "NEWY__MASS"});
  route("KANS", "MAX", {"KANS__DALL", "DALL__ATLA", "ATLA__WASH", "WASH__MAX"});
  route("KANS", "MICH", {"KANS__STAR", "STAR__MICH"});
  route("KANS", "NCSA", {"KANS__STAR", "STAR__NCSA"});
  route("KANS", "NEWY", {"KANS__STAR", "STAR__NEWY"});
  route("KANS", "PRIN", {"KANS__STAR", "STAR__NEWY", "NEWY__PRIN"});
  route("KANS", "PSC", {"KANS__DALL", "DALL__ATLA", "ATLA__WASH", "WASH__PSC"});
  route("KANS", "RUTG", {"KANS__STAR", "STAR__NEWY", "NEWY__RUTG"});
  route("KANS", "SALT", {"KANS__SALT"});
  route("KANS", "SEAT", {"KANS__DALL", "DALL__LOSA", "LOSA__SEAT"});
  route("KANS", "SRI", {"KANS__SALT", "SALT__LOSA", "LOSA__SRI"});
  route("KANS", "STAR", {"KANS__STAR"});
  route("KANS", "TACC", {"KANS__STAR", "STAR__TACC"});
  route("KANS", "UCSD", {"KANS__SALT", "SALT__LOSA", "LOSA__UCSD"});
  route("KANS", "UTAH", {"KANS__SALT", "SALT__UTAH"});
  route("KANS", "WASH", {"KANS__DALL", "DALL__ATLA", "ATLA__WASH"});
  route("LOSA", "MASS", {"LOSA__SALT", "SALT__STAR", "STAR__NEWY", "NEWY__MASS"});
  route("LOSA", "MAX", {"LOSA__DALL", "DALL__ATLA", "ATLA__WASH", "WASH__MAX"});
  route("LOSA", "MICH", {"LOSA__SALT", "SALT__STAR", "STAR__MICH"});
  route("LOSA", "NCSA", {"LOSA__SALT", "SALT__STAR", "STAR__NCSA"});
  route("LOSA", "NEWY", {"LOSA__SALT", "SALT__STAR", "STAR__NEWY"});
  route("LOSA", "PRIN", {"LOSA__SALT", "SALT__STAR", "STAR__NEWY", "NEWY__PRIN"});
  route("LOSA", "PSC", {"LOSA__DALL", "DALL__ATLA", "ATLA__WASH", "WASH__PSC"});
  route("LOSA", "RUTG", {"LOSA__SALT", "SALT__STAR", "STAR__NEWY", "NEWY__RUTG"});
  route("LOSA", "SALT", {"LOSA__SALT"});
  route("LOSA", "SEAT", {"LOSA__SEAT"});
  route("LOSA", "SRI", {"LOSA__SRI"});
  route("LOSA", "STAR", {"LOSA__SALT", "SALT__STAR"});
  route("LOSA", "TACC", {"LOSA__SALT", "SALT__STAR", "STAR__TACC"});
  route("LOSA", "UCSD", {"LOSA__UCSD"});
  route("LOSA", "UTAH", {"LOSA__SALT", "SALT__UTAH"});
  route("LOSA", "WASH", {"LOSA__DALL", "DALL__ATLA", "ATLA__WASH"});
  route("MASS", "MAX", {"MASS__NEWY", "NEWY__WASH", "WASH__MAX"});
  route("MASS", "MICH", {"MASS__NEWY", "NEWY__STAR", "STAR__MICH"});
  route("MASS", "NCSA", {"MASS__NEWY", "NEWY__STAR", "STAR__NCSA"});
  route("MASS", "NEWY", {"MASS__NEWY"});
  route("MASS", "PRIN", {"MASS__NEWY", "NEWY__PRIN"});
  route("MASS", "PSC", {"MASS__NEWY", "NEWY__WASH", "WASH__PSC"});
  route("MASS", "RUTG", {"MASS__NEWY", "NEWY__RUTG"});
  route("MASS", "SALT", {"MASS__NEWY", "NEWY__STAR", "STAR__SALT"});
  route("MASS", "SEAT", {"MASS__NEWY", "NEWY__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__SEAT"});
  route("MASS", "SRI", {"MASS__NEWY", "NEWY__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__SRI"});
  route("MASS", "STAR", {"MASS__NEWY", "NEWY__STAR"});
  route("MASS", "TACC", {"MASS__NEWY", "NEWY__STAR", "STAR__TACC"});
  route("MASS", "UCSD", {"MASS__NEWY", "NEWY__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__UCSD"});
  route("MASS", "UTAH", {"MASS__NEWY", "NEWY__STAR", "STAR__SALT", "SALT__UTAH"});
  route("MASS", "WASH", {"MASS__NEWY", "NEWY__WASH"});
  route("MAX", "MICH", {"MAX__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__MICH"});
  route("MAX", "NCSA", {"MAX__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__NCSA"});
  route("MAX", "NEWY", {"MAX__WASH", "WASH__NEWY"});
  route("MAX", "PRIN", {"MAX__WASH", "WASH__NEWY", "NEWY__PRIN"});
  route("MAX", "PSC", {"MAX__WASH", "WASH__PSC"});
  route("MAX", "RUTG", {"MAX__WASH", "WASH__NEWY", "NEWY__RUTG"});
  route("MAX", "SALT", {"MAX__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__SALT"});
  route("MAX", "SEAT", {"MAX__WASH", "WASH__ATLA", "ATLA__DALL", "DALL__LOSA", "LOSA__SEAT"});
  route("MAX", "SRI", {"MAX__WASH", "WASH__ATLA", "ATLA__DALL", "DALL__LOSA", "LOSA__SRI"});
  route("MAX", "STAR", {"MAX__WASH", "WASH__NEWY", "NEWY__STAR"});
  route("MAX", "TACC", {"MAX__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__TACC"});
  route("MAX", "UCSD", {"MAX__WASH", "WASH__ATLA", "ATLA__DALL", "DALL__LOSA", "LOSA__UCSD"});
  route("MAX", "UTAH", {"MAX__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__SALT", "SALT__UTAH"});
  route("MAX", "WASH", {"MAX__WASH"});
  route("MICH", "NCSA", {"MICH__STAR", "STAR__NCSA"});
  route("MICH", "NEWY", {"MICH__STAR", "STAR__NEWY"});
  route("MICH", "PRIN", {"MICH__STAR", "STAR__NEWY", "NEWY__PRIN"});
  route("MICH", "PSC", {"MICH__STAR", "STAR__NEWY", "NEWY__WASH", "WASH__PSC"});
  route("MICH", "RUTG", {"MICH__STAR", "STAR__NEWY", "NEWY__RUTG"});
  route("MICH", "SALT", {"MICH__STAR", "STAR__SALT"});
  route("MICH", "SEAT", {"MICH__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__SEAT"});
  route("MICH", "SRI", {"MICH__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__SRI"});
  route("MICH", "STAR", {"MICH__STAR"});
  route("MICH", "TACC", {"MICH__STAR", "STAR__TACC"});
  route("MICH", "UCSD", {"MICH__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__UCSD"});
  route("MICH", "UTAH", {"MICH__STAR", "STAR__SALT", "SALT__UTAH"});
  route("MICH", "WASH", {"MICH__STAR", "STAR__NEWY", "NEWY__WASH"});
  route("NCSA", "NEWY", {"NCSA__STAR", "STAR__NEWY"});
  route("NCSA", "PRIN", {"NCSA__STAR", "STAR__NEWY", "NEWY__PRIN"});
  route("NCSA", "PSC", {"NCSA__STAR", "STAR__NEWY", "NEWY__WASH", "WASH__PSC"});
  route("NCSA", "RUTG", {"NCSA__STAR", "STAR__NEWY", "NEWY__RUTG"});
  route("NCSA", "SALT", {"NCSA__STAR", "STAR__SALT"});
  route("NCSA", "SEAT", {"NCSA__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__SEAT"});
  route("NCSA", "SRI", {"NCSA__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__SRI"});
  route("NCSA", "STAR", {"NCSA__STAR"});
  route("NCSA", "TACC", {"NCSA__STAR", "STAR__TACC"});
  route("NCSA", "UCSD", {"NCSA__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__UCSD"});
  route("NCSA", "UTAH", {"NCSA__STAR", "STAR__SALT", "SALT__UTAH"});
  route("NCSA", "WASH", {"NCSA__STAR", "STAR__NEWY", "NEWY__WASH"});
  route("NEWY", "PRIN", {"NEWY__PRIN"});
  route("NEWY", "PSC", {"NEWY__WASH", "WASH__PSC"});
  route("NEWY", "RUTG", {"NEWY__RUTG"});
  route("NEWY", "SALT", {"NEWY__STAR", "STAR__SALT"});
  route("NEWY", "SEAT", {"NEWY__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__SEAT"});
  route("NEWY", "SRI", {"NEWY__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__SRI"});
  route("NEWY", "STAR", {"NEWY__STAR"});
  route("NEWY", "TACC", {"NEWY__STAR", "STAR__TACC"});
  route("NEWY", "UCSD", {"NEWY__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__UCSD"});
  route("NEWY", "UTAH", {"NEWY__STAR", "STAR__SALT", "SALT__UTAH"});
  route("NEWY", "WASH", {"NEWY__WASH"});
  route("PRIN", "PSC", {"PRIN__NEWY", "NEWY__WASH", "WASH__PSC"});
  route("PRIN", "RUTG", {"PRIN__NEWY", "NEWY__RUTG"});
  route("PRIN", "SALT", {"PRIN__NEWY", "NEWY__STAR", "STAR__SALT"});
  route("PRIN", "SEAT", {"PRIN__NEWY", "NEWY__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__SEAT"});
  route("PRIN", "SRI", {"PRIN__NEWY", "NEWY__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__SRI"});
  route("PRIN", "STAR", {"PRIN__NEWY", "NEWY__STAR"});
  route("PRIN", "TACC", {"PRIN__NEWY", "NEWY__STAR", "STAR__TACC"});
  route("PRIN", "UCSD", {"PRIN__NEWY", "NEWY__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__UCSD"});
  route("PRIN", "UTAH", {"PRIN__NEWY", "NEWY__STAR", "STAR__SALT", "SALT__UTAH"});
  route("PRIN", "WASH", {"PRIN__NEWY", "NEWY__WASH"});
  route("PSC", "RUTG", {"PSC__WASH", "WASH__NEWY", "NEWY__RUTG"});
  route("PSC", "SALT", {"PSC__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__SALT"});
  route("PSC", "SEAT", {"PSC__WASH", "WASH__ATLA", "ATLA__DALL", "DALL__LOSA", "LOSA__SEAT"});
  route("PSC", "SRI", {"PSC__WASH", "WASH__ATLA", "ATLA__DALL", "DALL__LOSA", "LOSA__SRI"});
  route("PSC", "STAR", {"PSC__WASH", "WASH__NEWY", "NEWY__STAR"});
  route("PSC", "TACC", {"PSC__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__TACC"});
  route("PSC", "UCSD", {"PSC__WASH", "WASH__ATLA", "ATLA__DALL", "DALL__LOSA", "LOSA__UCSD"});
  route("PSC", "UTAH", {"PSC__WASH", "WASH__NEWY", "NEWY__STAR", "STAR__SALT", "SALT__UTAH"});
  route("PSC", "WASH", {"PSC__WASH"});
  route("RUTG", "SALT", {"RUTG__NEWY", "NEWY__STAR", "STAR__SALT"});
  route("RUTG", "SEAT", {"RUTG__NEWY", "NEWY__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__SEAT"});
  route("RUTG", "SRI", {"RUTG__NEWY", "NEWY__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__SRI"});
  route("RUTG", "STAR", {"RUTG__NEWY", "NEWY__STAR"});
  route("RUTG", "TACC", {"RUTG__NEWY", "NEWY__STAR", "STAR__TACC"});
  route("RUTG", "UCSD", {"RUTG__NEWY", "NEWY__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__UCSD"});
  route("RUTG", "UTAH", {"RUTG__NEWY", "NEWY__STAR", "STAR__SALT", "SALT__UTAH"});
  route("RUTG", "WASH", {"RUTG__NEWY", "NEWY__WASH"});
  route("SALT", "SEAT", {"SALT__LOSA", "LOSA__SEAT"});
  route("SALT", "SRI", {"SALT__LOSA", "LOSA__SRI"});
  route("SALT", "STAR", {"SALT__STAR"});
  route("SALT", "TACC", {"SALT__STAR", "STAR__TACC"});
  route("SALT", "UCSD", {"SALT__LOSA", "LOSA__UCSD"});
  route("SALT", "UTAH", {"SALT__UTAH"});
  route("SALT", "WASH", {"SALT__STAR", "STAR__NEWY", "NEWY__WASH"});
  route("SEAT", "SRI", {"SEAT__LOSA", "LOSA__SRI"});
  route("SEAT", "STAR", {"SEAT__LOSA", "LOSA__SALT", "SALT__STAR"});
  route("SEAT", "TACC", {"SEAT__LOSA", "LOSA__SALT", "SALT__STAR", "STAR__TACC"});
  route("SEAT", "UCSD", {"SEAT__LOSA", "LOSA__UCSD"});
  route("SEAT", "UTAH", {"SEAT__LOSA", "LOSA__SALT", "SALT__UTAH"});
  route("SEAT", "WASH", {"SEAT__LOSA", "LOSA__DALL", "DALL__ATLA", "ATLA__WASH"});
  route("SRI", "STAR", {"SRI__LOSA", "LOSA__SALT", "SALT__STAR"});
  route("SRI", "TACC", {"SRI__LOSA", "LOSA__SALT", "SALT__STAR", "STAR__TACC"});
  route("SRI", "UCSD", {"SRI__LOSA", "LOSA__UCSD"});
  route("SRI", "UTAH", {"SRI__LOSA", "LOSA__SALT", "SALT__UTAH"});
  route("SRI", "WASH", {"SRI__LOSA", "LOSA__DALL", "DALL__ATLA", "ATLA__WASH"});
  route("STAR", "TACC", {"STAR__TACC"});
  route("STAR", "UCSD", {"STAR__SALT", "SALT__LOSA", "LOSA__UCSD"});
  route("STAR", "UTAH", {"STAR__SALT", "SALT__UTAH"});
  route("STAR", "WASH", {"STAR__NEWY", "NEWY__WASH"});
  route("TACC", "UCSD", {"TACC__STAR", "STAR__SALT", "SALT__LOSA", "LOSA__UCSD"});
  route("TACC", "UTAH", {"TACC__STAR", "STAR__SALT", "SALT__UTAH"});
  route("TACC", "WASH", {"TACC__STAR", "STAR__NEWY", "NEWY__WASH"});
  route("UCSD", "UTAH", {"UCSD__LOSA", "LOSA__SALT", "SALT__UTAH"});
  route("UCSD", "WASH", {"UCSD__LOSA", "LOSA__DALL", "DALL__ATLA", "ATLA__WASH"});
  route("UTAH", "WASH", {"UTAH__SALT", "SALT__STAR", "STAR__NEWY", "NEWY__WASH"});

  root->seal();
}
