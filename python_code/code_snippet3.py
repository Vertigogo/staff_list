
import dis
import struct
import array
import types
import functools


try:
    _array_to_bytes = array.array.tobytes
except AttributeError:
    _array_to_bytes = array.array.tostring


class _Bytecode:
    def __init__(self):
        code = (lambda: x if x else y).__code__.co_code
        opcode, oparg = struct.unpack_from('BB', code, 2)

        # Starting with Python 3.6, the bytecode format has changed, using
        # 16-bit words (8-bit opcode + 8-bit argument) for each instruction,
        # as opposed to previously 24 bit (8-bit opcode + 16-bit argument)
        # for instructions that expect an argument and otherwise 8 bit.
        # https://bugs.python.org/issue26647
        if dis.opname[opcode] == 'POP_JUMP_IF_FALSE':
            self.argument = struct.Struct('B')
            self.have_argument = 0
            # As of Python 3.6, jump targets are still addressed by their
            # byte unit. This is matter to change, so that jump targets,
            # in the future might refer to code units (address in bytes / 2).
            # https://bugs.python.org/issue26647
            self.jump_unit = 8 // oparg
        else:
            self.argument = struct.Struct('<H')
            self.have_argument = dis.HAVE_ARGUMENT
            self.jump_unit = 1

    @property
    def argument_bits(self):
        return self.argument.size * 8


_BYTECODE = _Bytecode()


def _make_code(code, codestring):
    args = [
        code.co_argcount,  code.co_nlocals,     code.co_stacksize,
        code.co_flags,     codestring,          code.co_consts,
        code.co_names,     code.co_varnames,    code.co_filename,
        code.co_name,      code.co_firstlineno, code.co_lnotab,
        code.co_freevars,  code.co_cellvars
    ]

    try:
        args.insert(1, code.co_kwonlyargcount)  # PY3
    except AttributeError:
        pass

    return types.CodeType(*args)


def _parse_instructions(code):
    extended_arg = 0
    extended_arg_offset = None
    pos = 0

    while pos < len(code):
        offset = pos
        if extended_arg_offset is not None:
            offset = extended_arg_offset

        opcode = struct.unpack_from('B', code, pos)[0]
        pos += 1

        oparg = None