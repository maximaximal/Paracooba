#ifndef PARACOOBA_COMMON_RANDOM_H
#define PARACOOBA_COMMON_RANDOM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

uint32_t
parac_uint32_normal_distribution(uint32_t mean, double s);

int64_t
parac_int64_normal_distribution(int64_t mean, double s);

size_t
parac_size_normal_distribution(size_t mean, double s);

uint32_t
parac_uint32_uniform_distribution(uint32_t min, uint32_t max);

int64_t
parac_int64_uniform_distribution(int64_t min, int64_t max);

size_t
parac_size_uniform_distribution(size_t min, size_t max);

#ifdef __cplusplus
}
#endif

#endif
