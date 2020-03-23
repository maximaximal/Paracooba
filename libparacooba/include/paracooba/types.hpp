#ifndef PARACOOBA_TYPES
#define PARACOOBA_TYPES

#include <functional>
#include <map>
#include <optional>
#include <vector>

namespace paracooba {
using Literal = int;
using Cube = std::vector<Literal>;
using OptionalCube = std::optional<Cube>;

using ID = int64_t;

class ClusterNode;
using ClusterNodeMap = std::map<ID, ClusterNode>;

using SuccessCB = std::function<void(bool)>;
}

#endif
