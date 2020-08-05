#ifndef PARACOOBA_COMMON_TYPES_H
#define PARACOOBA_COMMON_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef uint64_t parac_id;

typedef struct parac_string_vector {
  const char** strings;
  size_t size;
} parac_string_vector;

typedef enum parac_type {
  PARAC_TYPE_UINT64,
  PARAC_TYPE_INT64,
  PARAC_TYPE_UINT32,
  PARAC_TYPE_INT32,
  PARAC_TYPE_UINT16,
  PARAC_TYPE_INT16,
  PARAC_TYPE_UINT8,
  PARAC_TYPE_INT8,
  PARAC_TYPE_FLOAT,
  PARAC_TYPE_DOUBLE,
  PARAC_TYPE_STR,
  PARAC_TYPE_VECTOR_STR,
} parac_type;

typedef union parac_type_union {
  uint64_t uint64;
  int64_t int64;
  uint32_t uint32;
  int32_t int32;
  uint16_t uint16;
  int16_t int16;
  uint8_t uint8;
  int8_t int8;
  float f;
  double d;
  const char* string;
  parac_string_vector string_vector;
} parac_type_union;

#define _PARAC_TYPE_GET_ASSERT_uint64(UNION, TYPE) \
  assert(TYPE == PARAC_TYPE_UINT64);
#define _PARAC_TYPE_GET_ASSERT_int64(UNION, TYPE) \
  assert(TYPE == PARAC_TYPE_INT64);
#define _PARAC_TYPE_GET_ASSERT_uint32(UNION, TYPE) \
  assert(TYPE == PARAC_TYPE_UINT32);
#define _PARAC_TYPE_GET_ASSERT_int32(UNION, TYPE) \
  assert(TYPE == PARAC_TYPE_INT32);
#define _PARAC_TYPE_GET_ASSERT_uint16(UNION, TYPE) \
  assert(TYPE == PARAC_TYPE_UINT16);
#define _PARAC_TYPE_GET_ASSERT_int16(UNION, TYPE) \
  assert(TYPE == PARAC_TYPE_INT16);
#define _PARAC_TYPE_GET_ASSERT_uint8(UNION, TYPE) \
  assert(TYPE == PARAC_TYPE_UINT8);
#define _PARAC_TYPE_GET_ASSERT_int8(UNION, TYPE) \
  assert(TYPE == PARAC_TYPE_INT8);
#define _PARAC_TYPE_GET_ASSERT_f(UNION, TYPE) assert(TYPE == PARAC_TYPE_FLOAT);
#define _PARAC_TYPE_GET_ASSERT_d(UNION, TYPE) assert(TYPE == PARAC_TYPE_DOUBLE);
#define _PARAC_TYPE_GET_ASSERT_string(UNION, TYPE) \
  assert(TYPE == PARAC_TYPE_STRING);
#define _PARAC_TYPE_GET_ASSERT_string_vector(UNION, TYPE) \
  assert(TYPE == PARAC_TYPE_STRING_VECTOR);

#define PARAC_TYPE_GET(UNION, TYPE, MEMBER) \
  _PARAC_TYPE_GET_ASSERT_##MEMBER(UNION, TYPE) \ UNION.MEMBER

#ifdef __cplusplus
}
#endif

#endif
