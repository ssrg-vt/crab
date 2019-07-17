#pragma once 

#include <crab/cfg/cfg.hpp>
#include <crab/cfg/var_factory.hpp>
#include <crab/iterators/interleaved_fixpoint_iterator.hpp>
#include <crab/analysis/abs_transformer.hpp>
#include <crab/analysis/inter_fwd_analyzer_ds.hpp>
#include <crab/analysis/dataflow/liveness.hpp>
#include <crab/domains/abstract_domain_specialized_traits.hpp>

#include "boost/range/algorithm/set_algorithm.hpp"
#include "boost/shared_ptr.hpp"

namespace crab {
namespace analyzer {
namespace analyzer_internal_impl {
/**
 * Only for crab internal use.
 * Implementation of an intra-procedural forward analysis.
 * 
 * Perform a standard forward flow-sensitive analysis. AbsTr
 * defines the abstract transfer functions as well as which
 * operations are modeled.
 **/
template< typename CFG, typename AbsTr>
class fwd_analyzer: 
    private ikos::interleaved_fwd_fixpoint_iterator
            <CFG, typename AbsTr::abs_dom_t> { 
public:
  
  typedef CFG cfg_t;
  typedef typename CFG::basic_block_label_t basic_block_label_t;
  typedef typename CFG::varname_t varname_t;
  typedef typename CFG::variable_t variable_t;
  typedef typename CFG::number_t number_t;
  typedef typename CFG::statement_t stmt_t;   
  typedef typename AbsTr::abs_dom_t abs_dom_t;
  typedef AbsTr abs_tr_t;
  
private:
  
  typedef ikos::interleaved_fwd_fixpoint_iterator<CFG, abs_dom_t>
  fixpo_iterator_t;
  
public:
  
  typedef typename fixpo_iterator_t::invariant_table_t invariant_map_t;      
  typedef typename fixpo_iterator_t::assumption_map_t assumption_map_t;
  typedef liveness<CFG> liveness_t;
  typedef typename fixpo_iterator_t::wto_t wto_t;
  typedef typename fixpo_iterator_t::iterator iterator;
  typedef typename fixpo_iterator_t::const_iterator const_iterator;
  
private:
  
  typedef typename liveness_t::set_t live_set_t;     

  boost::shared_ptr<abs_tr_t>  m_abs_tr;  // the abstract transformer
  const liveness_t* m_live;
  live_set_t m_formals;
  bool m_pre_clear_done;
  bool m_post_clear_done;      
  
  abs_dom_t prune_dead_variables(basic_block_label_t node, abs_dom_t&& inv) {
    if (!m_live) {
      return std::move(inv);
    }
    crab::ScopedCrabStats __st__("Pruning dead variables");	
    if (inv.is_bottom() || inv.is_top()) {
      return std::move(inv);
    }	
    auto dead = m_live->dead_exit(node);       
    dead -= m_formals;
    std::vector<variable_t> dead_vec(dead.begin(), dead.end());
    inv.forget(dead_vec);
    return std::move(inv);
  }
  
  //! Given a basic block and the invariant at the entry it produces
  //! the invariant at the exit of the block.
  abs_dom_t analyze(basic_block_label_t node, abs_dom_t&& inv) {
    auto &b = this->get_cfg().get_node(node);
    m_abs_tr->set_abs_value(std::move(inv));
    for (auto &s : b) { s.accept(&*m_abs_tr); }
    inv = std::move(m_abs_tr->get_abs_value());
    inv = std::move(prune_dead_variables(node, std::move(inv)));
    return std::move(inv);
  } 
  
  void process_pre(basic_block_label_t node, abs_dom_t inv) {}
  void process_post(basic_block_label_t node, abs_dom_t inv) {}
  
public:
  
  fwd_analyzer(CFG cfg, const wto_t *wto, boost::shared_ptr<abs_tr_t> abs_tr,
	       // live can be nullptr if no live info is available
	       const liveness_t* live,
	       // fixpoint parameters
	       unsigned int widening_delay,
	       unsigned int descending_iters,
	       size_t jump_set_size)
    
    : fixpo_iterator_t(cfg, wto,
		       widening_delay, descending_iters, jump_set_size,
		       false /*disable processor*/)
    , m_abs_tr(abs_tr)
    , m_live(live)
    , m_pre_clear_done(false)
    , m_post_clear_done(false) {
    CRAB_VERBOSE_IF(1, get_msg_stream() << "Type checking CFG ... ";);
    crab::CrabStats::resume("CFG type checking");
    crab::cfg::type_checker<CFG> tc(this->_cfg);
    tc.run();
    crab::CrabStats::stop("CFG type checking");	
    CRAB_VERBOSE_IF(1, get_msg_stream() << "OK\n";);
    
    if (live) {
      // --- collect input and output parameters 
      if (this->get_cfg().has_func_decl()) {
	auto fdecl = this->get_cfg().get_func_decl();
	for (unsigned i=0; i < fdecl.get_num_inputs();i++)
	  m_formals += fdecl.get_input_name(i);
	for (unsigned i=0; i < fdecl.get_num_outputs();i++)
	  m_formals += fdecl.get_output_name(i);
      }
    }
  }
  
  //! Trigger the fixpoint computation 
  void run_forward()  {
    // ugly hook to initialize some global state. This needs to be
    // fixed properly.
    domains::array_sgraph_domain_traits<abs_dom_t>::do_initialization(this->get_cfg());
    // XXX: inv was created before the static data is initialized
    //      so it won't contain that data.
    this->run(m_abs_tr->get_abs_value());         
  }      

  void run_forward(basic_block_label_t entry, const assumption_map_t &assumptions)  {
    // ugly hook to initialize some global state. This needs to be
    // fixed properly.
    domains::array_sgraph_domain_traits<abs_dom_t>::do_initialization(this->get_cfg());
    // XXX: inv was created before the static data is initialized
    //      so it won't contain that data.	
    this->run(entry, m_abs_tr->get_abs_value(), assumptions);         
  }      
    
  const invariant_map_t& get_pre_invariants() const {
    return this->_pre;
  }
  
  const invariant_map_t& get_post_invariants() const {
    return this->_post;
  }

  invariant_map_t& get_pre_invariants() {
    return this->_pre;
  }
  
  invariant_map_t& get_post_invariants() {
    return this->_post;
  }
  
  iterator       pre_begin()       { return this->_pre.begin(); } 
  iterator       pre_end()         { return this->_pre.end();   }
  const_iterator pre_begin() const { return this->_pre.begin(); }
  const_iterator pre_end()   const { return this->_pre.end();   }
  
  iterator       post_begin()       { return this->_post.begin(); } 
  iterator       post_end()         { return this->_post.end();   }
  const_iterator post_begin() const { return this->_post.begin(); }
  const_iterator post_end()   const { return this->_post.end();   }
  
  //! Return the invariants that hold at the entry of b
  inline abs_dom_t operator[](basic_block_label_t b) const {
    return get_pre(b);
  }
  
  //! Return the invariants that hold at the entry of b
  abs_dom_t get_pre(basic_block_label_t b) const {
    if (m_pre_clear_done) {
      return abs_dom_t::top();
    }
	
    auto it = this->_pre.find(b);
    if (it == this->_pre.end()) {
      return abs_dom_t::bottom();
      // if the basic block is not in the invariant table it must
      // be because it was not reached by the analysis. We
      // returned top but it never had real effect because
      // process_pre made sure that all unreachable blocks were in
      // the invariant table with a bottom invariant. This was
      // just a waste of space.
      // 
      // return abs_dom_t::top();
    } else {
      return it->second;
    }
  }
  
  //! Return the invariants that hold at the exit of b
  abs_dom_t get_post(basic_block_label_t b) const {
    if (m_post_clear_done) {
      return abs_dom_t::top();
    }
    
    auto it = this->_post.find(b);
    if (it == this->_post.end()) {
      return abs_dom_t::bottom();	  
      //return abs_dom_t::top();
    } else {
      return it->second;
    }
  }
  
  //! Return the WTO of the CFG. The WTO contains also how many
  //! times each head was visited by the fixpoint iterator.
  const wto_t& get_wto() const {
    return fixpo_iterator_t::get_wto();
  }
  
  void clear_pre() {
    m_pre_clear_done = true;
    this->_pre.clear();
  }
  
  void clear_post() {
    m_post_clear_done = true;
    this->_post.clear();
  }
  
  // clear all invariants (pre and post)
  void clear() {
    clear_pre();
    clear_post();
  }

  void reset() {
    clear();
    m_pre_clear_done = false;
    m_post_clear_done = false;
  }

  /** Begin extra API for checkers **/

  CFG get_cfg() const {
    return this->_cfg;
  }
  
  boost::shared_ptr<abs_tr_t> get_abs_transformer(abs_dom_t&& inv) {
    m_abs_tr->set_abs_value(std::move(inv));
    return m_abs_tr;
  }
  
  void get_safe_assertions(std::set<const stmt_t*>& out) const {}

  /** End extra API for checkers **/
  
  void write(crab::crab_os& o) {
    for (auto& kv: this->_pre) {
      auto& pre = kv.second;
      auto& b = this->get_cfg().get_node(kv.first);      
      o << cfg_impl::get_label_str(kv.first) << ":\n";
      if (b.size() == 0) {
	o << "/**\n"
	  << "* " << pre << "\n"
	  << "**/\n";	
      } else {
	auto post = get_post(kv.first);
	o << "**\n"
	  << "* PRE: " << pre << "\n"
	  << "**/\n";	
	for (auto &s: b) {
	  o << "\t" << s << "\n";
	}
	o << "/**\n"
	  << "* POST: " << post << "\n"
	  << "**/\n";	
      }
    }
  }
  
}; 

/**
 * Wrapper for fwd_analyzer_class. 
 * 
 * The main difference with fwd_analyzer class is that here we
 * create an abstract transformer instance while fwd_analyzer does
 * not.
 **/
template<typename CFG, typename AbsDomain, typename AbsTr>
    class intra_fwd_analyzer_wrapper {
  typedef fwd_analyzer<CFG, AbsTr> fwd_analyzer_t;
  
public:
  
  typedef AbsDomain abs_dom_t;
  typedef liveness<CFG> liveness_t;
  typedef CFG cfg_t;
  typedef typename CFG::basic_block_label_t basic_block_label_t;
  typedef typename CFG::varname_t varname_t;
  typedef typename CFG::number_t number_t;
  typedef typename CFG::statement_t stmt_t;                   
  typedef typename fwd_analyzer_t::abs_tr_t abs_tr_t;
  typedef typename fwd_analyzer_t::wto_t wto_t;      
  typedef typename fwd_analyzer_t::assumption_map_t assumption_map_t;
  typedef typename fwd_analyzer_t::invariant_map_t invariant_map_t;
  typedef typename fwd_analyzer_t::iterator iterator;
  typedef typename fwd_analyzer_t::const_iterator const_iterator;
  
private:
  
  abs_dom_t m_init;      
  boost::shared_ptr<abs_tr_t> m_abs_tr;
  fwd_analyzer_t m_analyzer;
  
public:
  
  intra_fwd_analyzer_wrapper(CFG cfg, 
			     // fixpoint parameters
			     unsigned int widening_delay=1,
			     unsigned int descending_iters=UINT_MAX,
			     size_t jump_set_size=0)
    : m_init(AbsDomain::top()),
      m_abs_tr(boost::make_shared<abs_tr_t>(m_init)),
      m_analyzer(cfg, nullptr, m_abs_tr, nullptr,
		 widening_delay, descending_iters, jump_set_size) {}
  
  intra_fwd_analyzer_wrapper(CFG cfg,
			     AbsDomain init,
			     // live variables
			     const liveness_t* live = nullptr,
			     // avoid precompute wto if already available
			     const wto_t *wto = nullptr, 
			     // fixpoint parameters
			     unsigned int widening_delay=1,
			     unsigned int descending_iters=UINT_MAX,
			     size_t jump_set_size=0)
    : m_init(init),
      m_abs_tr(boost::make_shared<abs_tr_t>(m_init)),	  	  
      m_analyzer(cfg, wto, m_abs_tr, live, 
		 widening_delay, descending_iters, jump_set_size) {}
  
  void run() {
    m_analyzer.run_forward();
  }
  
  void run(basic_block_label_t entry, const assumption_map_t &assumptions) {
    m_analyzer.run_forward(entry, assumptions);
  }
  
  iterator       pre_begin()       { return m_analyzer.pre_begin();} 
  iterator       pre_end()         { return m_analyzer.pre_end();}
  const_iterator pre_begin() const { return m_analyzer.pre_begin();}
  const_iterator pre_end()   const { return m_analyzer.pre_end();}
  
  iterator       post_begin()       { return m_analyzer.post_begin();}
  iterator       post_end()         { return m_analyzer.post_end();}
  const_iterator post_begin() const { return m_analyzer.post_begin();}
  const_iterator post_end()   const { return m_analyzer.post_end();}
  
  const invariant_map_t& get_pre_invariants() const {
    return m_analyzer.get_pre_invariants();
  }

  invariant_map_t& get_pre_invariants() {
    return m_analyzer.get_pre_invariants();
  }

  const invariant_map_t& get_post_invariants() const {
    return m_analyzer.get_post_invariants();
  }

  invariant_map_t& get_post_invariants() {
    return m_analyzer.get_post_invariants();
  }
  
  abs_dom_t operator[](basic_block_label_t b) const {
    return m_analyzer[b];
  }
  
  abs_dom_t get_pre(basic_block_label_t b) const {
    return m_analyzer.get_pre(b);
  }
  
  abs_dom_t get_post(basic_block_label_t b) const {
    return m_analyzer.get_post(b);
  }
  
  
  const wto_t& get_wto() const {
    return m_analyzer.get_wto();
  }
  
  void clear_pre() {
    m_analyzer.clear_pre();
  }
  
  void clear_post() {
    m_analyzer.clear_post();	
  }
  
  void clear() {
    clear_pre();
    clear_post();
  }

  void reset() {
    m_analyzer.reset();
  }
  
  /** Begin extra API for checkers **/
  
  CFG get_cfg() {
    return m_analyzer.get_cfg();
  }
  
  boost::shared_ptr<abs_tr_t> get_abs_transformer(abs_dom_t&& inv) {
    m_abs_tr->set_abs_value(std::move(inv));
    return m_abs_tr;
  }
  
  void get_safe_assertions(std::set<const stmt_t*>& out) const {}

  /** End extra API for checkers **/
  
  void write(crab::crab_os& o) const {
    m_analyzer.write(o);
  }
  
};
} // end namespace analyzer_impl

/**
 * External api
 **/
template<typename CFG, typename AbsDomain>
using intra_fwd_analyzer = analyzer_internal_impl::
intra_fwd_analyzer_wrapper<CFG, AbsDomain, intra_abs_transformer<AbsDomain>>;

} // end namespace
} // end namespace

