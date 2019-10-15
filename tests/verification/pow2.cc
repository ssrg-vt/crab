#include "../program_options.hpp"
#include "../common.hpp"

// uint64_t pow2(uint32_t exponent) {
// 	uint64_t a = 1;

// 	for (uint32_t i = 0; i < exponent; ++i) {
// 		a += a;
// 	}

// 	return a;
// }

// TODO: array modification, incrementing index by 2 each iteration
// TODO: linked list operations (what do we get?)
// TODO: pointer operations (array-based heap?)

using namespace crab::analyzer;
using namespace crab::cfg;
using namespace crab::cfg_impl;
using namespace crab::domain_impl;

z_cfg_t* prog (variable_factory_t &vfac) {
  // Defining program variables
  z_var exponent(vfac["exponent"], crab::INT_TYPE, 32);
  z_var a(vfac["a"], crab::INT_TYPE, 64);
  z_var i(vfac["i"], crab::INT_TYPE, 32);

  // entry and exit blocks
  auto cfg = new z_cfg_t("start", "ret"); // Want to return without copying/moving

  // the blocks themselves
  auto& start = cfg->insert("start");
  auto& loop = cfg->insert("loop");
  auto& ret = cfg->insert("ret");
  
  // The control flow
  // TODO: may need revision
  start >> loop;
  loop >> loop;
  loop >> ret;

  // Program behavior
  start.assign(a, 1);
  start.assign(i, 0);

  // Have to explicitly use the operator functions with two vars for some reason
  loop.assume(operator<(i, exponent));
  loop.add(a, a, a);
  loop.add(i, i, 1);

  ret.assume(operator>=(i, exponent));

  return cfg;
}

/* Example of how to infer invariants from the above CFG */
int main (int argc, char** argv) {
  bool stats_enabled = false;
  if (!crab_tests::parse_user_options(argc, argv, stats_enabled)) {
      return 1;
  }

  variable_factory_t vfac;
  z_cfg_t* cfg = prog(vfac);
  cfg->simplify(); // this is optional
  crab::outs() << *cfg << "\n";

  std::string entry = cfg->entry();
  run<z_interval_domain_t>(cfg, entry, false, 1, 2, 20, stats_enabled);
  run<z_dbm_domain_t>(cfg, entry, false, 1, 2, 20, stats_enabled);
  run<z_sdbm_domain_t>(cfg, entry, false, 1, 2, 20, stats_enabled);
  run<z_ric_domain_t>(cfg, entry, false, 1, 2, 20, stats_enabled);
  run<z_term_domain_t>(cfg, entry, false, 1, 2, 20, stats_enabled);
  run<z_dis_interval_domain_t>(cfg, entry, false, 1, 2, 20, stats_enabled);

  // free the CFG
  delete cfg;

  return 0;
}
