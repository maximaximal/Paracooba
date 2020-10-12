#include <paracooba/common/compute_node.h>

const char*
parac_compute_node_state_to_str(parac_compute_node_state state) {
  switch(state) {
    case PARAC_COMPUTE_NODE_NEW:
      return "New";
    case PARAC_COMPUTE_NODE_ACTIVE:
      return "Active";
    case PARAC_COMPUTE_NODE_TIMEOUT:
      return "Timeout";
    case PARAC_COMPUTE_NODE_EXITED:
      return "Exited";
  }
  return "UNKNOWN STATE";
}
