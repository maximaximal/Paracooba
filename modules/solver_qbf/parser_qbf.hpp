#pragma once

#include <list>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include <boost/pool/object_pool.hpp>

#include <paracooba/common/status.h>

#include "qbf_types.hpp"

namespace parac::solver_qbf {
class Parser {
  public:
  Parser();
  ~Parser();

  using Literals = std::vector<Literal>;

  parac_status parse(std::string_view input);

  const Literals& literals() const { return m_literals; }

  private:
  struct Internal;

  std::vector<Literal> m_literals;

  std::string m_path;
  std::optional<std::string> m_fileToDelete;

  parac_status processInputToPath(std::string_view input);

  /* ================== PARSING ================== */
  typedef unsigned long long Sig;
  struct Clause;
  struct Node;
  struct Scope;
  struct Var;

  struct Anchor { /* list anchor for forward watches */
    int count;
    Clause *first, *last;
  };

  struct Occ { /* occurrence list anchor */
    int count;
    Node *first, *last;
  };

  enum Tag {
    FREE = 0,
    UNET = 1,
    UNATE = 2,
    FIXED = 3,
    ZOMBIE = 4,
    ELIMINATED = 5,
    SUBSTITUTED = 6,
    EXPANDED = 7,
    SOLVING = 8,
    FORALL_RED = 9,
    UNIT = 10,
  };

  struct Var {
    Scope* scope;
    Tag tag;
    int lmark, lmark2;             /* marks for lookup */
    int mark, mark2, mark3, mark4; /* primary mark flag */
    int submark;                   /* second subsumption mark flag */
    int mapped;                    /* index mapped to */
    int fixed;                     /* assignment */
    int score, pos;                /* for elimination priority queue */
    int expcopy;                   /* copy in expansion */
    Var *prev, *next;              /* scope variable list links */
    Occ occs[2];                   /* positive and negative occurrence lists */
  };

  struct Node { /* one 'lit' occurrence in a 'clause' */
    int lit;
    int blocked; /* 1 if used as pivot of blocked clause */
    Clause* clause;
    Node *prev, *next; /* links all occurrences of 'lit' */
  };

  struct Watch {         /* watch for forward subsumption */
    int idx;             /* watched 'idx' */
    Clause *prev, *next; /* links for watches of 'idx' */
  };

  struct Clause {
    int size = 0;
    int mark = 0, cmark2 = 0;
    int count = 0;
    Sig sig = 0; /* subsumption/strengthening signature */
    Clause *prev = nullptr,
           *next = nullptr; /* chronlogical clause list links */
    Clause *head = nullptr,
           *tail = nullptr;             /* backward subsumption queue links */
    Watch watch{ 0, nullptr, nullptr }; /* forward subsumption watch */
    std::vector<Node> nodes;            /* embedded literal nodes */
  };

  struct Scope {
    int type;          /* type>0 == existential, type<0 universal */
    int order;         /* scope order: outer most = 0 */
    int free;          /* remaining free variables */
    int stretch;       /* stretches to this scope */
    Var *first, *last; /* variable list */
    Scope *outer, *inner;
  };

  int lineno = 1;
  int universal_vars = 0, existential_vars = 0, implicit_vars = 0;
  int nline = 0, szline = 0;
  int remaining = 0;
  int num_vars = 0, num_lits = 0;
  int size_lits;
  int remaining_clauses_to_parse = 0;
  int orig_clauses;
  int orig_num_vars;
  int units, unates, unets, zombies, eliminated, mapped, outermost_blocked;
  int assigned_scope = -1;
  std::string line;

  using IntVec = std::vector<int>;

  IntVec dfsi, mindfsi, repr, subst_vals, lits;
  int *dfsi_ptr, *mindfsi_ptr, *repr_ptr;
  std::vector<IntVec*> clause_stack;
  int clause_stack_size, clause_stack_top;
  std::vector<Anchor> anchors;
  std::vector<Var> vars;
  std::vector<Sig> fwsigs, bwsigs;
  IntVec trail;
  int *top_of_trail, *next_on_trail;
  IntVec schedule;
  int size_schedule;
  Scope *inner_most_scope = nullptr, *outer_most_scope = nullptr;
  Clause *first_clause, *last_clause, *empty_clause, *queue;

  boost::object_pool<Scope> scopes;

  std::list<Clause> clauses;

  bool force = false;

  IntVec stack;
  const char* parse(FILE* ifile);
  Var* lit2var(int lit);
  Occ* lit2occ(int lit);
  Scope* lit2scope(int lit);
  void init_variables(int start);
  void init_implicit_scope();
  void add_outer_most_scope();
  void add_var(int idx, Scope* scope);
  void push_literal(int lit);
  void enlarge_lits();
  void add_quantifier(int lit);
  void add_clause();
  Sig lit2sig(int lit);
  Sig sig_lits();
  void add_node(Clause* clause, Node* node, int lit);

  template<typename T>
  inline void NEWN(T& vec, size_t size) {
    vec.resize(size);
  }
  template<typename T>
  inline void RSZ(T& vec, size_t newsize) {
    vec.resize(newsize);
  }
};
}
