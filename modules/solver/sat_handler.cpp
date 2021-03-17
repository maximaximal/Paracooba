#include "sat_handler.hpp"
#include "solver_assignment.hpp"

#include <paracooba/broker/broker.h>
#include <paracooba/common/compute_node_store.h>
#include <paracooba/common/log.h>
#include <paracooba/common/message.h>
#include <paracooba/common/noncopy_ostream.hpp>
#include <paracooba/module.h>

#include <cereal/archives/binary.hpp>

namespace parac::solver {
SatHandler::SatHandler(parac_module& solverModule, parac_id originatorId)
  : m_mod(solverModule)
  , m_originatorId(originatorId) {}

SatHandler::~SatHandler() {}

void
SatHandler::handleSatisfyingAssignmentFound(
  std::unique_ptr<SolverAssignment> assignment) {
  std::unique_lock lock(m_mutex, std::try_to_lock);

  if(!lock.owns_lock() || m_assignment) {
    // A result was already found! Do nothing with the assignment.
    parac_log(PARAC_SOLVER,
              PARAC_DEBUG,
              "Found another satisfying assignment for formula "
              "originating from {}! This is discarded.",
              m_originatorId);
    return;
  }

  m_assignment = std::move(assignment);

  if(m_originatorId == m_mod.handle->id) {
    // Local solution found! Give solution to handle and exit paracooba, so that
    // it can be printed in main().

    m_mod.handle->assignment_data = m_assignment.get();
    m_mod.handle->assignment_is_set = &SolverAssignment::static_isSet;
    m_mod.handle->assignment_highest_literal =
      &SolverAssignment::static_highestLiteral;

    m_mod.handle->request_exit(m_mod.handle);
  } else {
    auto& o = m_assignmentOstream;
    assert(!o);
    o = std::make_unique<NoncopyOStringstream>();

    parac_log(PARAC_SOLVER,
              PARAC_DEBUG,
              "SAT result destined for originator {} found in daemon compute "
              "node! Encoding and sending.",
              m_originatorId);

    {
      cereal::BinaryOutputArchive oa(*o);
      oa(*m_assignment);
    }

    parac_log(PARAC_SOLVER,
              PARAC_DEBUG,
              "SAT result destined for originator {} encoded into {}B.",
              m_originatorId,
              o->tellp());

    // Assignment found on some worker node! Send to master.
    auto brokerMod = m_mod.handle->modules[PARAC_MOD_BROKER];
    assert(brokerMod);
    auto broker = brokerMod->broker;
    assert(broker);
    auto computeNodeStore = broker->compute_node_store;
    assert(computeNodeStore);
    assert(computeNodeStore->has(computeNodeStore, m_originatorId));

    auto originator = computeNodeStore->get(computeNodeStore, m_originatorId);
    assert(originator);
    assert(originator->available_to_send_to(originator));

    parac_message_wrapper msg;
    msg.kind = PARAC_MESSAGE_SOLVER_SAT_ASSIGNMENT;
    msg.originator_id = m_originatorId;
    msg.data = o->ptr();
    msg.length = o->tellp();

    parac_log(PARAC_SOLVER,
              PARAC_DEBUG,
              "Sending SAT result destined for originator {}.",
              m_originatorId);

    originator->send_message_to(originator, &msg);
  }
}
}
