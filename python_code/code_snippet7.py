
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