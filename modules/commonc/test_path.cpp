#include <catch2/catch.hpp>

#include <paracooba/common/path.h>

#include <bitset>
#include <string>

using namespace parac;

TEST_CASE("Path Manipulation", "[commonc][path]") {
  using PathBitset = std::bitset<64>;

  parac_path path;
  path.rep = 0;
  path.length = 1;
  REQUIRE(!parac_path_get_assignment(path, 1));
  path = parac_path_set_assignment(path, 1, true);
  REQUIRE(parac_path_get_assignment(path, 1));

  Path test_path_str;
  test_path_str.length = 0;
  REQUIRE(to_string(test_path_str) == "(root)");
  test_path_str.length = 0b00111110;
  REQUIRE(to_string(test_path_str) == "(explicitly unknown)");
  test_path_str.length = 60;
  REQUIRE(to_string(test_path_str) == "INVALID PATH");

  Path p = Path(0xF000000000000002);
  REQUIRE(to_string(p) == "11");
  ++p.length;
  REQUIRE(to_string(p) == "111");
  p[3] = false;
  REQUIRE(to_string(p) == "110");

  p = Path::build(0, 0);

  REQUIRE(p.length == 0);
  REQUIRE(p.rep == 0);

  p.length = 1u;
  REQUIRE(p == 0x0000000000000001u);

  p.length = 4;
  REQUIRE(!p[1]);
  REQUIRE(!p[2]);
  REQUIRE(!p[3]);
  REQUIRE(!p[4]);
  REQUIRE(p == 0x0000000000000004u);

  p[1] = true;
  p[2] = true;
  p[3] = true;
  p[4] = true;
  REQUIRE(p == 0xF000000000000004u);

  p = Path::build(0xFFFF0000FFFF0000u, 4);
  REQUIRE(p == 0xF000000000000004u);

  p = Path::build((uint8_t)0b00000001u, 2);
  REQUIRE(p.path == ((uint64_t)0b00000000u) << (64 - 8));
  REQUIRE(!p[1]);

  p = Path::build((uint8_t)0b10000000u, 2);
  REQUIRE(p.left_aligned() == ((uint64_t)0b10000000u) << (64 - 8));

  REQUIRE(p.length == 2);
  REQUIRE(p[1]);
  REQUIRE(!p[2]);
  REQUIRE(p.depth_shifted() == (uint64_t)0b00000010uL);

  p = Path::build((uint8_t)0b10000000u, 4);
  REQUIRE(p.depth_shifted() == (uint64_t)0b00001000uL);
  REQUIRE(p.depth_shifted() == 8u);

  REQUIRE(p.left().length == 5);
  REQUIRE(!p.left()[5]);
  REQUIRE(p.right()[5]);

  REQUIRE(p.left().depth_shifted() == (uint64_t)0b00010000uL);
  REQUIRE(p.right().depth_shifted() == (uint64_t)0b00010001uL);

  p = Path::build((uint32_t)0xFFFFFFF0u, 32);
  REQUIRE(p[15]);
  REQUIRE(p[16]);
  REQUIRE(p[20]);
  REQUIRE(p[25]);
  REQUIRE(!p[31]);

  p = Path::build((uint32_t)0xFFFFFFFFu, 16);
  REQUIRE(p.length == 16);
  REQUIRE(p.length - 1 == 15);

  // This is also a test for cuber::Cuber::getAdditionComponent. The behaviour
  // stays the same.
  p = Path::build((uint8_t)0b11110000u, 4);
  REQUIRE(p == Path(0xF000000000000004u));
  REQUIRE(PathBitset(p.depth_shifted()) == PathBitset(15u));

  p.length = 5;
  p[5] = true;

  REQUIRE(p.left_aligned() == ((uint64_t)0b11111000u) << (64 - 8));
  REQUIRE(p.depth_shifted() == 31u);
}
