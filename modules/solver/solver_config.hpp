#pragma once

#include <cstdint>

#include <cereal/access.hpp>
#include <cereal/types/vector.hpp>

struct parac_config_entry;
struct parac_config;

namespace parac::solver {
class SolverConfig {
  public:
  SolverConfig() = default;
  explicit SolverConfig(parac_config* config);
  ~SolverConfig() = default;

  void extractFromConfigEntries();

  enum class Entry {
    DeactivatePredefinedCubes,
    CaDiCaLCubes,
    Resplit,
    InitialCubeDepth,
    InitialMinimalCubeDepth,
    MarchCubes,
    FastSplitMultiplicationFactor,
    SplitMultiplicationFactor,
    _COUNT
  };

  bool PredefinedCubes() const { return m_predefinedCubes; }
  bool CaDiCaLCubes() const { return m_CaDiCaLCubes; };
  bool Resplit() const { return m_resplit; }
  uint16_t InitialCubeDepth() const { return m_initialCubeDepth; }
  uint16_t InitialMinimalCubeDepth() const { return m_initialMinimalCubeDepth; }
  bool MarchCubes() const { return m_marchCubes; }
  float FastSplitMultiplicationFactor() const {
    return m_fastSplitMultiplicationFactor;
  }
  float SplitMultiplicationFactor() const {
    return m_splitMultiplicationFactor;
  }

  private:
  parac_config_entry* m_config = nullptr;

  bool m_predefinedCubes = false;
  bool m_CaDiCaLCubes = false;
  bool m_resplit = false;
  uint16_t m_initialCubeDepth = 0;
  uint16_t m_initialMinimalCubeDepth = 0;
  bool m_marchCubes = false;
  float m_fastSplitMultiplicationFactor = 0.5;
  float m_splitMultiplicationFactor = 2;

  friend class cereal::access;
  template<class Archive>
  void serialize(Archive& ar) {
    ar(cereal::make_nvp("predefinedCubes", m_predefinedCubes),
       cereal::make_nvp("CaDiCaLCubes", m_CaDiCaLCubes),
       cereal::make_nvp("resplit", m_resplit),
       cereal::make_nvp("initialCubeDepth", m_initialCubeDepth),
       cereal::make_nvp("initialMinimalCubeDepth", m_initialMinimalCubeDepth),
       cereal::make_nvp("marchCubes", m_marchCubes),
       cereal::make_nvp("fastSplitMultiplicationFactor",
                        m_fastSplitMultiplicationFactor),
       cereal::make_nvp("splitMultiplicationFactor",
                        m_splitMultiplicationFactor));
  }
};
std::ostream&
operator<<(std::ostream& o, const SolverConfig& config);
}
