#include "paracooba/solver/solver.h"
#include "cadical_handle.hpp"
#include "cadical_manager.hpp"
#include "cereal/archives/binary.hpp"
#include "kissat_task.hpp"
#include "paracooba/common/compute_node.h"
#include "paracooba/common/compute_node_store.h"
#include "paracooba/common/log.h"
#include "paracooba/common/noncopy_ostream.hpp"
#include "paracooba/common/status.h"
#include "paracooba/common/task_store.h"
#include "parser_task.hpp"
#include "sat_handler.hpp"
#include "solver_assignment.hpp"
#include "solver_config.hpp"
#include "solver_task.hpp"
#include <paracooba/broker/broker.h>
#include <paracooba/common/file.h>
#include <paracooba/common/message.h>
#include <paracooba/common/message_kind.h>
#include <paracooba/common/path.h>
#include <paracooba/module.h>
#include <paracooba/runner/runner.h>
#include <paracooba/solver/solver.h>
#include <paracooba/solver/types.hpp>

#include <cassert>
#include <list>
#include <mutex>

#include <parac_solver_export.h>

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>

#define SOLVER_NAME "cpp_cubetree_splitter"
#define SOLVER_VERSION_MAJOR 1
#define SOLVER_VERSION_MINOR 0
#define SOLVER_VERSION_PATCH 0
#define SOLVER_VERSION_TWEAK 0

using parac::solver::CaDiCaLManager;
using parac::solver::Clause;
using parac::solver::KissatTask;
using parac::solver::ParserTask;
using parac::solver::SatHandler;
using parac::solver::SolverAssignment;
using parac::solver::SolverConfig;
using parac::solver::SolverTask;

struct SolverInstance;
using SolverInstanceList = std::list<SolverInstance>;

struct SolverInstance {
  explicit SolverInstance(parac_module* mod, parac_id originatorId)
    : mod(mod)
    , satHandler(std::make_unique<SatHandler>(*mod, originatorId)) {
    instance.originator_id = originatorId;
  }

  parac_module* mod;

  parac_module_solver_instance instance;
  std::unique_ptr<CaDiCaLManager> cadicalManager;
  std::unique_ptr<SatHandler> satHandler;
  SolverConfig config;
  parac_task_store* task_store = nullptr;
  SolverInstanceList::iterator it;

  bool configured = false;

  std::unique_ptr<parac::NoncopyOStringstream> configStream;
};

// static_assert(std::is_standard_layout<SolverInstance>());

struct SolverUserdata {
  SolverInstanceList instances;
  std::mutex instancesMutex;
  SolverConfig config;
};

static void
serialize_solver_config_into_msg(parac_module_solver_instance* instance,
                                 parac_message* msg) {
  assert(instance);
  assert(instance->userdata);
  assert(msg);

  SolverInstance* solverInstance =
    static_cast<SolverInstance*>(instance->userdata);

  msg->kind = PARAC_MESSAGE_SOLVER_DESCRIPTION;

  if(!solverInstance->configStream) {
    solverInstance->configStream =
      std::make_unique<parac::NoncopyOStringstream>();
    cereal::BinaryOutputArchive oa(*solverInstance->configStream);
    oa(solverInstance->config);
  }

  msg->data = solverInstance->configStream->ptr();
  msg->length = solverInstance->configStream->tellp();
  msg->originator_id = instance->originator_id;
}

static parac_status
parse_formula_file(parac_module& mod,
                   const char* file,
                   parac_id originatorId,
                   ParserTask::FinishedCB parsingFinishedCB) {
  assert(mod.handle);
  assert(file);

  assert(mod.handle->modules[PARAC_MOD_BROKER]);
  auto& broker = *mod.handle->modules[PARAC_MOD_BROKER]->broker;
  assert(broker.task_store);
  auto& task_store = *broker.task_store;

  bool useLocalWorkers = true;
  if(mod.handle->modules[PARAC_MOD_RUNNER]) {
    useLocalWorkers =
      mod.handle->modules[PARAC_MOD_RUNNER]->runner->available_worker_count > 0;
  }

  if(useLocalWorkers) {
    parac_task* task = task_store.new_task(
      &task_store, nullptr, parac_path_unknown(), originatorId);
    parac_log(PARAC_SOLVER,
              PARAC_DEBUG,
              "Create ParserTask for formula in file \"{}\" from node {}.",
              file[0] == ':' ? "(inline - given on CLI)" : file,
              originatorId);

    assert(task);
    new ParserTask(*mod.handle, *task, file, originatorId, parsingFinishedCB);
    task_store.assess_task(&task_store, task);
  } else {
    parac_task task;
    parac_task_init(&task);
    task.path.rep = PARAC_PATH_PARSER;
    parac_log(PARAC_SOLVER,
              PARAC_DEBUG,
              "Create ParserTask for formula in file \"{}\" from node {}.",
              file[0] == ':' ? "(inline - given on CLI)" : file,
              originatorId);
    new ParserTask(*mod.handle, task, file, originatorId, parsingFinishedCB);
    task.work(&task, 0);
  }

  return PARAC_OK;
}

static parac_status
initiate_root_solver_on_file(parac_module& mod,
                             parac_module_solver_instance* instance,
                             const char* file,
                             parac_id originatorId) {
  assert(mod.handle);
  assert(mod.handle->input_file);

  assert(mod.handle->modules[PARAC_MOD_BROKER]);
  assert(mod.handle->modules[PARAC_MOD_BROKER]->broker);
  auto& broker = *mod.handle->modules[PARAC_MOD_BROKER]->broker;
  assert(broker.task_store);
  auto& task_store = *broker.task_store;

  assert(mod.handle->modules[PARAC_MOD_RUNNER]);
  assert(mod.handle->modules[PARAC_MOD_RUNNER]->runner);
  auto& runner = *mod.handle->modules[PARAC_MOD_RUNNER]->runner;

  auto parserDone = [&mod, &task_store, originatorId, instance](
                      parac_status status,
                      ParserTask::CaDiCaLHandlePtr parsedFormula) {
    if(status != PARAC_OK) {
      parac_log(PARAC_SOLVER,
                PARAC_FATAL,
                "Parsing of formula \"{}\" failed with status {}! Exiting.",
                mod.handle->input_file,
                status);
      mod.handle->request_exit(mod.handle);
      return;
    }

    SolverUserdata* solverUserdata = static_cast<SolverUserdata*>(mod.userdata);
    assert(solverUserdata);

    SolverInstance* i = static_cast<SolverInstance*>(instance->userdata);
    i->task_store = &task_store;
    i->cadicalManager = std::make_unique<CaDiCaLManager>(
      mod, std::move(parsedFormula), i->config, *i->satHandler);
    i->config = solverUserdata->config;
    i->configured = true;

    for(auto& rootCubeSource : i->cadicalManager->getRootCubeSources()) {
      parac_task* task = task_store.new_task(
        &task_store, nullptr, parac_path_unknown(), originatorId);
      assert(task);
      task->path = parac_path_root();
      SolverTask::create(*task, *i->cadicalManager, rootCubeSource);
      task_store.assess_task(&task_store, task);
    }
  };

  parac_status status = parse_formula_file(mod, file, originatorId, parserDone);
  assert(instance->userdata);
  SolverInstance* i = static_cast<SolverInstance*>(instance->userdata);
  if(runner.available_worker_count > 1 && !i->config.DisableLocalKissat()) {
    parac_log(PARAC_SOLVER, PARAC_DEBUG, "Create local Kissat solver task.");
    parac_task* task = task_store.new_task(
      &task_store, nullptr, parac_path_root(), originatorId);
    assert(task);
    new KissatTask(*mod.handle, file, *task, *i->satHandler);
    task_store.assess_task(&task_store, task);
  } else {
    parac_log(
      PARAC_SOLVER, PARAC_DEBUG, "Local Kissat solver task not created.");
  }

  return status;
}

static parac_status
initiate_peer_solver_on_file(
  parac_module& mod,
  parac_module_solver_instance* instance,
  const char* file,
  parac_id originatorId,
  void* cb_userdata,
  parac_module_solver_instance_formula_parsed_cb cb) {
  assert(mod.handle);
  assert(cb);

  assert(mod.handle->modules[PARAC_MOD_BROKER]);
  auto& broker = *mod.handle->modules[PARAC_MOD_BROKER]->broker;
  assert(broker.task_store);
  auto& task_store = *broker.task_store;

  auto parserDone = [&mod, &task_store, instance, cb, cb_userdata](
                      parac_status status,
                      ParserTask::CaDiCaLHandlePtr parsedFormula) {
    if(status != PARAC_OK) {
      parac_log(PARAC_SOLVER,
                PARAC_FATAL,
                "Parsing of formula \"{}\" failed with status {}! Exiting.",
                parsedFormula->path(),
                status);
      cb(instance, cb_userdata, status);
      return;
    }

    parac_log(PARAC_SOLVER,
              PARAC_TRACE,
              "Parsing of formula \"{}\" finished! Now sending status to peers "
              "and waiting for tasks.",
              parsedFormula->path(),
              status);

    SolverInstance* i = static_cast<SolverInstance*>(instance->userdata);
    i->task_store = &task_store;
    i->cadicalManager = std::make_unique<CaDiCaLManager>(
      mod, std::move(parsedFormula), i->config, *i->satHandler);

    cb(instance, cb_userdata, status);
  };

  return parse_formula_file(mod, file, originatorId, parserDone);
}

static parac_status
instance_handle_message(parac_module_solver_instance* instance,
                        parac_message* msg) {
  assert(instance);
  assert(msg);
  assert(instance->userdata);

  SolverInstance* solverInstance =
    static_cast<SolverInstance*>(instance->userdata);

  parac_task_store* task_store = solverInstance->task_store;
  assert(task_store);

  boost::iostreams::stream<boost::iostreams::basic_array_source<char>> data(
    msg->data, msg->length);

  switch(msg->kind) {
    case PARAC_MESSAGE_SOLVER_TASK: {
      assert(solverInstance->cadicalManager);
      assert(solverInstance->configured);

      parac_path_type pathType;
      parac_task* task = nullptr;
      SolverTask* solverTask = nullptr;

      {
        cereal::BinaryInputArchive ia(data);
        intptr_t remoteTask;
        ia(pathType);
        ia(remoteTask);
        parac_path p;
        p.rep = pathType;
        task =
          task_store->new_task(task_store, nullptr, p, instance->originator_id);
        assert(task);
        task->received_from = msg->origin;
        task->parent_task_ = reinterpret_cast<parac_task*>(remoteTask);

        solverTask =
          solverInstance->cadicalManager->createSolverTask(*task, nullptr);
        ia(*solverTask);
      }

      task_store->assess_task(task_store, task);
      msg->cb(msg, PARAC_OK);
      break;
    }
    case PARAC_MESSAGE_SOLVER_DESCRIPTION: {
      if(solverInstance->configured) {
        parac_log(PARAC_SOLVER,
                  PARAC_GLOBALWARNING,
                  "Received solver config from {}, even though old config was "
                  "already received: {}",
                  msg->origin->id,
                  solverInstance->config);
        msg->cb(msg, PARAC_SOLVER_ALREADY_CONFIGURED_ERROR);
        break;
      }
      {
        cereal::BinaryInputArchive ia(data);
        ia(solverInstance->config);
      }
      solverInstance->configured = true;
      parac_log(PARAC_SOLVER,
                PARAC_TRACE,
                "Received solver config from {}: {}",
                msg->origin->id,
                solverInstance->config);
      msg->cb(msg, PARAC_OK);
      break;
    }
    case PARAC_MESSAGE_SOLVER_SAT_ASSIGNMENT: {
      assert(solverInstance->cadicalManager);

      std::unique_ptr<SolverAssignment> assignment{
        std::make_unique<SolverAssignment>()
      };

      {
        cereal::BinaryInputArchive ia(data);
        ia(*assignment);
      }

      solverInstance->cadicalManager->handleSatisfyingAssignmentFound(
        std::move(assignment));
      msg->cb(msg, PARAC_OK);
      break;
    }
    case PARAC_MESSAGE_SOLVER_NEW_LEARNED_CLAUSE: {
      assert(solverInstance->cadicalManager);
      std::vector<Clause> learnedClauses;
      {
        cereal::BinaryInputArchive ia(data);
        ia(learnedClauses);
      }
      assert(solverInstance);

      if(parac_log_enabled(PARAC_SOLVER, PARAC_TRACE)) {
        std::vector<std::string> clausesString;
        std::transform(learnedClauses.begin(),
                       learnedClauses.end(),
                       std::back_inserter(clausesString),
                       [](const Clause& c) {
                         return fmt::format("({})", fmt::join(c, "|"));
                       });

        parac_log(
          PARAC_SOLVER,
          PARAC_TRACE,
          "Received new learned clauses {} for originator ID {} from {}.",
          fmt::join(clausesString, ","),
          msg->originator_id,
          msg->origin->id);
      }

      for(auto& clause : learnedClauses) {
        solverInstance->cadicalManager->applyAndDistributeNewLearnedClause(
          clause, msg->origin->id);
      }
      msg->cb(msg, PARAC_OK);
      break;
    }
    case PARAC_MESSAGE_SOLVER_NEW_REMOTE_AVAILABLE:
      assert(solverInstance->cadicalManager);
      assert(msg->origin);
      parac_log(PARAC_SOLVER,
                PARAC_TRACE,
                "Registered (possibly) new peer with ID {} in solver with "
                "originator id {}.",
                msg->origin->id,
                solverInstance->cadicalManager->originatorId());
      solverInstance->cadicalManager->addPossiblyNewNodePeer(*msg->origin);
      break;
    default:
      assert(false);
  }
  return PARAC_OK;
}

static parac_status
instance_handle_formula(parac_module_solver_instance* instance,
                        parac_file* formula,
                        void* cb_userdata,
                        parac_module_solver_instance_formula_parsed_cb cb) {
  assert(instance);
  assert(formula);
  assert(instance->userdata);

  SolverInstance* solverInstance =
    static_cast<SolverInstance*>(instance->userdata);

  assert(solverInstance->mod);
  assert(!solverInstance->cadicalManager);

  parac_status status = initiate_peer_solver_on_file(*solverInstance->mod,
                                                     instance,
                                                     formula->path,
                                                     instance->originator_id,
                                                     cb_userdata,
                                                     cb);

  formula->cb(formula, status);

  return status;
}

static parac_module_solver_instance*
add_instance(parac_module* mod,
             parac_id originatorId,
             parac_task_store* task_store) {
  assert(mod);
  assert(mod->userdata);
  SolverUserdata* userdata = static_cast<SolverUserdata*>(mod->userdata);
  std::unique_lock lock(userdata->instancesMutex);
  auto& instance = userdata->instances.emplace_front(mod, originatorId);
  instance.it = userdata->instances.begin();
  instance.instance.userdata = &instance;
  instance.instance.handle_message = &instance_handle_message;
  instance.instance.handle_formula = &instance_handle_formula;
  instance.instance.serialize_config_to_msg = &serialize_solver_config_into_msg;
  instance.task_store = task_store;

  parac_log(PARAC_SOLVER,
            PARAC_TRACE,
            "Create new solver instance with originator ID {}",
            originatorId);

  return &instance.instance;
}

static parac_status
remove_instance(parac_module* mod, parac_module_solver_instance* instance) {
  assert(mod);
  assert(mod->userdata);
  assert(instance);

  SolverUserdata* userdata = static_cast<SolverUserdata*>(mod->userdata);
  SolverInstance* i =
    reinterpret_cast<SolverInstance*>(reinterpret_cast<std::byte*>(instance) -
                                      offsetof(SolverInstance, instance));
  std::unique_lock lock(userdata->instancesMutex);

  userdata->instances.erase(i->it);

  return PARAC_OK;
}

static parac_status
pre_init(parac_module* mod) {
  assert(mod);
  assert(mod->solver);
  assert(mod->handle);
  assert(mod->handle->config);

  mod->solver->add_instance = &add_instance;
  mod->solver->remove_instance = &remove_instance;

  SolverUserdata* userdata = static_cast<SolverUserdata*>(mod->userdata);
  userdata->config.extractFromConfigEntries();

  return PARAC_OK;
}

static parac_status
init(parac_module* mod) {
  assert(mod);
  assert(mod->runner);
  assert(mod->handle);
  assert(mod->handle->config);
  assert(mod->handle->thread_registry);

  SolverUserdata* userdata = static_cast<SolverUserdata*>(mod->userdata);

  /* The solver is responsible to create the initial task and to announce the
   * result. All actual solving happens in this module.
   *
   * As everything is started now, the initial task can be created. This initial
   * task is the ParserTask. Atferwards, SolverTask instances are created, that
   * work according to the CubeTree concept. The CaDiCaLManager copies the
   * original ParserTask to as many task instances as there are worker threads
   * enabled and provides the solver instances to SolverTask instances. They
   * then work and split on their stuff, accumulating data, propagating it
   * upwards and are at the original solver task in the end. This original task
   * then ends processing.
   */

  parac_status status = PARAC_OK;

  if(mod->handle->input_file) {
    parac_task_store* task_store = nullptr;
    if(mod->handle->modules[PARAC_MOD_BROKER]) {
      auto modBroker = mod->handle->modules[PARAC_MOD_BROKER];
      if(modBroker->broker) {
        auto broker = modBroker->broker;
        task_store = broker->task_store;
      }
    }

    parac_module_solver_instance* instance =
      mod->solver->add_instance(mod, mod->handle->id, task_store);

    SolverInstance* i = static_cast<SolverInstance*>(instance->userdata);
    i->config = userdata->config;

    if(mod->handle->modules[PARAC_MOD_BROKER] &&
       mod->handle->modules[PARAC_MOD_BROKER]->broker) {
      auto broker = mod->handle->modules[PARAC_MOD_BROKER]->broker;
      assert(broker);
      assert(broker->compute_node_store);
      auto thisNode = broker->compute_node_store->this_node;
      assert(thisNode);
      thisNode->solver_instance = instance;
    }

    status = initiate_root_solver_on_file(
      *mod, instance, mod->handle->input_file, mod->handle->id);

  } else {
    parac_log(PARAC_SOLVER,
              PARAC_DEBUG,
              "Solver did not receive input file, so paracooba is going into "
              "daemon mode.");
  }

  return status;
}

static parac_status
mod_request_exit(parac_module* mod) {
  assert(mod);
  assert(mod->solver);
  assert(mod->handle);

  return PARAC_OK;
}

static parac_status
mod_exit(parac_module* mod) {
  assert(mod);
  assert(mod->solver);
  assert(mod->handle);

  if(mod->userdata) {
    SolverUserdata* userdata = static_cast<SolverUserdata*>(mod->userdata);
    delete userdata;
    mod->userdata = nullptr;
  }

  return PARAC_OK;
}

extern "C" PARAC_SOLVER_EXPORT parac_status
parac_module_discover_solver(parac_handle* handle) {
  assert(handle);

  parac_module* mod = handle->prepare(handle, PARAC_MOD_SOLVER);
  assert(mod);

  mod->type = PARAC_MOD_SOLVER;
  mod->name = SOLVER_NAME;
  mod->version.major = SOLVER_VERSION_MAJOR;
  mod->version.minor = SOLVER_VERSION_MINOR;
  mod->version.patch = SOLVER_VERSION_PATCH;
  mod->version.tweak = SOLVER_VERSION_TWEAK;
  mod->pre_init = pre_init;
  mod->init = init;
  mod->request_exit = mod_request_exit;
  mod->exit = mod_exit;

  SolverUserdata* userdata = new SolverUserdata();
  mod->userdata = userdata;

  userdata->config = SolverConfig(handle->config, handle->id);

  return PARAC_OK;
}
