/* alzheimer.c
 *
 * Copyright (c) 2009 Will Drewry <redpig@dataspill.org>. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file or at http://github.org/redpig/patient0.
 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include <patient0/log.h>
#include <patient0/mach_jump.h>
#include <patient0/mach_jump/clobber.h>
#include <patient0/runtime.h>

static time_t current_time = 1243090562;

int alz_gettimeofday(struct timeval *tp, void *tzp) {
  /* a test date. TODO: make this an appended argument */
  tp->tv_sec = current_time++;
  return 0;
}

void alz_uuid_generate(uuid_t out) {
  memset(out, 0, sizeof(uuid_t));
}

void alz_uuid_generate_random(uuid_t out) {
  memset(out, 0, sizeof(uuid_t));
}

void alz_uuid_generate_time(uuid_t out) {
  memset(out, 0, sizeof(uuid_t));
}

static struct {
  char *name;
  void *addr;
} table[] = {
  { "gettimeofday", alz_gettimeofday },
  { "uuid_generate", alz_uuid_generate },
  { "uuid_generate_random", alz_uuid_generate_random },
  { "uuid_generate_time", alz_uuid_generate_time },
};
static int table_size = 4;

void run(unsigned char *code, uint32_t size) {
  int entry = 0;
  p0_logf(P0_INFO, "alzheimer running");
  /* install function replacements */
  mach_jump_init();
  for ( ; entry < table_size; ++entry) {
    //if (!mach_jump_patch_images(table[entry].name, table[entry].addr)) {
    if (!clobber_function_by_symbol(table[entry].name, &(table[entry].addr))) {
      p0_logf(P0_ERR, "failed to patch %s", table[entry].name);
    }
  }
 p0_logf(P0_INFO, "terminating");
  runtime_terminate();
}
