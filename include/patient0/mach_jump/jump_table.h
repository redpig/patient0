/* mach_jump.h
 * Copyright (c) 2009 Will Drewry <redpig@dataspill.org>. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file or at http://github.org/redpig/patient0.
 * vim:tw=80:ts=2:et:sw=2
 */
#ifndef PATIENT0_JUMP_TABLE_H_
#define PATIENT0_JUMP_TABLE_H_
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <mach-o/getsect.h>

typedef struct jump_table {
  intptr_t addr;
  uint32_t size;
  uint32_t reserved1;
  uint32_t reserved2;
} jump_table_t;

typedef struct __attribute__((packed)) {
  unsigned char opcode;
  intptr_t target;
} jmp_entry_t;

bool jump_table_init();
jump_table_t *jump_table_global();
intptr_t jump_table_find(jump_table_t *table, intptr_t fn_address);
intptr_t jump_table_find_by_symbol_address(jump_table_t *table, const char *symbol);
bool jump_table_patch(intptr_t entry_address, void *target);
bool jump_table_get_table(const char *framework, jump_table_t *table);
bool jump_table_get_indexed_table(uint32_t index, jump_table_t *table);

#endif  /* PATIENT0_JUMP_TABLE_H_ */
