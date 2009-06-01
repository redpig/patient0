/* mach_jump.h
 * Copyright (c) 2009 Will Drewry <redpig@dataspill.org>. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file or at http://github.org/redpig/patient0.
 * vim:tw=80:ts=2:et:sw=2
 */
#ifndef PATIENT0_IMAGE_INFO_H_
#define PATIENT0_IMAGE_INFO_H_
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <mach-o/getsect.h>

#include <patient0/mach_jump/jump_table.h>

bool image_info_initialize();
bool image_info_ready();
uint32_t image_info_count();
bool image_info_wait_until_ready();
bool image_info_jump_table(uint32_t index, jump_table_t *table);

#endif  /* PATIENT0_IMAGE_INFO_H_ */
