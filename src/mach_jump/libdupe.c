/*
 * DO NOT USE. THIS IS UNFINISHED AND DOES NOT WORK.
 * PULL IN MACH_STAR IF YOU NEED DIRECT FUNCTION OVERRIDES.
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
#include <mach/mach_vm.h>
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
#include <mach-o/nlist.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <patient0/log.h>
#include <patient0/mach_jump/libdupe.h>

/* Find the section and return its info */
bool libdupe_dupe(const char *symbol, libdupe_entry_t *entry) {
  void *sym = dlsym(RTLD_DEFAULT, symbol);
  vm_address_t address = (vm_address_t)sym;
  vm_size_t size = 0;
  uint32_t image_index = 0;
  vm_region_basic_info_data_t basic_info;
  mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT;
  mach_port_t object_name = MACH_PORT_NULL;
  if (!sym || !entry) {
    return false;
  }
  /* Let's grab the region */
  p0_logf(P0_INFO, "attempting to find the region for %p", address);
  if (vm_region(mach_task_self(), &address, &size,
                VM_REGION_BASIC_INFO, (vm_region_info_t) &basic_info,
                &info_count, &object_name) != KERN_SUCCESS) {
    p0_logf(P0_ERR, "failed to load vm_region information");
    return false;
  }
  p0_logf(P0_INFO, "found region at %p with size %d", address, size);
  p0_logf(P0_INFO, "prot = %d", basic_info.protection);
  p0_logf(P0_INFO, "offset = %d", basic_info.offset);
  /* Well that was easy. Let's dupe it and populate our return structure and
   * hope for the best.
   */
  entry->original_base = (uint8_t *)address;
  entry->size = size;
  p0_logf(P0_INFO, "attempting to allocate some space");
  if (vm_allocate(mach_task_self(), &address, size, 1) != KERN_SUCCESS) {
    p0_logf(P0_ERR, "failed to create allocation");
    return false;
  }
  entry->base = (uint8_t *)address;
  p0_logf(P0_INFO, "memory allocated @ %p", entry->base);
  /* TODO: Add vm_protect here if needed */
  if (vm_write(mach_task_self(),
               address,
               (vm_offset_t) entry->original_base,
               entry->size) != KERN_SUCCESS) {
    p0_logf(P0_ERR, "failed to copy the region. bollocks.");
    return false;
  }
  p0_logf(P0_INFO, "libdupe completed!");
  return true;
}

