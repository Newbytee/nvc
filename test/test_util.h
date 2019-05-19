#ifndef _TEST_UTIL_H
#define _TEST_UTIL_H

#include "util.h"
#include "tree.h"

#include <check.h>

BEGIN_C_HEADER

#define parse_and_check(...) ({                                    \
         static const tree_kind_t array[] = { __VA_ARGS__ };       \
         _parse_and_check(array, ARRAY_LEN(array), false, false);  \
      })

#define parse_check_and_simplify(...) ({                           \
         static const tree_kind_t array[] = { __VA_ARGS__ };       \
         _parse_and_check(array, ARRAY_LEN(array), true, false);   \
      })

#define parse_check_simplify_and_lower(...) ({                     \
         static const tree_kind_t array[] = { __VA_ARGS__ };       \
         _parse_and_check(array, ARRAY_LEN(array), true, true);    \
      })

typedef struct {
   int        line;
   const char *snippet;
} nvc_error_t;

void expect_errors(const nvc_error_t *lines);
TCase *nvc_unit_test(const char *name);
int nvc_run_test(Suite *s);
tree_t run_elab(void);
tree_t _parse_and_check(const tree_kind_t *array, int num,
                        bool simp, bool lower);
void nvc_add_bytecode_fixture(TCase *tc);

END_C_HEADER

#endif  // _TEST_UTIL_H
