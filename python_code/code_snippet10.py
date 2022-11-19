
#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from __future__ import with_statement

import os
import re
import sys
import getopt
from os.path import join, splitext, abspath, exists
from collections import defaultdict

all_directives = '(' + '|'.join(directives) + ')'
seems_directive_re = re.compile(r'(?<!\.)\.\. %s([^a-z:]|:(?!:))' % all_directives)
default_role_re = re.compile(r'(^| )`\w([^`]*?\w)?`($| )')
leaked_markup_re = re.compile(r'[a-z]::\s|`|\.\.\s*\w+:')
checkers = {}
checker_props = {'severity': 1, 'falsepositives': False}


def checker(*suffixes, **kwds):
    """Decorator to register a function as a checker."""

    def deco(func):