#include <assert.h>
#include <paracooba/common/file.h>
#include <stdlib.h>

void
parac_file_free(parac_file* file) {
  assert(file);
  assert(file->path);
  free(file->path);
}
