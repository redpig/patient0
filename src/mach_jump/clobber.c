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

/* clobber_function_by_symbol
 *
 * This is heinous, but functional.  It looks up the given
 * symbol in the currently loaded application, then overwrites
 * the first 6 bytes with a jmp to our replacement.
 * Issues:
 * - non-atomic. can be fixed with an cmpxchg8b
 * - non-reversible. could save a backup
 * - no easy way to access the original without unpatching.
 * Arguments:
 * - sym: symbol name to be resolved by dlsym
 * - ptr_to_addrptr: the address of a global pointer which contains the
 *                   address of the replacement function‥ E.g.,
 *                     void * repl_fn = &my_replacement;
 *                     clobber...("symbol", (intptr_t)&repl_fn);
 */
int clobber_function_by_symbol(const char *sym, intptr_t ptr_to_addrptr) {
  char *addr = NULL;
  addr = dlsym(RTLD_DEFAULT, sym);
  /* We make the original function address space writeable.
   * You'd expect this to need alignment and maybe a good minimum
   * size based on the page size, but you'd be wrong.
   */
  if (addr && !vm_protect(mach_task_self(),
                          (vm_address_t)addr,
                          6,  // 6 bytes really... (32-bit)
                          0,
                          0x1|0x2|0x4)) {
    /* Write the opcode for >jmp [dword] */
    *addr++ = '\xff';
    *addr++ = '\x25';
    /* Write the global memory which will have the fn bounce addr. */
    memcpy(addr, &ptr_to_addrptr, sizeof(intptr_t));
    return 0;
  }
  return 1;
}

