/* Platform test: data flows from node-0.inst to node-0.cori via DTN nodes
 *
 * Intended path (mirrors esnet.json routes):
 *
 *   node-0.inst
 *       | stream: inst_to_ffb        (instrument-to-reduction + reduction-to-ffb links)
 *   ffb_server                       (SLAC fast-feedback storage)
 *       | stream: ffb_to_slac_dtn    (ffb-to-slac-dtn link)
 *   node-0.slack-dtn
 *       | stream: wan                (slac-to-nersc WAN link)
 *   node-0.cori-dtn
 *       | stream: cori_dtn_to_scratch (scratch-to-cori-dtn link)
 *   scratch_server                   (NERSC scratch storage)
 *       | stream: scratch_to_cori    (cori-to-scratch link)
 *   node-0.cori
 */

#include <fstream>
#include <iostream>
#include <string>

#include <dtlmod/DTL.hpp>
#include <nlohmann/json.hpp>
#include <simgrid/s4u.hpp>

XBT_LOG_NEW_DEFAULT_CATEGORY(test_platform, "Platform test");

namespace sg4 = simgrid::s4u;

// ─── Shared constants ─────────────────────────────────────────────────────────

static constexpr const char* VAR_NAME  = "data";
static constexpr size_t      VAR_BYTES = 1024 * 1024; // 1 MiB per step

// Stream names — must match what is declared in the DTL config
static constexpr const char* S_INST_TO_FFB        = "inst_to_ffb";
static constexpr const char* S_FFB_TO_SLAC_DTN    = "ffb_to_slac_dtn";
static constexpr const char* S_WAN                = "wan";
static constexpr const char* S_CORI_DTN_TO_SCRATCH = "cori_dtn_to_scratch";
static constexpr const char* S_SCRATCH_TO_CORI    = "scratch_to_cori";
static constexpr const char* S_INST_TO_CORI          = "inst_to_cori"; // for direct path (not used in this test)

// ─── Helper: open + define variable for publishing ────────────────────────────

static std::pair<std::shared_ptr<dtlmod::Engine>, std::shared_ptr<dtlmod::Variable>>
open_pub(std::shared_ptr<dtlmod::DTL> dtl, const char* stream_name)
{
  const auto& stream = dtl->get_stream_by_name(stream_name).value_or(nullptr);
  if (not stream)
    xbt_die("Unknown stream '%s'", stream_name);
  auto engine = stream->open("file", dtlmod::Stream::Mode::Publish);
  auto var    = stream->define_variable(VAR_NAME, {VAR_BYTES}, {0}, {VAR_BYTES}, sizeof(char));
  return {engine, var};
}

// ─── Helper: open + inquire variable for subscribing ─────────────────────────

static std::pair<std::shared_ptr<dtlmod::Engine>, std::shared_ptr<dtlmod::Variable>>
open_sub(std::shared_ptr<dtlmod::DTL> dtl, const char* stream_name)
{
  const auto& stream = dtl->get_stream_by_name(stream_name).value_or(nullptr);
  if (not stream)
    xbt_die("Unknown stream '%s'", stream_name);
  auto engine = stream->open("file", dtlmod::Stream::Mode::Subscribe);
  auto var    = stream->inquire_variable(VAR_NAME);
  var->set_selection({0}, {VAR_BYTES});
  return {engine, var};
}

// ─── Helper: single pub/sub step ─────────────────────────────────────────────

static void do_put(std::shared_ptr<dtlmod::Engine> engine, std::shared_ptr<dtlmod::Variable> var, int step,
                   const char* stream_name)
{
  XBT_INFO("step %d — put %zu B -> '%s'", step, VAR_BYTES, stream_name);
  engine->begin_transaction();
  engine->put(var, VAR_BYTES);
  engine->end_transaction();
}

static void do_get(std::shared_ptr<dtlmod::Engine> engine, std::shared_ptr<dtlmod::Variable> var, int step,
                   const char* stream_name)
{
  engine->begin_transaction();
  engine->get(var);
  XBT_INFO("step %d — got %zu B <- '%s'", step, VAR_BYTES, stream_name);
  engine->end_transaction();
}

// ─── Actor: instrument node (source) ─────────────────────────────────────────
// Publishes to inst_to_ffb

static void act_instrument(int nsteps)
{
  auto dtl           = dtlmod::DTL::connect();
  // auto [engine, var] = open_pub(dtl, S_INST_TO_FFB);
  auto [engine, var] = open_pub(dtl, S_INST_TO_CORI);

  for (int s = 1; s <= nsteps; s++)
    // do_put(engine, var, s, S_INST_TO_FFB);
    do_put(engine, var, s, S_INST_TO_CORI);

  engine->close();
  dtlmod::DTL::disconnect();
}

// ─── Actor: FFB storage node ──────────────────────────────────────────────────
// Subscribes from inst_to_ffb, republishes to ffb_to_slac_dtn

static void act_ffb(int nsteps)
{
  auto dtl              = dtlmod::DTL::connect();
  auto [sub_e, sub_v]   = open_sub(dtl, S_INST_TO_FFB);
  auto [pub_e, pub_v]   = open_pub(dtl, S_FFB_TO_SLAC_DTN);

  for (int s = 1; s <= nsteps; s++) {
    do_get(sub_e, sub_v, s, S_INST_TO_FFB);
    do_put(pub_e, pub_v, s, S_FFB_TO_SLAC_DTN);
  }

  sub_e->close();
  pub_e->close();
  dtlmod::DTL::disconnect();
}

// ─── Actor: SLAC DTN ──────────────────────────────────────────────────────────
// Subscribes from ffb_to_slac_dtn, republishes over the WAN

static void act_slac_dtn(int nsteps)
{
  auto dtl            = dtlmod::DTL::connect();
  auto [sub_e, sub_v] = open_sub(dtl, S_FFB_TO_SLAC_DTN);
  auto [pub_e, pub_v] = open_pub(dtl, S_WAN);

  for (int s = 1; s <= nsteps; s++) {
    do_get(sub_e, sub_v, s, S_FFB_TO_SLAC_DTN);
    do_put(pub_e, pub_v, s, S_WAN);
  }

  sub_e->close();
  pub_e->close();
  dtlmod::DTL::disconnect();
}

// ─── Actor: NERSC DTN ─────────────────────────────────────────────────────────
// Subscribes from WAN, republishes to cori_dtn_to_scratch

static void act_cori_dtn(int nsteps)
{
  auto dtl            = dtlmod::DTL::connect();
  auto [sub_e, sub_v] = open_sub(dtl, S_WAN);
  auto [pub_e, pub_v] = open_pub(dtl, S_CORI_DTN_TO_SCRATCH);

  for (int s = 1; s <= nsteps; s++) {
    do_get(sub_e, sub_v, s, S_WAN);
    do_put(pub_e, pub_v, s, S_CORI_DTN_TO_SCRATCH);
  }

  sub_e->close();
  pub_e->close();
  dtlmod::DTL::disconnect();
}

// ─── Actor: scratch storage node ─────────────────────────────────────────────
// Subscribes from cori_dtn_to_scratch, republishes to scratch_to_cori

static void act_scratch(int nsteps)
{
  auto dtl            = dtlmod::DTL::connect();
  auto [sub_e, sub_v] = open_sub(dtl, S_CORI_DTN_TO_SCRATCH);
  auto [pub_e, pub_v] = open_pub(dtl, S_SCRATCH_TO_CORI);

  for (int s = 1; s <= nsteps; s++) {
    do_get(sub_e, sub_v, s, S_CORI_DTN_TO_SCRATCH);
    do_put(pub_e, pub_v, s, S_SCRATCH_TO_CORI);
  }

  sub_e->close();
  pub_e->close();
  dtlmod::DTL::disconnect();
}

// ─── Actor: Cori compute node (destination) ───────────────────────────────────
// Subscribes from scratch_to_cori

static void act_cori(int nsteps)
{
  auto dtl           = dtlmod::DTL::connect();
  // auto [engine, var] = open_sub(dtl, S_SCRATCH_TO_CORI);
  auto [engine, var] = open_sub(dtl, S_INST_TO_CORI);

  for (int s = 1; s <= nsteps; s++)
    // do_get(engine, var, s, S_SCRATCH_TO_CORI);
    do_get(engine, var, s, S_INST_TO_CORI);

  engine->close();
  dtlmod::DTL::disconnect();
}

// ─── Main ─────────────────────────────────────────────────────────────────────

int main(int argc, char** argv)
{
  if (argc < 4) {
    std::cerr << "Usage: " << argv[0]
              << " <platform.json> <libplatform.so> <dtl_cfg.json> [nsteps]\n"
              << "  nsteps defaults to 3\n";
    return 1;
  }

  sg4::Engine e(&argc, argv);

  xbt_log_control_set("root.fmt:[%10.3r]%e[%12a]%e[%12h]%e%m%n");
  xbt_log_control_set("no_loc");
  xbt_log_control_set("test_platform.thresh:debug");

  setenv("PLATFORM_CONFIG", argv[1], /*overwrite=*/1);
  e.load_platform(argv[2]);
  dtlmod::DTL::create(argv[3]);

  const int nsteps = (argc >= 5) ? std::stoi(argv[4]) : 3;

  // ── Print hosts ────────────────────────────────────────────────────────────
  XBT_INFO("=== Platform hosts ===");
  for (const auto* host : e.get_all_hosts())
    XBT_INFO("  host: %-30s  speed: %.0f Gf  cores: %d",
             host->get_name().c_str(), host->get_speed() / 1e9, host->get_core_count());

  // ── Print links ────────────────────────────────────────────────────────────
  XBT_INFO("=== Platform links ===");
  for (const auto* link : e.get_all_links())
    XBT_INFO("  link: %-35s  bw: %.3f Gbps  lat: %.3f ms",
             link->get_name().c_str(), link->get_bandwidth() / 1e9, link->get_latency() * 1e3);

  // ── Resolve hosts ──────────────────────────────────────────────────────────
  // Each actor is placed on the host that owns that hop in the data path
  const std::vector<std::pair<std::string, std::string>> host_actors = {
      {"node-0.inst",       "instrument"},
      {"ffb_server",        "ffb"},
      {"node-0.slack-dtn",  "slac_dtn"},
      {"node-0.cori-dtn",   "cori_dtn"},
      {"scratch_server",    "scratch"},
      {"node-0.cori",       "cori"},
  };

  for (const auto& [hostname, _] : host_actors) {
    if (!e.host_by_name_or_null(hostname)) {
      XBT_ERROR("Host '%s' not found — check platform JSON", hostname.c_str());
      return 1;
    }
  }

  // ── Launch actors ──────────────────────────────────────────────────────────
  XBT_INFO("=== Starting pipeline: node-0.inst -> ffb -> slac_dtn -[WAN]-> cori_dtn -> scratch -> node-0.cori (%d step(s), %zu B/step) ===",
           nsteps, VAR_BYTES);

  e.host_by_name("node-0.inst"     )->add_actor("instrument", [nsteps]() { act_instrument(nsteps); });
//   e.host_by_name("ffb_server"      )->add_actor("ffb",        [nsteps]() { act_ffb(nsteps);        });
//   e.host_by_name("node-0.slack-dtn")->add_actor("slac_dtn",   [nsteps]() { act_slac_dtn(nsteps);   });
//   e.host_by_name("node-0.cori-dtn" )->add_actor("cori_dtn",   [nsteps]() { act_cori_dtn(nsteps);   });
//   e.host_by_name("scratch_server"  )->add_actor("scratch",    [nsteps]() { act_scratch(nsteps);    });
  e.host_by_name("node-0.cori"     )->add_actor("cori",       [nsteps]() { act_cori(nsteps);       });

  e.run();

  XBT_INFO("=== Simulation finished at t=%.3f s ===", e.get_clock());
  return 0;
}
