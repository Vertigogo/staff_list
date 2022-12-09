
#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import atexit
import errno
import os
import sys
import time
import signal
import logging
import argparse
from collections import Sequence

path = os.path.dirname(os.path.abspath(__file__))

class Etrigan(object):
        """
        Daemonizer based on double-fork method
        --------------------------------------
        Each option can be passed as a keyword argument or modified by assigning
        to an attribute on the instance:

        jasonblood = Etrigan(pidfile,
                             argument_example_1 = foo,
                             argument_example_2 = bar)

        that is equivalent to:

        jasonblood = Etrigan(pidfile)
        jasonblood.argument_example_1 = foo
        jasonblood.argument_example_2 = bar

        Object constructor expects always `pidfile` argument.
        `pidfile`
                Path to the pidfile.

        The following other options are defined:
        `stdin`
        `stdout`
        `stderr`
                :Default: `os.devnull`
                        File objects used as the new file for the standard I/O streams
                        `sys.stdin`, `sys.stdout`, and `sys.stderr` respectively.

        `funcs_to_daemonize`
                :Default: `[]`
                        Define a list of your custom functions
                        which will be executed after daemonization.
                        If None, you have to subclass Etrigan `run` method.
                        Note that these functions can return elements that will be
                        added to Etrigan object (`etrigan_add` list) so the other subsequent
                        ones can reuse them for further processing.
                        You only have to provide indexes of `etrigan_add` list,
                        (an int (example: 2) for single index or a string (example: '1:4') for slices)
                        as first returning element.

        `want_quit`
                :Default: `False`
                        If `True`, runs Etrigan `quit_on_start` or `quit_on_stop`
                        lists of your custom functions at the end of `start` or `stop` operations.
                        These can return elements as `funcs_to_daemonize`.

        `logfile`
                :Default: `None`
                        Path to the output log file.

        `loglevel`
                :Default: `None`
                        Set the log level of logging messages.

        `mute`
                :Default: `False`
                        Disable all stdout and stderr messages (before double forking).

        `pause_loop`
                :Default: `None`
                        Seconds of pause between the calling, in an infinite loop,
                        of every function in `funcs_to_daemonize` list.
                        If `-1`, no pause between the calling, in an infinite loop,
                        of every function in `funcs_to_daemonize` list.
                        If `None`, only one run (no infinite loop) of functions in
                        `funcs_to_daemonize` list, without pause.
        """

        def __init__(self, pidfile,
                     stdin = os.devnull, stdout = os.devnull, stderr = os.devnull,
                     funcs_to_daemonize = [], want_quit = False,
                     logfile = None, loglevel = None,
                     mute = False, pause_loop = None):

                self.pidfile = pidfile
                self.funcs_to_daemonize = funcs_to_daemonize
                self.stdin = stdin
                self.stdout = stdout
                self.stderr = stderr
                self.logfile = logfile
                self.loglevel = loglevel
                self.mute = mute
                self.want_quit = want_quit
                self.pause_loop = pause_loop
                # internal only.
                self.homedir = '/'
                self.umask = 0o22
                self.etrigan_restart, self.etrigan_reload = (False for _ in range(2))
                self.etrigan_alive = True
                self.etrigan_add = []
                self.etrigan_index = None
                # seconds of pause between stop and start during the restart of the daemon.
                self.pause_restart = 5
                # when terminate a process, seconds to wait until kill the process with signal.
                # self.pause_kill = 3

                # create logfile.
                self.setup_files()

        def handle_terminate(self, signum, frame):
                if os.path.exists(self.pidfile):
                        self.etrigan_alive = False
                        # eventually run quit (on stop) function/s.
                        if self.want_quit:
                                if not isinstance(self.quit_on_stop, (list, tuple)):
                                        self.quit_on_stop = [self.quit_on_stop]
                                self.execute(self.quit_on_stop)
                        # then always run quit standard.
                        self.quit_standard()
                else:
                        self.view(self.logdaemon.error, self.emit_error, "Failed to stop the daemon process: can't find PIDFILE '%s'" %self.pidfile)
                sys.exit(0)

        def handle_reload(self, signum, frame):
                self.etrigan_reload = True

        def setup_files(self):
                self.pidfile = os.path.abspath(self.pidfile)

                if self.logfile is not None:
                        self.logdaemon = logging.getLogger('logdaemon')
                        self.logdaemon.setLevel(self.loglevel)

                        filehandler = logging.FileHandler(self.logfile)
                        filehandler.setLevel(self.loglevel)
                        formatter = logging.Formatter(fmt = '[%(asctime)s] [%(levelname)8s] --- %(message)s',
                                                      datefmt = '%Y-%m-%d %H:%M:%S')
                        filehandler.setFormatter(formatter)
                        self.logdaemon.addHandler(filehandler)
                else:
                        nullhandler = logging.NullHandler()
                        self.logdaemon.addHandler(nullhandler)

        def emit_error(self, message, to_exit = True):
                """ Print an error message to STDERR. """
                if not self.mute:
                        sys.stderr.write(message + '\n')
                        sys.stderr.flush()
                if to_exit:
                        sys.exit(1)

        def emit_message(self, message, to_exit = False):
                """ Print a message to STDOUT. """
                if not self.mute:
                        sys.stdout.write(message + '\n')
                        sys.stdout.flush()
                if to_exit:
                        sys.exit(0)

        def view(self, logobj, emitobj, msg, **kwargs):
                options = {'to_exit' : False,
                           'silent' : False
                           }
                options.update(kwargs)

                if logobj:
                        logobj(msg)
                if emitobj:
                        if not options['silent']:
                                emitobj(msg, to_exit = options['to_exit'])

        def daemonize(self):
                """
                Double-forks the process to daemonize the script.
                see Stevens' "Advanced Programming in the UNIX Environment" for details (ISBN 0201563177)
                http://www.erlenstar.demon.co.uk/unix/faq_2.html#SEC16
                """
                self.view(self.logdaemon.debug, None, "Attempting to daemonize the process...")

                # First fork.
                self.fork(msg = "First fork")
                # Decouple from parent environment.
                self.detach()
                # Second fork.
                self.fork(msg = "Second fork")
                # Write the PID file.
                self.create_pidfile()
                self.view(self.logdaemon.info, self.emit_message, "The daemon process has started.")
                # Redirect standard file descriptors.
                sys.stdout.flush()
                sys.stderr.flush()
                self.attach('stdin', mode = 'r')
                self.attach('stdout', mode = 'a+')

                try:
                        self.attach('stderr', mode = 'a+', buffering = 0)
                except ValueError:
                        # Python 3 can't have unbuffered text I/O.
                        self.attach('stderr', mode = 'a+', buffering = 1)

                # Handle signals.
                signal.signal(signal.SIGINT, self.handle_terminate)
                signal.signal(signal.SIGTERM, self.handle_terminate)
                signal.signal(signal.SIGHUP, self.handle_reload)
                #signal.signal(signal.SIGKILL....)

        def fork(self, msg):
                try:
                        pid = os.fork()
                        if pid > 0:
                                self.view(self.logdaemon.debug, None, msg + " success with PID %d." %pid)
                                # Exit from parent.
                                sys.exit(0)
                except Exception as e:
                        msg += " failed: %s." %str(e)
                        self.view(self.logdaemon.error, self.emit_error, msg)

        def detach(self):
                # cd to root for a guarenteed working dir.
                try:
                        os.chdir(self.homedir)
                except Exception as e:
                        msg = "Unable to change working directory: %s." %str(e)
                        self.view(self.logdaemon.error, self.emit_error, msg)

                # clear the session id to clear the controlling tty.
                pid = os.setsid()
                if pid == -1:
                        sys.exit(1)

                # set the umask so we have access to all files created by the daemon.
                try:
                        os.umask(self.umask)
                except Exception as e:
                        msg = "Unable to change file creation mask: %s." %str(e)
                        self.view(self.logdaemon.error, self.emit_error, msg)

        def attach(self, name, mode, buffering = -1):
                with open(getattr(self, name), mode, buffering) as stream:
                        os.dup2(stream.fileno(), getattr(sys, name).fileno())

        def checkfile(self, path, typearg, typefile):
                filename = os.path.basename(path)
                pathname = os.path.dirname(path)
                if not os.path.isdir(pathname):
                        msg = "argument %s: invalid directory: '%s'. Exiting..." %(typearg, pathname)
                        self.view(self.logdaemon.error, self.emit_error, msg)
                elif not filename.lower().endswith(typefile):
                        msg = "argument %s: not a %s file, invalid extension: '%s'. Exiting..." %(typearg, typefile, filename)