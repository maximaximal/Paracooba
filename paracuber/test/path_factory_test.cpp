#include "paracuber/task_factory.hpp"
#include "paracuber/util.hpp"
#include <catch2/catch.hpp>
#include <paracuber/cnftree.hpp>
#include <paracuber/config.hpp>
#include <paracuber/decision_task.hpp>
#include <paracuber/log.hpp>

using namespace paracuber;

TEST_CASE("TaskFactory Manipulation")
{
  ConfigPtr config = std::make_shared<Config>();
  char* argv[] = { (char*)"", NULL };
  config->parseParameters(1, argv);
  LogPtr log = std::make_shared<Log>(config);

  TaskFactory factory(config, log, nullptr);

  CNFTree::Path p = CNFTree::buildPath(static_cast<uint8_t>(0b01010101), 8);

  REQUIRE(!factory.canProduceTask());
  REQUIRE(!factory.produceTask().task);
  factory.addPath(p, TaskFactory::Mode::CubeOrSolve, 0);
  REQUIRE(factory.canProduceTask());
  auto produced = factory.produceTask();
  REQUIRE(produced.originator == 0);
  REQUIRE(produced.task);

  auto decisionTask =
    static_unique_pointer_cast<DecisionTask>(std::move(produced.task));
  REQUIRE(decisionTask->getPath() == p);
}
