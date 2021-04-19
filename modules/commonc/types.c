#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "paracooba/common/types.h"

#include <parac_common_export.h>

PARAC_COMMON_EXPORT bool
parac_type_from_str(const char* str,
                    parac_type expected_type,
                    parac_type_union* tgt) {
  assert(str);
  switch(expected_type) {
    case PARAC_TYPE_UINT64:
      return sscanf(str, "%" PRIu64, &tgt->uint64) == 1;
    case PARAC_TYPE_INT64:
      return sscanf(str, "%" PRId64, &tgt->int64) == 1;
    case PARAC_TYPE_UINT32:
      return sscanf(str, "%" PRIu32, &tgt->uint32) == 1;
    case PARAC_TYPE_INT32:
      return sscanf(str, "%" PRId32, &tgt->int32) == 1;
    case PARAC_TYPE_UINT16:
      return sscanf(str, "%hu", &tgt->uint16) == 1;
    case PARAC_TYPE_INT16:
      return sscanf(str, "%h", &tgt->int16) == 1;
    case PARAC_TYPE_UINT8: {
      unsigned int scanned = 0;
      int r = sscanf(str, "%ud", &scanned);
      if(r != 1)
        return false;
      if(scanned > SCHAR_MAX)
        return false;
      tgt->uint8 = scanned;
      return true;
    }
    case PARAC_TYPE_INT8: {
      int scanned = 0;
      int r = sscanf(str, "%d", &scanned);
      if(r != 1)
        return false;
      if(scanned > SCHAR_MAX)
        return false;
      if(scanned < SCHAR_MIN)
        return false;
      tgt->int8 = scanned;
      return true;
    }
    case PARAC_TYPE_FLOAT:
      return sscanf(str, "%f", &tgt->f) == 1;
    case PARAC_TYPE_DOUBLE:
      return sscanf(str, "%lf" PRIu64, &tgt->d) == 1;
    case PARAC_TYPE_STR:
      tgt->string = str;
    case PARAC_TYPE_SWITCH:
      tgt->boolean_switch = true;
      return 1;
    case PARAC_TYPE_VECTOR_STR: {
      size_t i = 0;

      // Calculate size first, then create vector.
      char* str_copy = strdup(str);
      char* pch;
      pch = strtok(str_copy, " ");
      while(pch != NULL) {
        ++i;
        pch = strtok(NULL, " ");
      }

      str_copy = strdup(str);

      tgt->string_vector.size = i;
      tgt->string_vector.strings =
        (const char**)malloc(sizeof(const char*) * i);
      i = 0;

      pch = strtok(str_copy, " ");
      while(pch != NULL) {
        tgt->string_vector.strings[i++] = strdup(pch);
        pch = strtok(NULL, " ");
      }
      free(str_copy);
      return 1;
    }
  }
}
