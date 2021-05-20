#include "parser_qbf.hpp"
#include "paracooba/common/path.h"
#include "paracooba/common/status.h"

#include <limits>
#include <paracooba/common/log.h>
#include <paracooba/util/string_to_file.hpp>
#include <paracooba/util/unique_file.hpp>

#include <iostream>

namespace parac::solver_qbf {

// Todo: Integrate automatic archive reading

static void
msg(const char* fmt, ...) {
  if(!parac_log_enabled(PARAC_SOLVER, PARAC_TRACE))
    return;

  va_list ap;
  char buffer[4096];
  va_start(ap, fmt);
  std::vsnprintf(buffer, 4096, fmt, ap);
  va_end(ap);
  parac_log(PARAC_SOLVER, PARAC_TRACE, "Parser message: {}", buffer);
}

// Be more efficient with file reading through using unlocked IO.

#if !defined(DISABLE_UNLOCKED) and                            \
  ((_POSIX_C_SOURCE >= 199309L or defined(_POSIX_C_SOURCE) or \
    defined(_SVID_SOURCE) or defined(_BSD_SOURCE)))
#define USE_UNLOCKED
#endif

#ifdef USE_UNLOCKED
#define getc(FILE) fgetc_unlocked(FILE)
#endif

/* Parsing code from Armin Biere's Bloqqer */
const char*
Parser::parse(FILE* ifile) {
  assert(ifile);

  int ch, m, n, i, c, q, lit, sign;

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
  while(isdigit(ch = getc(ifile))) {
    m = 10 * m + (ch - '0');
  }
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
      lit *= q;
      add_quantifier(lit);
      if(q > 0)
        existential_vars++;
      else
        universal_vars++;
    } else
      q = 0;
  } else {
    if(lit)
      lit *= sign, c++;
    else
      i++, remaining_clauses_to_parse--;
    if(lit)
      push_literal(lit);
    else {
      add_clause();
    }
  }
  goto NEXT;
DONE:
  return 0;
}

void
Parser::enlarge_lits() {
  msg("Enlarge lits from %d", size_lits);
  auto max = std::numeric_limits<decltype(size_lits)>::max() - 1;
  assert(size_lits < max);
  int64_t new_size = size_lits;
  new_size = new_size ? 2 * new_size : 1;
  if(new_size > max) {
    new_size = max;
  }
  int new_size_lits = new_size;
  RSZ(lits, new_size_lits);
  size_lits = new_size_lits;
}

void
Parser::push_literal(int lit) {
  auto alit = abs(lit);

  assert(alit <= num_vars);
  if(num_lits + 1 >= size_lits)
    enlarge_lits();
  lits[num_lits++] = lit;

  if(alit > m_highestLit) {
    m_highestLit = alit;
  }
}

void
Parser::add_quantifier(int lit) {
  m_quantifiers.emplace_back(Quantifier(lit));
}

void
Parser::add_clause() {
  assert(lits.size() > 0);
  if(static_cast<int>(lits.size()) <= num_lits + 1)
    enlarge_lits();
  lits[num_lits++] = 0;
  std::copy(
    lits.begin(), lits.begin() + num_lits, std::back_inserter(m_literals));

  num_lits = 0;
}

Parser::Parser() {}
Parser::~Parser() {
  if(m_fileToDelete) {
    parac_log(
      PARAC_SOLVER, PARAC_TRACE, "Delete temp file \"{}\".", *m_fileToDelete);
    unlink(m_fileToDelete->c_str());
  }
}

parac_status
Parser::prepare(std::string_view input) {
  parac_status status;

  if((status = processInputToPath(input)) != PARAC_OK)
    return status;

  return status;
}

parac_status
Parser::parse() {
  parac_log(
    PARAC_SOLVER, PARAC_TRACE, "Starting parsing of file \"{}\".", m_path);

  parac::util::UniqueFile f{ fopen(m_path.c_str(), "r") };

  if(!f.get()) {
    return PARAC_FILE_NOT_FOUND_ERROR;
  }

  auto F = f.get();
#ifdef USE_UNLOCKED
  flockfile(F);
#endif

  const char* status = parse(F);

#ifdef USE_UNLOCKED
  funlockfile(F);
#endif

  if(status != nullptr) {
    parac_log(PARAC_SOLVER,
              PARAC_LOCALERROR,
              "Cannot parse file \"{}\"! Error: {}",
              m_path,
              status);
    return PARAC_PARSE_ERROR;
  }

  return PARAC_OK;
}

parac_status
Parser::processInputToPath(std::string_view input) {
  if(!input.length())
    return PARAC_INVALID_PATH_ERROR;

  if(input[0] == ':') {
    input.remove_prefix(1);
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

    parac_log(PARAC_SOLVER,
              PARAC_TRACE,
              "QBF Parser will work on directly given file \"{}\".",
              m_path);
  }

  assert(m_path != "");

  return PARAC_OK;
}
}
