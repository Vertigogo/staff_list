
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