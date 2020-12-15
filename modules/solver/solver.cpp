#include "paracooba/solver/solver.h"
#include "cadical_handle.hpp"
#include "cadical_manager.hpp"
#include "cereal/archives/binary.hpp"
#include "paracooba/common/log.h"
#include "paracooba/common/task_store.h"
#include "parser_task.hpp"
#include "solver_config.hpp"
#include "solver_task.hpp"
#include <paracooba/broker/broker.h>
#include <paracooba/common/message.h>
#include <paracooba/common/message_kind.h>
#include <paracooba/common/path.h>
#include <paracooba/module.h>
#include <paracooba/runner/runner.h>
#include <paracooba/solver/solver.h>

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
using parac::solver::ParserTask;
using parac::solver::SolverConfig;
using parac::solver::SolverTask;

struct SolverInstance;
using SolverInstanceList = std::list<SolverInstance>;

struct SolverInstance {
  explicit SolverInstance(parac_id originatorId)
    : originatorId(originatorId) {}

  parac_module_solver_instance instance;
  std::unique_ptr<CaDiCaLManager> cadicalManager;
  SolverInstanceList::iterator it;
  SolverConfig config;
  const parac_id originatorId;
  parac_task_store* task_store = nullptr;

  bool configured = false;
};

struct SolverUserdata {
  SolverInstanceList instances;
  std::mutex instancesMutex;
  SolverConfig config;
};

static parac_status
initiate_solving_on_file(parac_module& mod,
                         parac_module_solver_instance* instance,
                         const char* file,
                         parac_id originatorId) {
  assert(mod.handle);
  assert(mod.handle->input_file);

  assert(mod.handle->modules[PARAC_MOD_BROKER]);
  auto& broker = *mod.handle->modules[PARAC_MOD_BROKER]->broker;
  assert(broker.task_store);
  auto& task_store = *broker.task_store;

  bool useLocalWorkers = true;
  if(mod.handle->modules[PARAC_MOD_RUNNER]) {
    useLocalWorkers =
      mod.handle->modules[PARAC_MOD_RUNNER]->runner->available_worker_count > 0;
  }

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

    SolverInstance* i = static_cast<SolverInstance*>(instance->userdata);
    i->task_store = &task_store;
    i->cadicalManager = std::make_unique<CaDiCaLManager>(
      mod, std::move(parsedFormula), i->config);

    parac_task* task = task_store.new_task(
      &task_store, nullptr, parac_path_unknown(), originatorId);
    assert(task);
    SolverTask::createRoot(*task, *i->cadicalManager);
    task_store.assess_task(&task_store, task);
  };

  if(useLocalWorkers) {
    parac_task* task = task_store.new_task(
      &task_store, nullptr, parac_path_unknown(), originatorId);
    parac_log(PARAC_SOLVER,
              PARAC_DEBUG,
              "Create ParserTask for formula in file \"{}\" from node {}.",
              file[0] == ':' ? "(inline - given on CLI)" : file,
              originatorId);

    assert(task);
    new ParserTask(*task, file, originatorId, parserDone);
    task_store.assess_task(&task_store, task);
  } else {
    parac_task task;
    task.path.rep = PARAC_PATH_PARSER;
    parac_log(PARAC_SOLVER,
              PARAC_DEBUG,
              "Create ParserTask for formula in file \"{}\" from node {}.",
              file[0] == ':' ? "(inline - given on CLI)" : file,
              originatorId);
    new ParserTask(task, file, originatorId, parserDone);
    task.work(&task, 0);
  }

  return PARAC_OK;
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

      cereal::BinaryInputArchive ia(data);
      ia(pathType);

      parac_task* task = task_store->new_task(task_store,
                                              nullptr,
                                              parac_path{ .rep = pathType },
                                              instance->originator_id);
      assert(task);
      task->received_from = msg->origin;

      SolverTask* solverTask =
        solverInstance->cadicalManager->createSolverTask(*task, nullptr);
      ia(*solverTask);

      task_store->assess_task(task_store, task);

      break;
    }
    case PARAC_MESSAGE_SOLVER_DESCRIPTION: {
      assert(!solverInstance->configured);
      {
        cereal::BinaryInputArchive ia(data);
        ia(solverInstance->config);
      }
      solverInstance->configured = true;
      break;
    }
    default:
      assert(false);
  }
  return PARAC_OK;
}

static parac_module_solver_instance*
add_instance(parac_module* mod, parac_id originatorId) {
  assert(mod);
  assert(mod->userdata);
  SolverUserdata* userdata = static_cast<SolverUserdata*>(mod->userdata);
  std::unique_lock lock(userdata->instancesMutex);
  auto& instance = userdata->instances.emplace_front(originatorId);
  instance.it = userdata->instances.begin();
  instance.instance.userdata = &instance;
  instance.instance.handle_message = &instance_handle_message;
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
    parac_module_solver_instance* instance =
      mod->solver->add_instance(mod, mod->handle->id);

    SolverInstance* i = static_cast<SolverInstance*>(instance->userdata);
    i->config = userdata->config;

    status = initiate_solving_on_file(
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

  userdata->config = SolverConfig(handle->config);

  return PARAC_OK;
}
