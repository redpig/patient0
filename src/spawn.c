/*
 * Copyright (c) 2009 Will Drewry <redpig@dataspill.org>. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file or at http://github.org/redpig/patient0.
 * vim:tw=80:ts=2:et:sw=2
 */
#include <mach/mach.h>
#include <mach/mach_types.h>
#include <errno.h>
#include <sched.h>
#include <string.h>
#include <stdbool.h>
#include <sys/signal.h>
#include <unistd.h>

#include <patient0/log.h>
#include <patient0/spawn.h>

/* This file is heavily based upon the work released on
 * Michael Weber's blog post:
 * http://www.foldr.org/~michaelw/lo g/computers/macosx/task-info-fun-with-mach
 * As well as NetBSD's src/sys/compat/mach/mach_port.h and
 * Darwin's CFMessagePort.c.
 */
static bool create_mach_receive_port(mach_port_t *port) {
  kern_return_t ret;
  mach_port_t self = mach_task_self();

  if (!port) {
    return false;
  }

  ret = mach_port_allocate(self, MACH_PORT_RIGHT_RECEIVE, port);
  p0_logf_if(P0_ERR, ret, "mach_port_allocate() -> %d", ret);

  ret |= mach_port_insert_right(self, *port, *port, MACH_MSG_TYPE_MAKE_SEND);
  p0_logf_if(P0_ERR, ret, "mach_port_insert_right() -> %d", ret);

  if (ret != 0) {
    mach_port_deallocate(self, *port);
    return false;
  }
  return true;
}


/* struct which encapsulates the mach msg header and the accompanying
 * payload of a mach task port.
 */
struct mach_send_port_msg {
    mach_msg_header_t header;
    mach_msg_body_t body;
    mach_msg_port_descriptor_t task_port;
};

struct mach_receive_port_msg {
  struct mach_send_port_msg m;
  mach_msg_trailer_t trailer;
};



static bool send_mach_port(mach_port_t dst_port, mach_port_t port) {
  struct mach_send_port_msg msg = { 0 };

  /* Populate the mach msg header */
  msg.header.msgh_remote_port = dst_port;
  msg.header.msgh_local_port = MACH_PORT_NULL;  /* no src port is required */
  msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
  msg.header.msgh_bits |= MACH_MSGH_BITS_COMPLEX;
  msg.header.msgh_size = sizeof(msg);
  msg.body.msgh_descriptor_count = 1;
  /* Duplicate the port in to the target mach task's port namespace */
  msg.task_port.name = port;
  msg.task_port.disposition = MACH_MSG_TYPE_COPY_SEND;
  msg.task_port.type = MACH_MSG_PORT_DESCRIPTOR;

  if (mach_msg_send(&msg.header)) {
    p0_logf(P0_ERR, "failed to send task port to parent");
    return false;
  }
  return true;
}

static bool receive_mach_port(mach_port_t recv_port, mach_port_t *port) {
  struct mach_receive_port_msg  msg;
  if (mach_msg(&msg.m.header,    /* header */
               MACH_RCV_MSG,     /* option */
               0,                /* send size */
               sizeof(msg),      /* receive limit */
               recv_port,        /* receive port */
               MACH_MSG_TIMEOUT_NONE,
               MACH_PORT_NULL) != KERN_SUCCESS ) {  /* notify port */
    p0_logf(P0_ERR, "failed to receive the expected mach port");
    return false;
  }

  *port = msg.m.task_port.name;
  return true;
}

static void execute(const char *path,
                    char **argv,
                    char **envp,
                    pid_t pid) {
  /* ensure we are running in a valid directory */
  chdir("/");
  /* separate from the parent process */
  setsid();
  /* redirect stdin/out/err unless we want logging */
#ifndef P0_VERBOSE
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
#endif 
  p0_logf(P0_INFO, "running '%s'", path);
  if (pid > 0) {
    kill(pid, SIGKILL);
  }
  execve(path, argv, envp);
  p0_logf(P0_INFO, "execve failed: %s", strerror(errno));
}

static bool send_task_port(mach_port_t remote_port) {
  if (!send_mach_port(remote_port, mach_task_self())) {
    p0_logf(P0_ERR, "failed to send task port");
    return false;
  }
  return true;
}

static bool reset_bootstrap(mach_port_t local_port, mach_port_t remote_port) {
  if (!send_mach_port(remote_port, local_port)) {
    p0_logf(P0_ERR, "failed to send receive port");
    return false;
  }
  if (!receive_mach_port(remote_port, &bootstrap_port)) {
    p0_logf(P0_ERR, "failed to receive bootstrap port");
    return false;
  }
  if (bootstrap_port == MACH_PORT_NULL) {
    p0_logf(P0_ERR, "received NULL bootstrap port");
    return false;
  }
  if (task_set_bootstrap_port(mach_task_self(), bootstrap_port)) {
    p0_logf(P0_ERR, "failed to set the proper bootstrap_port");
    return false;
  }
  return true;
}

/* TODO: add flags to control whether we redirect std*, setsid, etc */
pid_t spawn(const char *path,
            char **argv,
            char **envp,
            mach_port_t *taskport,
            pid_t pid) {
  pid_t child_pid = -1;

  /* Create a port to receive the child's task port on */
  mach_port_t recv_port = MACH_PORT_NULL;
  if (!create_mach_receive_port(&recv_port)) {
    p0_logf(P0_ERR, "failed to allocate a recv port");
    return -1;
  }

  /* Set the port to the bootstrap port. This is the only
   * mach port inherited on exec*() calls.
   */
  if (task_set_bootstrap_port(mach_task_self(), recv_port)) {
    p0_logf(P0_ERR, "failed to register recv_port as a bootstrap port\n");
    mach_port_deallocate(mach_task_self(), recv_port);
    return -1;
  }
  /* Now we can fork and grab the child's task port */
  if ((child_pid = fork()) == -1) {
    p0_logf(P0_ERR, "failed to fork!");
    return -1;
  } else if (child_pid == 0) {  /* child */
    mach_port_t remote_port = MACH_PORT_NULL;

    if (task_get_bootstrap_port(mach_task_self(), &remote_port)) {
      p0_logf(P0_FATAL, "failed to get bootstrap port");
    }
    if (!send_task_port(remote_port)) {
      p0_logf(P0_ERR, "failed to send task port to parent");
    }
    execute(path, argv, envp, pid);
    p0_logf(P0_FATAL, "execute has failed");
  } else {  /* parent */
    /* Restore our bootstrap port */
    if (task_set_bootstrap_port(mach_task_self(), bootstrap_port)) {
      p0_logf(P0_ERR, "failed to reset parent bootstrap port.");
      return -1;
    }
    /* Grab the task port and send our original bootstrap port
     * so that the process isn't weirdly orphaned */
    if (!receive_mach_port(recv_port, taskport)) {
      p0_logf(P0_ERR, "failed to receive the child task port");
      return -1;
    }
    /* Reset the child's bootstrap port forcibly. There is a race,
     * but probably before the child needs the original port, we'll
     * have replaced it. Maybe?
     */
    if (task_set_bootstrap_port(*taskport, bootstrap_port)) {
      p0_logf(P0_ERR, "failed to reset child bootstrap port.");
      return -1;
    }
    sched_yield();
  }
  return child_pid;
}


#ifdef SPAWN_MAIN
#include <stdlib.h>

int main(int argc, char **argv, char **envp) {
  pid_t pid = atoi(*(++argv)), child_pid;
  argv++;
  mach_port_t child = MACH_PORT_NULL;
  if (argc < 3) {
    printf("Usage:\nspawn_main <pid> </path/to/binary> [<args>]\n");
    return -1;
  }
  printf("(re)spawning: %s -> %d\n", argv[0], spawn(argv[0], argv, envp, &child, pid));
  if (child != MACH_PORT_NULL) {
    sleep(1);
    printf("Attempting to suspend...\n");
    task_suspend(child);
  }
}
#endif

