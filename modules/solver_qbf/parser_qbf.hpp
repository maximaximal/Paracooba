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

  struct Quantifier {
    enum Type { EXISTENTIAL, UNIVERSAL };

    explicit Quantifier(Literal lit)
      : lit(lit) {}
    explicit Quantifier(Literal lit, Type t)
      : lit(abs(lit) * (t == EXISTENTIAL ? 1 : -1)) {}

    Literal lit;
    inline constexpr Type type() const {
      return lit > 0 ? EXISTENTIAL : UNIVERSAL;
    }
    inline Literal alit() const { return abs(lit); }

    constexpr bool operator==(const Quantifier& o) const {
      return type() == o.type() && lit == o.lit;
    }
  };

  using Literals = std::vector<Literal>;
  using Quantifiers = std::vector<Quantifier>;

  parac_status prepare(std::string_view input);
  parac_status parse();

  Literal highestLiteral() const { return m_highestLit; }
  int clauseCount() const { return m_clauseCount; }
  const Literals& literals() const { return m_literals; }
  const Quantifiers& quantifiers() const { return m_quantifiers; }
  const std::string& path() const { return m_path; }

  bool isTrivial() const;
  bool isTrivialSAT() const;
  bool isTrivialUNSAT() const;

  private:
  struct Internal;

  Literals m_literals;
  Quantifiers m_quantifiers;

  std::string m_path;
  std::optional<std::string> m_fileToDelete;

  parac_status processInputToPath(std::string_view input);

  /* ================== PARSING ================== */
  /* Parsing includes the complete bloqqer code, so optimizations can be added
   * at a later stage. The parsed literals() and quantifiers() are always the
   * result of a parsing operation. */

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
    int mapped = 0;                /* index mapped to */
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

  Literal m_highestLit = 0;
  int m_clauseCount = 0;

  int lineno = 1;
  int universal_vars = 0, existential_vars = 0, implicit_vars = 0;
  int nline = 0, szline = 0;
  int remaining = 0;
  int num_vars = 0, num_lits = 0;
  int size_lits = 0;
  int remaining_clauses_to_parse = 0;
  int orig_clauses = 0;
  int orig_num_vars = 0;
  int units = 0, unates = 0, unets = 0, zombies = 0, eliminated = 0, mapped = 0,
      outermost_blocked = 0;
  int assigned_scope = -1;
  std::string line;

  Literals lits;

  bool force = false;

  const char* parse(FILE* ifile);
  void push_literal(int lit);
  void enlarge_lits();
  void add_quantifier(int lit);
  void add_clause();
  Sig sig_lits();

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
