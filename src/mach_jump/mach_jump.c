/*
 * Copyright (c) 2009 Will Drewry <redpig@dataspill.org>. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file or at http://github.org/redpig/patient0.
 * vim:tw=80:ts=2:et:sw=2
 */
#include <assert.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <libkern/OSByteOrder.h>
#include <mach/mach_init.h>
#include <mach/vm_prot.h>
#include <mach/vm_map.h>
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
#include <mach-o/nlist.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <patient0/log.h>
#include <patient0/mach_jump.h>
#include <patient0/mach_jump/jump_table.h>
#include <patient0/mach_jump/lazy_symbol.h>
#include <patient0/mach_jump/clobber.h>



static bool mach_jump_initialized = false;

bool mach_jump_init() {
  if (mach_jump_initialized)
    return true;
  mach_jump_initialized = jump_table_init() &&
                          linkedit_init() &&
                          lazy_symbol_init();
  return mach_jump_initialized;
}

bool mach_jump_patch(const char *symbol, void *replacement_fn) {
  intptr_t entry = lazy_symbol_stub(symbol);
  if (!entry && (entry = jump_table_find_by_symbol_address(NULL, symbol)) < 0) {
    return false;
  }
  p0_logf(P0_INFO, "patching %s @ %p with %p\n", symbol, entry, replacement_fn);
  return jump_table_patch(entry, replacement_fn);
}

bool mach_jump_unpatch(const char *symbol) {
  intptr_t entry = lazy_symbol_stub(symbol);
  void *real_func = dlsym(RTLD_DEFAULT, symbol);
  if (!entry && (entry = jump_table_find_by_symbol_address(NULL, symbol)) < 0) {
    p0_logf(P0_ERR, "symbol not found in jump table");
    return false;
  }
  if (!real_func) {
    p0_logf(P0_ERR, "symbol not found by dlsym()");
    return false;
  }
  return jump_table_patch(entry, real_func);
}

bool mach_jump_framework_unpatch(const char *framework,
                                 const char *symbol,
                                 void *replacement) {
  intptr_t entry = 0;
  void *real_func = dlsym(RTLD_DEFAULT, symbol);
  jump_table_t table = { 0 };

  if (!jump_table_get_table(framework, &table)) {
    p0_logf(P0_ERR, "failed to acquire jump table for '%s'", framework);
    return false;
  }
  if (table.addr == 0) {
    p0_logf(P0_ERR, "framework '%s' mapped at PAGE_ZERO. Unlikely.", framework);
    return false;
  }
  entry = jump_table_find(&table, (intptr_t)replacement);
  if (entry == -1) {
    p0_logf(P0_ERR, "failed to find address '%p' in table", replacement);
    return false;
  }
  return jump_table_patch(entry, real_func);
}

bool mach_jump_framework_patch(const char *framework,
                               const char *symbol,
                               void *replacement) {
  jump_table_t table = { 0 };
  void *addr = dlsym(RTLD_DEFAULT, symbol);
  intptr_t entry = 0;

  if (!jump_table_get_table(framework, &table)) {
    p0_logf(P0_ERR, "failed to acquire jump table for '%s'", framework);
    return false;
  }
  if (table.addr == 0) {
    p0_logf(P0_ERR, "framework '%s' mapped at PAGE_ZERO. Unlikely.", framework);
    return false;
  }
  entry = jump_table_find(&table, (intptr_t)addr);
  if (entry == -1) {
    p0_logf(P0_ERR, "failed to find address '%p' in table", addr);
    return false;
  }

  if (!jump_table_patch(entry, replacement)) {
    p0_logf(P0_INFO,
            "failed to patch '%p' for '%s' in '%s'",
            entry,
            symbol,
            framework);
    return false;
  }
  return true;
}
