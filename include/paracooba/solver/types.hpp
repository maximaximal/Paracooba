#ifndef PARACOOBA_SOLVER_TYPES_HPP
#define PARACOOBA_SOLVER_TYPES_HPP

#include <cstdint>
#include <optional>
#include <vector>

namespace parac::solver {
using Literal = int;
using Cube = std::vector<Literal>;
using Clause = Cube;
using CubeId = std::uint64_t;
}

#endif
