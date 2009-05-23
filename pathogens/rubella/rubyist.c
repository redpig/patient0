/* rubyist.c: proof of concept bundle that executes an appended ruby script.
 *
 * Copyright (c) 2009 Will Drewry <redpig@dataspill.org>. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file or at http://github.org/redpig/patient0.
 */
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <ruby.h>  /* Ruby.framework */

#include <patient0/runtime.h>
#include <patient0/log.h>

EXTERN VALUE rb_progname;
EXTERN VALUE rb_argv0;

void run(char *code, uint32_t size) {
  RUBY_INIT_STACK
  uint32_t script_size = PATIENT0_PAYLOAD_SIZE(code, size);
  char *script = PATIENT0_PAYLOAD(code, size);
  ruby_init();
  ruby_init_loadpath(); /* Ruby does ship on all macs :) */
  p0_logf(P0_INFO, "code: %p", code);
  p0_logf(P0_INFO, "code size: %d", size);
  p0_logf(P0_INFO, "script size: %d", script_size);
  p0_logf(P0_INFO, "script: %p", script);
  if (script) {
    script[script_size] = '\0';
    ruby_script("patient0");
    rb_argv0 = rb_progname;
    p0_logf(P0_INFO, "evaluating script");
    rb_eval_string(script);
    p0_logf(P0_INFO, "finalizing ruby environment");
    ruby_finalize();
  }
  p0_logf(P0_INFO, "deadlocking");
  runtime_deadlock();
}
