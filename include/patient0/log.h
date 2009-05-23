/*
 * Copyright (c) 2009 Will Drewry <redpig@dataspill.org>. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file or at http://github.org/redpig/patient0.
 */
#ifndef PATIENT0_LOG_H_
#define PATIENT0_LOG_H_

#include <stdio.h>
#include <stdlib.h>

#define P0_INFO 1
#define P0_ERR 128
#define P0_FATAL 256

#ifndef P0_LOG_CUTOFF
#  ifdef NDEBUG
#    define P0_LOG_CUTOFF 255
#  elif defined(P0_VERBOSE)
#    define P0_LOG_CUTOFF 0
#  else
#    define P0_LOG_CUTOFF 127
#  endif
#endif


#ifdef P0_LOG_FILE
extern FILE *p0_log_file;
#  define P0_LOG_STREAM p0_log_file
#else
#  define P0_LOG_STREAM stderr
#endif

#define p0_logf(_level, _format, ...) \
  if (_level > P0_LOG_CUTOFF && P0_LOG_STREAM) { \
    fprintf(P0_LOG_STREAM, "%s:%d:%s: " _format "\n", \
            __FILE__, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__); \
    fflush(P0_LOG_STREAM); \
    if (_level >= P0_FATAL) { abort(); } \
   }

#define p0_logf_if(_level, _cond, _format, ...) \
  if (_level > P0_LOG_CUTOFF) { \
    if ( (_cond) ) { \
      p0_logf(_level, _format, ##__VA_ARGS__); \
    } \
  }

#endif  /* PATIENT0_LOG_H_ */
