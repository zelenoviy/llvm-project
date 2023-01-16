// -*- C++ -*-
//===-----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP_SUPPORT_HAIKU_XLOCALE_H
#define _LIBCPP_SUPPORT_HAIKU_XLOCALE_H

#define _DEFAULT_SOURCE

#include <__support/xlocale/__strtonum_fallback.h>
#include <stdlib.h>
#include <wchar.h>
//#include <bsd/stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

int vasprintf(char **strp, const char *fmt, va_list ap);

/*
inline _LIBCPP_HIDE_FROM_ABI int
vasprintf(char **strp, const char *fmt, va_list ap) {
  const size_t buff_size = 256;
  if ((*strp = (char *)malloc(buff_size)) == NULL) {
    return -1;
  }

  va_list ap_copy;
  // va_copy may not be provided by the C library in C++ 03 mode.
#if defined(_LIBCPP_CXX03_LANG) && __has_builtin(__builtin_va_copy)
  __builtin_va_copy(ap_copy, ap);
#else
  va_copy(ap_copy, ap);
#endif
  int str_size = vsnprintf(*strp, buff_size, fmt,  ap_copy);
  va_end(ap_copy);

  if ((size_t) str_size >= buff_size) {
    if ((*strp = (char *)realloc(*strp, str_size + 1)) == NULL) {
      return -1;
    }
    str_size = vsnprintf(*strp, str_size + 1, fmt,  ap);
  }
  return str_size;
}
*/


#ifdef __cplusplus
}
#endif

#endif
