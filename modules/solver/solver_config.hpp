#pragma once

#include <cstdint>
#include <optional>

#include <cereal/access.hpp>
#include <cereal/types/vector.hpp>

#include <paracooba/common/types.h>

struct parac_config_entry;
struct parac_config;

namespace parac::solver {
class SolverConfig {
  public:
  SolverConfig() = default;
  explicit SolverConfig(parac_config* config, parac_id localId);
  ~SolverConfig() = default;

  void extractFromConfigEntries();

  enum class Entry {
    DeactivatePredefinedCubes,
    CaDiCaLCubes,
    Resplit,
    QuapiSolvers,
    InitialCubeDepth,
    InitialMinimalCubeDepth,
    InitialSplitTimeoutMS,
    FastSplitMultiplicationFactor,
    SplitMultiplicationFactor,
    DisableLocalKissat,
    ConcurrentCubeTreeCount,
    DistributeCubeTreeLearntClausesMaxLevel,
    _COUNT
  };

  bool PredefinedCubes() const { return m_predefinedCubes; }
  bool CaDiCaLCubes() const { return m_CaDiCaLCubes; };
  bool Resplit() const { return m_resplit; }
  uint16_t InitialCubeDepth() const { return m_initialCubeDepth; }
  uint16_t InitialMinimalCubeDepth() const { return m_initialMinimalCubeDepth; }
  uint16_t ConcurrentCubeTreeCount() const { return m_concurrentCubeTreeCount; }
  uint16_t DistributeCubeTreeLearntClausesMaxLevel() const {
    return m_distributeCubeTreeLearntClausesMaxLevel;
  }
  float FastSplitMultiplicationFactor() const {
    return m_fastSplitMultiplicationFactor;
  }
  float SplitMultiplicationFactor() const {
    return m_splitMultiplicationFactor;
  }
  bool DisableLocalKissat() const { return m_disableLocalKissat; }
  parac_id OriginatorId() const { return m_originatorId; }
  uint32_t InitialSplitTimeoutMS() const { return m_initialSplitTimeoutMS; }

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

  bool m_predefinedCubes = false;
  bool m_CaDiCaLCubes = false;
  bool m_resplit = false;
  bool m_disableLocalKissat = false;
  uint16_t m_initialCubeDepth = 0;
  uint16_t m_initialMinimalCubeDepth = 0;
  uint16_t m_concurrentCubeTreeCount = 1;
  uint16_t m_distributeCubeTreeLearntClausesMaxLevel = 1;
  float m_fastSplitMultiplicationFactor = 0.5;
  float m_splitMultiplicationFactor = 2;
  parac_id m_originatorId = 0;
  uint32_t m_initialSplitTimeoutMS = 30000;
  std::vector<QuapiSolver> m_quapiSolvers;

  friend class cereal::access;
  template<class Archive>
  void serialize(Archive& ar) {
    ar(cereal::make_nvp("predefinedCubes", m_predefinedCubes),
       cereal::make_nvp("CaDiCaLCubes", m_CaDiCaLCubes),
       cereal::make_nvp("resplit", m_resplit),
       cereal::make_nvp("disableLocalKissat", m_disableLocalKissat),
       cereal::make_nvp("initialCubeDepth", m_initialCubeDepth),
       cereal::make_nvp("initialMinimalCubeDepth", m_initialMinimalCubeDepth),
       cereal::make_nvp("fastSplitMultiplicationFactor",
                        m_fastSplitMultiplicationFactor),
       cereal::make_nvp("concurrentCubeTreeCount", m_concurrentCubeTreeCount),
       cereal::make_nvp("distributeCubeTreeLearntClausesMaxLevel",
                        m_distributeCubeTreeLearntClausesMaxLevel),
       cereal::make_nvp("splitMultiplicationFactor",
                        m_splitMultiplicationFactor),
       cereal::make_nvp("initialSplitTimeoutMS", m_initialSplitTimeoutMS),
       cereal::make_nvp("originatorId", m_originatorId),
       cereal::make_nvp("quapisolvers", m_quapiSolvers));
  }

  void parseQuapiSolversCLI(const parac_config_entry& e);
};
std::ostream&
operator<<(std::ostream& o, const SolverConfig& config);
std::ostream&
operator<<(std::ostream& o, const SolverConfig::QuapiSolver& s);
}
