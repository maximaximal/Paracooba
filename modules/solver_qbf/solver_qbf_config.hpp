#pragma once

#include <cstdint>
#include <optional>

#include <cereal/access.hpp>
#include <cereal/types/vector.hpp>

#include <paracooba/common/types.h>

struct parac_config_entry;
struct parac_config;

namespace parac::solver_qbf {
class SolverConfig {
  public:
  SolverConfig() = default;
  explicit SolverConfig(parac_config* config, parac_id localId);
  ~SolverConfig();

  void extractFromConfigEntries();

  parac_id originatorId() const { return m_originatorId; }
  bool useDepQBF() const { return m_useDepQBF; }
  int64_t treeDepth() const { return m_treeDepth; }

  bool integerBasedSplitsEnabled() const {
    return m_integerBasedSplits.size() > 0;
  }
  const std::vector<int>& integerBasedSplits() const {
    return m_integerBasedSplits;
  }
  size_t integerBasedSplitsCurrentIndex(size_t currentCubeLength) const;
  size_t integerBasedSplitsCurrentLength(size_t index) const;

  size_t fullyRealizedTreeDepth() const;

  enum class Entry {
    UseDepQBF,
    TreeDepth,
    IntegerBasedSplits,
    QuapiSolvers,
    QuapiDebug,
    _COUNT
  };

  struct QuapiSolver {
    /** @brief Create QuapiSolver config instance from a single CLI arg.
     *
     */
    QuapiSolver() = default;
    QuapiSolver(const std::string& cliParam);

    std::string path;
    std::vector<std::string> argv;
    std::vector<std::string> envp;
    std::optional<std::string> SAT_regex;
    std::optional<std::string> UNSAT_regex;

    std::shared_ptr<const char*[]> argv_cstyle;
    std::shared_ptr<const char*[]> envp_cstyle;

    std::string name() const;

    friend class cereal::access;
    template<class Archive>
    void serialize(Archive& ar) {
      ar(cereal::make_nvp("path", path),
         cereal::make_nvp("argv", argv),
         cereal::make_nvp("envp", envp),
         cereal::make_nvp("SAT_regex", SAT_regex),
         cereal::make_nvp("UNSAT_regex", UNSAT_regex));
    }
  };

  const std::vector<QuapiSolver>& quapiSolvers() const {
    return m_quapiSolvers;
  }

  private:
  parac_config_entry* m_config = nullptr;

  parac_id m_originatorId = 0;

  bool m_useDepQBF = false;
  int64_t m_treeDepth;
  std::vector<int> m_integerBasedSplits;
  std::vector<QuapiSolver> m_quapiSolvers;
  bool m_quapiDebug = false;

  friend class cereal::access;
  template<class Archive>
  void serialize(Archive& ar) {
    ar(cereal::make_nvp("originatorId", m_originatorId),
       cereal::make_nvp("useDepQBF", m_useDepQBF),
       cereal::make_nvp("treeDepth", m_treeDepth),
       cereal::make_nvp("quapisolvers", m_quapiSolvers),
       cereal::make_nvp("quapidebug", m_quapiDebug),
       cereal::make_nvp("integerBasedSplits", m_integerBasedSplits));
  }

  void parseQuapiSolversCLI(const parac_config_entry& e);
};
std::ostream&
operator<<(std::ostream& o, const SolverConfig& config);
std::ostream&
operator<<(std::ostream& o, const SolverConfig::QuapiSolver& s);
}
