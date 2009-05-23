/*
 *
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
#include <patient0/mach_jump/jump_table.h>

static jump_table_t global_jump_table = { 0 };
static bool jump_table_initialized = false;

jump_table_t *jump_table_global() {
  return &global_jump_table;
}

/* Initializes the global_jump_table and ensures we are going to
 * be compatible (jump entry size)
 */
bool jump_table_init() {
  const struct section *jump_table;
  if (jump_table_initialized) return true;
  /* This only addresses lazy pointers AFAICT.
   */
  if ((jump_table = getsectbyname("__IMPORT",  "__jump_table")) != 0) {
    if (jump_table->reserved2 != sizeof(jmp_entry_t)) {
      p0_logf(P0_INFO, "entry sizes in jump table do not match jmp_entry_t size\n");
      return false;
    }
    global_jump_table.addr = jump_table->addr;
    global_jump_table.size = jump_table->size;
    global_jump_table.reserved1 = jump_table->reserved1;
    global_jump_table.reserved2 = jump_table->reserved2;
    jump_table_initialized = true;
  }
  return jump_table_initialized;
}


/* jump_table_get_table
 * Returns the jump table (symbol stubs) used by a given, loaded framework.
 * This allows for symbol stub patching when calls are made from functions
 * resident in a loaded shared library.  The canonical example is the
 * SecurityFoundation Obj-C functions calling Security.framework functions.
 * (see pathogens/rubella.c)
 */
bool jump_table_get_table(const char *framework, jump_table_t *table) {
  unsigned long size = 0;
  table->addr = (intptr_t) getsectdatafromFramework(framework,
                                                    "__IMPORT",
                                                    "__jump_table",
                                                    (unsigned long *)
                                                      &table->size);
  /* XXX: none of this code is 64-bit clean */
  p0_logf(P0_INFO, "framework: %s addr %p", framework, table->addr);
  if (table->addr == 0)
    return false;
  /* Make sure we can patch the table */
  if (vm_protect(mach_task_self(),
                 (vm_address_t)table->addr,
                 table->size,
                 false,
                 VM_PROT_ALL) != KERN_SUCCESS) {
    /* we will keep on truckin' though. just in case! */
    p0_logf(P0_ERR, "failed to change the protections on the jump table");
  }
  return true;
}


/* jmp_table_find (and jmp_table_find_by_symbol)
 * Takes in a symbol and attempts to resolve the symbol and find its
 * lazy resolution address in the executable images __jump_table
 * Returns -1 on error.
 * This will only find entries which have already been called once
 * (and have therefore been resolved by dyld) unless the executable
 * was launched with the DYLD_BIND_AT_LAUNCH environment variable
 * set.  If set, then any symbols which are referenced in the code will
 * be bound at link-time and show up here :)
 */
intptr_t jump_table_find(jump_table_t *table, intptr_t func) {
  jmp_entry_t *cursor = NULL;
  jmp_entry_t *eot = NULL;

  if (!jump_table_initialized && !jump_table_init()) {
    p0_logf(P0_ERR, "jump_table uninitialized");
    return -1;
  }

  if (!func) {
    p0_logf(P0_INFO, "symbol not found\n");
    return -1;
  }

  if (table == NULL) {
    table = jump_table_global();
  }

  /* Assume the jump table is comprised of 5 byte entries only.
   * If this assumption is later violated, a lightweight x86 disasm
   * will make it easy enough to scan the table.
   */
  for (cursor = (jmp_entry_t *)table->addr,
       eot = cursor + (table->size / sizeof(jmp_entry_t));
       cursor < eot;
       ++cursor) {
    intptr_t jmps_to;
    /* dyld only seems to use call (e8) when it is calling
     * __dyld_fast_stub_binding_helper_interface.  This assumption is probably
     * wrong.
     */
    if (cursor->opcode == 0xe8) {
      continue;
    }
    if (cursor->opcode != 0xe9) {
      p0_logf(P0_INFO, "unknown opcode (%hhx) @ %p\n", cursor->opcode, cursor);
    }
    /* make the target address absolute */
    jmps_to = cursor->target +  ((intptr_t)(cursor) + sizeof(jmp_entry_t));
    p0_logf(P0_INFO, "%p: %hhx %x %x\n", cursor,
                               cursor->opcode,
                               (uint32_t) cursor->target,
                               (uint32_t)jmps_to);
    if (jmps_to == (intptr_t)func) {
      return (intptr_t) cursor;
    }
  }
  p0_logf(P0_INFO, "jmp entry not found\n");
  //printf("%p\n", dyld_fast_stub_binding_helper_interface);
  return -1;
}

intptr_t jump_table_find_by_symbol_address(jump_table_t *table,
                                           const char *symbol) {
  void *func_location = dlsym(RTLD_DEFAULT, symbol);
  if (!func_location) {
    p0_logf(P0_INFO, "symbol not found\n");
    return -1;
  }
  return jump_table_find(table, (intptr_t)func_location);
}

bool jump_table_patch(intptr_t entry_address, void *target) {
  jmp_entry_t *entry = (jmp_entry_t *) entry_address;
  if (entry == NULL) {
    return false;
  }

  /* TODO: fix  protections */
  p0_logf(P0_INFO, "patching %p from %p to (%p)", entry, entry->target, target);
  /* Calculate the relative signed integer offset argument to 0xe9 */
  entry->target = ((intptr_t)target) - (entry_address + sizeof(jmp_entry_t));
  /* This allows us to patch unbound symbols too. */
  if (entry->opcode != 0xe9) {
    entry->opcode = 0xe9;
  }
  return true;
}
