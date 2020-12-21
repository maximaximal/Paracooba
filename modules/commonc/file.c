#include <assert.h>
#include <paracooba/common/file.h>
#include <stdlib.h>

#include <parac_common_export.h>

PARAC_COMMON_EXPORT void
parac_file_free(parac_file* file) {
  assert(file);
  assert(file->path);
}
