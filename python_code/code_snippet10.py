
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
        for suffix in suffixes:
            checkers.setdefault(suffix, []).append(func)
        for prop in checker_props:
            setattr(func, prop, kwds.get(prop, checker_props[prop]))
        return func

    return deco


@checker('.py', severity=4)
def check_syntax(fn, lines):
    """Check Python examples for valid syntax."""
    code = ''.join(lines)
    if '\r' in code:
        if os.name != 'nt':
            yield 0, '\\r in code file'
        code = code.replace('\r', '')
    try:
        compile(code, fn, 'exec')
    except SyntaxError as err:
        yield err.lineno, 'not compilable: %s' % err


@checker('.rst', severity=2)
def check_suspicious_constructs(fn, lines):
    """Check for suspicious reST constructs."""
    inprod = False
    for lno, line in enumerate(lines):
        if seems_directive_re.search(line):
            yield lno + 1, 'comment seems to be intended as a directive'
        if '.. productionlist::' in line:
            inprod = True
        elif not inprod and default_role_re.search(line):
            yield lno + 1, 'default role used'
        elif inprod and not line.strip():
            inprod = False


@checker('.py', '.rst')
def check_whitespace(fn, lines):
    """Check for whitespace and line length issues."""
    for lno, line in enumerate(lines):
        if '\r' in line:
            yield lno + 1, '\\r in line'
        if '\t' in line:
            yield lno + 1, 'OMG TABS!!!1'
        if line[:-1].rstrip(' \t') != line[:-1]:
            yield lno + 1, 'trailing whitespace'


@checker('.rst', severity=0)
def check_line_length(fn, lines):
    """Check for line length; this checker is not run by default."""
    for lno, line in enumerate(lines):
        if len(line) > 81:
            # don't complain about tables, links and function signatures
            if (
                line.lstrip()[0] not in '+|'
                and 'http://' not in line
                and not line.lstrip().startswith(
                    ('.. function', '.. method', '.. cfunction')
                )
            ):
                yield lno + 1, "line too long"


@checker('.html', severity=2, falsepositives=True)
def check_leaked_markup(fn, lines):
    """Check HTML files for leaked reST markup; this only works if
    the HTML files have been built.
    """
    for lno, line in enumerate(lines):
        if leaked_markup_re.search(line):
            yield lno + 1, 'possibly leaked markup: %r' % line


def main(argv):
    usage = '''\