#include "parser_qbf.hpp"

#include <paracooba/common/log.h>
#include <paracooba/util/string_to_file.hpp>

namespace parac::solver_qbf {

static void
msg(const char* fmt, ...) {
  if(!parac_log_enabled(PARAC_SOLVER, PARAC_TRACE))
    return;

  va_list ap;
  char buffer[4096];
  va_start(ap, fmt);
  std::snprintf(buffer, 4096, fmt, ap);
  va_end(ap);
  parac_log(PARAC_SOLVER, PARAC_TRACE, "Parser message: {}", buffer);
}

static int
sign(int lit) {
  return lit < 0 ? -1 : 1;
}

/* Parsing code from Armin Biere's Bloqqer */
const char*
Parser::parse(FILE* ifile) {
  int ch, m, n, i, j, c, q, lit, sign;

  lineno = 1;
  i = c = q = 0;

  assert(!universal_vars);
  assert(!existential_vars);

  szline = 128;
  NEWN(line, szline);
  nline = 0;

SKIP:
  ch = getc(ifile);
  if(ch == '\n') {
    lineno++;
    goto SKIP;
  }
  if(ch == ' ' || ch == '\t' || ch == '\r')
    goto SKIP;
  if(ch == 'c') {
    line[nline = 0] = 0;
    while((ch = getc(ifile)) != '\n') {
      if(ch == EOF)
        return "end of file in comment";
      if(nline + 1 == szline) {
        RSZ(line, 2 * szline);
        szline *= 2;
      }
      line[nline++] = ch;
      line[nline] = 0;
    }
    lineno++;
    goto SKIP;
  }
  if(ch != 'p') {
  HERR:
    return "invalid or missing header";
  }
  if(getc(ifile) != ' ')
    goto HERR;
  while((ch = getc(ifile)) == ' ')
    ;
  if(ch != 'c')
    goto HERR;
  if(getc(ifile) != 'n')
    goto HERR;
  if(getc(ifile) != 'f')
    goto HERR;
  if(getc(ifile) != ' ')
    goto HERR;
  while((ch = getc(ifile)) == ' ')
    ;
  if(!isdigit(ch))
    goto HERR;
  m = ch - '0';
  while(isdigit(ch = getc(ifile)))
    m = 10 * m + (ch - '0');
  if(ch != ' ')
    goto HERR;
  while((ch = getc(ifile)) == ' ')
    ;
  if(!isdigit(ch))
    goto HERR;
  n = ch - '0';
  while(isdigit(ch = getc(ifile)))
    n = 10 * n + (ch - '0');
  while(ch != '\n')
    if(ch != ' ' && ch != '\t' && ch != '\r')
      goto HERR;
    else
      ch = getc(ifile);
  lineno++;
  msg("found header 'p cnf %d %d'", m, n);
  remaining = num_vars = m;
  remaining_clauses_to_parse = n;
  NEWN(vars, num_vars + 1);
  if(num_vars)
    init_variables(1);
  NEWN(dfsi, 2 * num_vars + 1);
  dfsi_ptr = dfsi.data() + num_vars;
  NEWN(mindfsi, 2 * num_vars + 1);
  mindfsi_ptr = mindfsi.data() + num_vars;
  orig_num_vars = num_vars;
  NEWN(repr, 2 * num_vars + 1);
  repr_ptr = repr.data() + num_vars;
  NEWN(subst_vals, num_vars + 1);
  for(j = 0; j < num_vars + 1; j++) {
    subst_vals[j] = j;
  }
  NEWN(fwsigs, num_vars + 1);
  NEWN(bwsigs, num_vars + 1);
  NEWN(anchors, num_vars + 1);
  NEWN(trail, num_vars);
  NEWN(clause_stack, num_vars);
  clause_stack_top = 0;
  clause_stack_size = num_vars;
  top_of_trail = next_on_trail = trail.data();
  NEWN(schedule, num_vars);
  assert(!size_schedule);
NEXT:
  ch = getc(ifile);
  if(ch == '\n') {
    lineno++;
    goto NEXT;
  }
  if(ch == ' ' || ch == '\t' || ch == '\r')
    goto NEXT;
  if(ch == 'c') {
    while((ch = getc(ifile)) != '\n')
      if(ch == EOF)
        return "end of file in comment";
    lineno++;
    goto NEXT;
  }
  if(ch == EOF) {
    if(!force && i < n)
      return "clauses missing";
    orig_clauses = i;
    if(!q && !c)
      init_implicit_scope();
    goto DONE;
  }
  if(ch == '-') {
    if(q)
      return "negative number in prefix";
    sign = -1;
    ch = getc(ifile);
    if(ch == '0')
      return "'-' followed by '0'";
  } else
    sign = 1;
  if(ch == 'e') {
    if(c)
      return "'e' after at least one clause";
    if(q)
      return "'0' missing after 'e'";
    q = 1;
    goto NEXT;
  }
  if(ch == 'a') {
    if(c)
      return "'a' after at least one clause";
    if(q)
      return "'0' missing after 'a'";
    q = -1;
    goto NEXT;
  }
  if(!isdigit(ch))
    return "expected digit";
  lit = ch - '0';
  while(isdigit(ch = getc(ifile)))
    lit = 10 * lit + (ch - '0');
  if(ch != EOF && ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r')
    return "expected space after literal";
  if(ch == '\n')
    lineno++;
  if(lit > m)
    return "maximum variable index exceeded";
  if(!force && !q && i == n)
    return "too many clauses";
  if(q) {
    if(lit) {
      if(sign < 0)
        return "negative literal quantified";
      if(lit2scope(lit)) {
        return "variable quantified twice";
      }
      lit *= q;
      add_quantifier(lit);
      if(q > 0)
        existential_vars++;
      else
        universal_vars++;
    } else
      q = 0;
  } else {
    if(!q && !c) {
      init_implicit_scope();
    }
    if(lit)
      lit *= sign, c++;
    else
      i++, remaining_clauses_to_parse--;
    if(lit)
      push_literal(lit);
    else {
      if(!empty_clause) {
        add_clause();
        if(empty_clause) {
          orig_clauses = i;
          goto DONE;
        }
      } else {
        num_lits = 0;
      }
    }
  }
  goto NEXT;
DONE:
  return 0;
}

Parser::Var*
Parser::lit2var(int lit) {
  assert(lit && abs(lit) <= num_vars);
  return &vars[abs(lit)];
}

Parser::Occ*
Parser::lit2occ(int lit) {
  Var* v = lit2var(lit);
  return v->occs + (lit < 0);
}

Parser::Scope*
Parser::lit2scope(int lit) {
  return lit2var(lit)->scope;
}

void
Parser::add_var(int idx, Scope* scope) {
  Var* v;
  assert(0 < idx && idx <= num_vars);
  v = lit2var(idx);
  assert(!v->scope);
  v->scope = scope;
  v->next = NULL;
  v->prev = scope->last;

  if(scope->last)
    scope->last->next = v;
  else
    scope->first = v;
  scope->last = v;
  assert(v->tag == FREE);
  scope->free++;
}

void
Parser::init_variables(int start) {
  int i;
  assert(0 < start && start <= num_vars);
  for(i = start; i <= num_vars; i++) {
    Var* v = &vars[i];
    v->pos = -1;
    v->mark = 0;
    v->mark2 = v->mark3 = v->mark4 = 0;
    v->mapped = ++mapped;
    assert(mapped == i);
  }
  assert(mapped == num_vars);
}

void
Parser::add_outer_most_scope(void) {
  outer_most_scope = scopes.malloc();
  outer_most_scope->type = 1;
  inner_most_scope = outer_most_scope;
}

void
Parser::init_implicit_scope(void) {
  Var* v;
  int idx;
  int count = 0;
  Var* p;

  assert(!implicit_vars);
  implicit_vars = num_vars - universal_vars - existential_vars;
  existential_vars += implicit_vars;
  assert(implicit_vars >= 0);
  msg("%d universal, %d existential variables (%d implicit)",
      universal_vars,
      existential_vars,
      implicit_vars);
  if(implicit_vars > 0) {

    if(!outer_most_scope)
      add_outer_most_scope();
    for(v = vars.data() + 1; v <= vars.data() + num_vars; v++)
      if(!v->scope) {
        idx = v - vars.data();
        add_var(idx, outer_most_scope);
      }
  }

  for(p = outer_most_scope->first; p; p = p->next) {
    count++;
  }

  if(count == 0)
    assigned_scope = 1;
  else
    assigned_scope = 0;
}

void
Parser::enlarge_lits() {
  int new_size_lits = size_lits ? 2 * size_lits : 1;
  RSZ(lits, new_size_lits);
  size_lits = new_size_lits;
}

void
Parser::push_literal(int lit) {
  assert(abs(lit) <= num_vars);
  if(size_lits == num_lits)
    enlarge_lits();
  lits[num_lits++] = lit;
}

void
Parser::add_quantifier(int lit) {
  Scope* scope;
  if(!outer_most_scope)
    add_outer_most_scope();
  if(inner_most_scope->type != sign(lit)) {
    scope = scopes.malloc();
    scope->outer = inner_most_scope;
    scope->type = sign(lit);
    scope->order = inner_most_scope->order + 1;
    scope->stretch = scope->order;
    inner_most_scope->inner = scope;
    inner_most_scope = scope;
  } else
    scope = inner_most_scope;
  add_var(abs(lit), scope);
}

Parser::Sig
Parser::lit2sig(int lit) {
  return 1ull << ((100623947llu * (Parser::Sig)abs(lit)) & 63llu);
}

Parser::Sig
Parser::sig_lits() {
  Sig res = 0ull;
  int i, lit;
  for(i = 0; i < num_lits; i++) {
    lit = lits[i];
    res |= lit2sig(lit);
  }
  return res;
}

void
Parser::add_node(Clause* clause, Node* node, int lit) {
  Occ* occ;
  assert(clause->nodes.data() <= node &&
         node < clause->nodes.data() + clause->size);
  node->clause = clause;
  node->lit = lit;
  node->next = NULL;
  occ = lit2occ(lit);
  node->prev = occ->last;
  if(occ->last)
    occ->last->next = node;
  else
    occ->first = node;
  occ->last = node;
  occ->count++;
}

void
Parser::add_clause() {
  Clause& clause = clauses.emplace_back();
  clause.nodes.resize(num_lits);
  last_clause = &clause;

  clause.count = 1;
  clause.size = num_lits;
  clause.prev = last_clause;
  clause.sig = sig_lits();

  if(last_clause)
    last_clause->next = &clause;
  else
    first_clause = &clause;
  last_clause = &clause;

  for(int i = 0; i < num_lits; i++)
    add_node(&clause, &clause.nodes[i], lits[i]);

  num_lits = 0;
}

Parser::Parser() {}
Parser::~Parser() {
  if(m_fileToDelete) {
    unlink(m_fileToDelete->c_str());
  }
}

parac_status
Parser::parse(std::string_view input) {
  parac_status status;

  if((status = processInputToPath(input)) != PARAC_OK)
    return status;

  return PARAC_OK;
}

parac_status
Parser::processInputToPath(std::string_view input) {
  if(!input.length())
    return PARAC_INVALID_PATH_ERROR;

  if(input[0] == ':') {
    auto [status, tmp_path] = StringToFile(input);
    if(status != PARAC_OK) {
      return status;
    }

    m_path = tmp_path;
    m_fileToDelete = m_path;

    parac_log(PARAC_SOLVER,
              PARAC_TRACE,
              "Wrote temp file \"{}\" in order to parse QDIMACS from string.",
              m_path);
  } else {
    m_path = input;
  }

  return PARAC_OK;
}
}
