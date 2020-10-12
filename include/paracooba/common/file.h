#ifndef PARAC_COMMON_FILE_H
#define PARAC_COMMON_FILE_H

#include "paracooba/common/status.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

typedef void (*parac_file_cb)(void*, parac_status);

typedef struct parac_file {
  char* path;
  void* userdata;
  parac_file_cb cb;
} parac_file;

/** @brief Free data in file struct.*/
void
parac_file_free(parac_file* file);

#ifdef __cplusplus
}

class parac_file_wrapper : public parac_file {
  public:
  parac_file_wrapper() {
    path = nullptr;
    userdata = nullptr;
    cb = nullptr;
  };
  parac_file_wrapper(const parac_file& f) {
    path = f.path;
    userdata = f.userdata;
    cb = f.cb;
  };
  ~parac_file_wrapper() { parac_file_free(this); }
};
#endif

#endif
