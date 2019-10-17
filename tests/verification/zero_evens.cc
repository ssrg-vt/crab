#include "../program_options.hpp"
#include "../common.hpp"

using namespace crab::analyzer;
using namespace crab::cfg;
using namespace crab::cfg_impl;
using namespace crab::domain_impl;

z_cfg_t* prog(variable_factory_t &vfac) {
  // Defining program variables
  z_var array(vfac["array"], crab::ARR_INT_TYPE, 32);
  z_var len(vfac["len"], crab::INT_TYPE, 32);
  z_var i(vfac["i"], crab::INT_TYPE, 32);


  // entry and exit blocks
  auto cfg = new z_cfg_t("entry", "b63c_ret");

  // the blocks themselves
  auto& entry = cfg->insert("entry");
  auto& b60e_614 = cfg->insert("b60e_614");
  auto& b616_62a = cfg->insert("b616_62a"); // Doesn't actually do anything semantically, only exists for cmp+jge
  auto& b630 = cfg->insert("b630");
  auto& b634_63a = cfg->insert("b634_63a"); // Doesn't actually do anything semantically, only exists for cmp+jl
  auto& b63c_ret = cfg->insert("b63c_ret");

  // The control flow
  entry >> b634_63a; b634_63a >> b63c_ret;
  b630 >> b634_63a; b634_63a >> b60e_614; b60e_614 >> b630;
  b60e_614 >> b616_62a; b616_62a >> b630;

  // Program behavior
  entry.assign(i, 0);

  b630.add(i, i, 2);

  b60e_614.assume(operator<(i, len));

  b616_62a.assume(operator<(i, len)); // may not be necessary
  b616_62a.array_store(array, i, 0, 4); // four-byte store of 0

  b63c_ret.assume(operator>=(i, len));

  return cfg;
}

int main(int argc, char** argv) {
  bool stats_enabled = false;
  if (!crab_tests::parse_user_options(argc, argv, stats_enabled)) {
      return 1;
  }

  variable_factory_t vfac;
  z_cfg_t* cfg = prog(vfac);
  cfg->simplify(); // this is optional
  crab::outs() << *cfg << "\n";

  // TODO: none of these provide information about array!
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
