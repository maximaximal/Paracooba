#include <catch2/catch.hpp>

#include <paracooba/common/linked_list.h>

extern "C" {
#include <stdlib.h>
PARAC_LINKED_LIST(int, int)
}

TEST_CASE("Inserting Elements into linked list", "[commonc][linked_list]") {
  parac_int_list test_list;
  parac_int_list_init(&test_list);

  REQUIRE(test_list.size == 0);
  REQUIRE(test_list.first == NULL);

  int* entry1 = parac_int_list_alloc_new(&test_list);

  REQUIRE(test_list.size == 1);
  REQUIRE(&test_list.first->entry == entry1);
  REQUIRE(test_list.first->next == NULL);

  int* entry2 = parac_int_list_alloc_new(&test_list);

  REQUIRE(test_list.size == 2);
  REQUIRE(&test_list.first->entry == entry2);
  REQUIRE(&test_list.first->next->entry == entry1);

  *entry1 = 1;
  *entry2 = 2;

  int* entry3 = parac_int_list_alloc_new(&test_list);
  *entry3 = 3;

  REQUIRE(entry1 != entry2);
  REQUIRE(entry2 != entry3);
  REQUIRE(*entry1 == 1);
  REQUIRE(*entry2 == 2);
  REQUIRE(*entry3 == 3);

  REQUIRE(test_list.size == 3);

  parac_int_list_free(&test_list);

  REQUIRE(test_list.size == 0);
  REQUIRE(test_list.first == NULL);
}
