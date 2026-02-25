/* Simple platform test: print hosts and send a message between two hosts */

#include <fstream>
#include <iostream>
#include <string>

#include <nlohmann/json.hpp>
#include <simgrid/s4u.hpp>

XBT_LOG_NEW_DEFAULT_CATEGORY(test_platform, "Platform test");

namespace sg4 = simgrid::s4u;

// ─── Actors ───────────────────────────────────────────────────────────────────

static void sender(const std::string& dst_host, const std::string& msg)
{
  XBT_INFO("Sending \"%s\" to %s", msg.c_str(), dst_host.c_str());
  auto mailbox = sg4::Mailbox::by_name(dst_host);
  mailbox->put(new std::string(msg), msg.size());
  XBT_INFO("Message sent.");
}

static void receiver()
{
  auto mailbox = sg4::Mailbox::by_name(sg4::this_actor::get_host()->get_name());
  auto* msg    = mailbox->get<std::string>();
  XBT_INFO("Received: \"%s\"", msg->c_str());
  delete msg;
}

// ─── Main ─────────────────────────────────────────────────────────────────────

int main(int argc, char** argv)
{
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <platform.json> <libplatform.so> [src_host] [dst_host]\n"
              << "  src_host defaults to node-0.inst  (first SLAC instrument node)\n"
              << "  dst_host defaults to node-0.cori  (first NERSC cori node)\n";
    return 1;
  }

  sg4::Engine e(&argc, argv);

  xbt_log_control_set("root.fmt:[%10.3r]%e[%12a]%e[%12h]%e%m%n");
  xbt_log_control_set("no_loc");
  xbt_log_control_set("test_platform.thresh:debug");

  // Pass the JSON path to the loader via env var, then load the .so
  setenv("PLATFORM_CONFIG", argv[1], /*overwrite=*/1);
  e.load_platform(argv[2]);

  // ── 1. Print all hosts ─────────────────────────────────────────────────────
  XBT_INFO("=== Platform hosts ===");
  for (const auto* host : e.get_all_hosts()) {
    XBT_INFO("  host: %-30s  speed: %.0f Gf  cores: %d",
             host->get_name().c_str(),
             host->get_speed() / 1e9,
             host->get_core_count());
  }

  // ── 2. Print all links ─────────────────────────────────────────────────────
  XBT_INFO("=== Platform links ===");
  for (const auto* link : e.get_all_links()) {
    XBT_INFO("  link: %-35s  bw: %.3f Gbps  lat: %.3f ms",
             link->get_name().c_str(),
             link->get_bandwidth() / 1e9,
             link->get_latency() * 1e3);
  }

  // ── 3. Send a message from src_host → dst_host ────────────────────────────
  const std::string src_name = (argc >= 4) ? argv[3] : "node-0.inst";
  const std::string dst_name = (argc >= 5) ? argv[4] : "node-0.cori";

  auto* src = e.host_by_name_or_null(src_name);
  auto* dst = e.host_by_name_or_null(dst_name);

  if (!src) {
    XBT_ERROR("Source host '%s' not found in platform. Check host names with the list above.", src_name.c_str());
    return 1;
  }
  if (!dst) {
    XBT_ERROR("Destination host '%s' not found in platform. Check host names with the list above.", dst_name.c_str());
    return 1;
  }

  XBT_INFO("=== Messaging: %s -> %s ===", src_name.c_str(), dst_name.c_str());

  // Print the route between the two hosts before running
  std::vector<sg4::Link*> route;
  double latency = 0.0;
  src->route_to(dst, route, &latency);
  XBT_INFO("Route has %zu link(s), total latency: %.3f ms", route.size(), latency * 1e3);
  for (const auto* l : route) {
    XBT_INFO("  -> %s (%.3f Gbps)", l->get_name().c_str(), l->get_bandwidth() / 1e9);
  }

  const std::string message = "Hello from " + src_name + "!";
  dst->add_actor("receiver", receiver);
  src->add_actor("sender",   [dst_name, message]() { sender(dst_name, message); });

  e.run();

  XBT_INFO("=== Simulation finished at t=%.3f s ===", e.get_clock());
  return 0;
}