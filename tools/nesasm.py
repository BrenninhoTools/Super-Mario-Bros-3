import re
from pathlib import Path

OPCODES = {
    "adc": {"imm": 0x69, "zp": 0x65, "zpx": 0x75, "abs": 0x6D, "absx": 0x7D, "absy": 0x79, "indx": 0x61, "indy": 0x71},
    "and": {"imm": 0x29, "zp": 0x25, "zpx": 0x35, "abs": 0x2D, "absx": 0x3D, "absy": 0x39, "indx": 0x21, "indy": 0x31},
    "asl": {"acc": 0x0A, "zp": 0x06, "zpx": 0x16, "abs": 0x0E, "absx": 0x1E},
    "bcc": {"rel": 0x90},
    "bcs": {"rel": 0xB0},
    "beq": {"rel": 0xF0},
    "bit": {"zp": 0x24, "abs": 0x2C},
    "bmi": {"rel": 0x30},
    "bne": {"rel": 0xD0},
    "bpl": {"rel": 0x10},
    "brk": {"imp": 0x00},
    "bvc": {"rel": 0x50},
    "bvs": {"rel": 0x70},
    "clc": {"imp": 0x18},
    "cld": {"imp": 0xD8},
    "cli": {"imp": 0x58},
    "clv": {"imp": 0xB8},
    "cmp": {"imm": 0xC9, "zp": 0xC5, "zpx": 0xD5, "abs": 0xCD, "absx": 0xDD, "absy": 0xD9, "indx": 0xC1, "indy": 0xD1},
    "cpx": {"imm": 0xE0, "zp": 0xE4, "abs": 0xEC},
    "cpy": {"imm": 0xC0, "zp": 0xC4, "abs": 0xCC},
    "dec": {"zp": 0xC6, "zpx": 0xD6, "abs": 0xCE, "absx": 0xDE},
    "dex": {"imp": 0xCA},
    "dey": {"imp": 0x88},
    "eor": {"imm": 0x49, "zp": 0x45, "zpx": 0x55, "abs": 0x4D, "absx": 0x5D, "absy": 0x59, "indx": 0x41, "indy": 0x51},
    "inc": {"zp": 0xE6, "zpx": 0xF6, "abs": 0xEE, "absx": 0xFE},
    "inx": {"imp": 0xE8},
    "iny": {"imp": 0xC8},
    "jmp": {"abs": 0x4C, "ind": 0x6C},
    "jsr": {"abs": 0x20},
    "lda": {"imm": 0xA9, "zp": 0xA5, "zpx": 0xB5, "abs": 0xAD, "absx": 0xBD, "absy": 0xB9, "indx": 0xA1, "indy": 0xB1},
    "ldx": {"imm": 0xA2, "zp": 0xA6, "zpy": 0xB6, "abs": 0xAE, "absy": 0xBE},
    "ldy": {"imm": 0xA0, "zp": 0xA4, "zpx": 0xB4, "abs": 0xAC, "absx": 0xBC},
    "lsr": {"acc": 0x4A, "zp": 0x46, "zpx": 0x56, "abs": 0x4E, "absx": 0x5E},
    "nop": {"imp": 0xEA},
    "ora": {"imm": 0x09, "zp": 0x05, "zpx": 0x15, "abs": 0x0D, "absx": 0x1D, "absy": 0x19, "indx": 0x01, "indy": 0x11},
    "pha": {"imp": 0x48},
    "php": {"imp": 0x08},
    "pla": {"imp": 0x68},
    "plp": {"imp": 0x28},
    "rol": {"acc": 0x2A, "zp": 0x26, "zpx": 0x36, "abs": 0x2E, "absx": 0x3E},
    "ror": {"acc": 0x6A, "zp": 0x66, "zpx": 0x76, "abs": 0x6E, "absx": 0x7E},
    "rti": {"imp": 0x40},
    "rts": {"imp": 0x60},
    "sbc": {"imm": 0xE9, "zp": 0xE5, "zpx": 0xF5, "abs": 0xED, "absx": 0xFD, "absy": 0xF9, "indx": 0xE1, "indy": 0xF1},
    "sec": {"imp": 0x38},
    "sed": {"imp": 0xF8},
    "sei": {"imp": 0x78},
    "sta": {"zp": 0x85, "zpx": 0x95, "abs": 0x8D, "absx": 0x9D, "absy": 0x99, "indx": 0x81, "indy": 0x91},
    "stx": {"zp": 0x86, "zpy": 0x96, "abs": 0x8E},
    "sty": {"zp": 0x84, "zpx": 0x94, "abs": 0x8C},
    "tax": {"imp": 0xAA},
    "tay": {"imp": 0xA8},
    "tsx": {"imp": 0xBA},
    "txa": {"imp": 0x8A},
    "txs": {"imp": 0x9A},
    "tya": {"imp": 0x98},
}

SIZES = {"imp": 1, "acc": 1, "imm": 2, "zp": 2, "zpx": 2, "zpy": 2, "rel": 2, "indx": 2, "indy": 2, "abs": 3, "absx": 3, "absy": 3, "ind": 3}

BANK_SIZE = 0x2000

DIRECTIVES = ("org", "byte", "word", "db", "dw", "ds", "bank", "include", "data", "code", "incchr")

IDENT = r"[A-Za-z_.][A-Za-z0-9_.]*"
LABEL_RE = re.compile(r"^(" + IDENT + r")(?:(:)|(?=\s|$))")
INDENTED_LABEL_RE = re.compile(r"^\s+(" + IDENT + r"):")
TOKEN_RE = re.compile(r"\s*(\$[0-9A-Fa-f]+|%[01]+|\d+|[A-Za-z_.][A-Za-z0-9_.]*|<<|>>|<=|>=|==|!=|&&|\|\||[-+*/&|^~!()<>=,\\]|'.')")


class AsmError(Exception):
    pass


def split_operands(text):
    parts = []
    depth = 0
    quote = False
    current = []
    for ch in text:
        if ch == '"':
            quote = not quote
        if not quote:
            if ch in "([":
                depth += 1
            elif ch in ")]":
                depth -= 1
            elif ch == "," and depth == 0:
                parts.append("".join(current).strip())
                current = []
                continue
        current.append(ch)
    tail = "".join(current).strip()
    if tail or parts:
        parts.append(tail)
    return parts


def strip_comment(line):
    out = []
    quote = False
    for ch in line:
        if ch == '"':
            quote = not quote
        if ch == ";" and not quote:
            break
        out.append(ch)
    return "".join(out).rstrip()


class Statement:
    __slots__ = ("kind", "source", "line", "name", "bank", "addr", "mnem", "operand", "mode", "expr", "value", "values", "size", "scope", "labels", "words", "data")

    def __init__(self, kind, source, line, **kw):
        self.kind = kind
        self.source = source
        self.line = line
        self.name = None
        self.bank = None
        self.addr = None
        self.mode = None
        self.expr = None
        self.value = None
        self.values = None
        self.size = 0
        self.labels = []
        self.words = None
        self.data = None
        for k, v in kw.items():
            setattr(self, k, v)


class Expr:
    def __init__(self, asm, text, scope, args=None, pc=None):
        self.asm = asm
        self.scope = scope
        self.args = args
        self.pc = pc
        self.tokens = [t for t in TOKEN_RE.findall(text)]
        if "".join(self.tokens).replace(" ", "") != text.replace(" ", "").replace("\t", ""):
            raise AsmError(f"bad expression '{text}'")
        self.pos = 0
        self.unresolved = False

    def peek(self):
        return self.tokens[self.pos] if self.pos < len(self.tokens) else None

    def take(self, expected=None):
        tok = self.peek()
        if expected is not None and tok != expected:
            raise AsmError(f"expected '{expected}' got '{tok}'")
        self.pos += 1
        return tok

    def parse(self):
        value = self.logical_or()
        if self.peek() is not None:
            raise AsmError(f"trailing tokens in expression: {self.tokens[self.pos:]}")
        return value

    def binary(self, ops, sub):
        value = sub()
        while self.peek() in ops:
            op = self.take()
            rhs = sub()
            value = ops[op](value, rhs)
        return value

    def logical_or(self):
        return self.binary({"||": lambda a, b: int(bool(a) or bool(b))}, self.logical_and)

    def logical_and(self):
        return self.binary({"&&": lambda a, b: int(bool(a) and bool(b))}, self.bit_or)

    def bit_or(self):
        return self.binary({"|": lambda a, b: a | b}, self.bit_xor)

    def bit_xor(self):
        return self.binary({"^": lambda a, b: a ^ b}, self.bit_and)

    def bit_and(self):
        return self.binary({"&": lambda a, b: a & b}, self.equality)

    def equality(self):
        return self.binary({"==": lambda a, b: int(a == b), "!=": lambda a, b: int(a != b), "=": lambda a, b: int(a == b)}, self.relational)

    def relational(self):
        return self.binary({"<": lambda a, b: int(a < b), ">": lambda a, b: int(a > b), "<=": lambda a, b: int(a <= b), ">=": lambda a, b: int(a >= b)}, self.shift)

    def shift(self):
        return self.binary({"<<": lambda a, b: a << b, ">>": lambda a, b: a >> b}, self.additive)

    def additive(self):
        return self.binary({"+": lambda a, b: a + b, "-": lambda a, b: a - b}, self.multiplicative)

    def multiplicative(self):
        return self.binary({"*": lambda a, b: a * b, "/": lambda a, b: int(a / b) if b else 0, "%": lambda a, b: a % b if b else 0}, self.unary)

    def unary(self):
        tok = self.peek()
        if tok == "-":
            self.take()
            return -self.unary()
        if tok == "+":
            self.take()
            return self.unary()
        if tok == "~":
            self.take()
            return ~self.unary()
        if tok == "!":
            self.take()
            return int(not self.unary())
        return self.primary()

    def primary(self):
        tok = self.take()
        if tok is None:
            raise AsmError("unexpected end of expression")
        if tok == "(":
            value = self.logical_or()
            self.take(")")
            return value
        if tok == "\\":
            index = int(self.take())
            if not self.args or index > len(self.args):
                raise AsmError(f"missing argument \\{index}")
            sub = Expr(self.asm, self.args[index - 1], self.scope, None, self.pc)
            value = sub.parse()
            self.unresolved |= sub.unresolved
            return value
        if tok[0] == "$":
            return int(tok[1:], 16)
        if tok[0] == "%":
            return int(tok[1:], 2)
        if tok[0].isdigit():
            return int(tok, 10)
        if tok[0] == "'":
            return ord(tok[1])
        if tok == "*" and self.pc is not None:
            return self.pc
        upper = tok.upper()
        if upper in ("LOW", "HIGH", "BANK") and self.peek() == "(":
            self.take("(")
            if upper == "BANK":
                name = self.take()
                self.take(")")
                return self.asm.bank_of(name, self.scope, self)
            value = self.logical_or()
            self.take(")")
            return value & 0xFF if upper == "LOW" else (value >> 8) & 0xFF
        if tok in self.asm.funcs and self.peek() == "(":
            self.take("(")
            args = []
            start = self.pos
            depth = 0
            while True:
                t = self.peek()
                if t is None:
                    raise AsmError("unterminated function call")
                if t == "(":
                    depth += 1
                elif t == ")":
                    if depth == 0:
                        args.append(" ".join(self.tokens[start:self.pos]))
                        self.take()
                        break
                    depth -= 1
                elif t == "," and depth == 0:
                    args.append(" ".join(self.tokens[start:self.pos]))
                    self.take()
                    start = self.pos
                    continue
                self.take()
            sub = Expr(self.asm, self.asm.funcs[tok], self.scope, args, self.pc)
            value = sub.parse()
            self.unresolved |= sub.unresolved
            return value
        return self.asm.symbol(tok, self.scope, self)


class Assembler:
    def __init__(self, root):
        self.root = Path(root)
        self.consts = {}
        self.funcs = {}
        self.macros = {}
        self.labels = {}
        self.label_banks = {}
        self.statements = []
        self.final = False
        self.load(self.root.name, self.root.parent)

    def symbol(self, name, scope, expr):
        key = self.qualify(name, scope)
        if key in self.labels:
            return self.labels[key]
        if name in self.consts:
            sub = Expr(self, self.consts[name][0], self.consts[name][1], None, None)
            value = sub.parse()
            expr.unresolved |= sub.unresolved
            return value
        if self.final:
            raise AsmError(f"undefined symbol '{name}'")
        expr.unresolved = True
        return 0x8000

    def bank_of(self, name, scope, expr):
        key = self.qualify(name, scope)
        if key in self.label_banks:
            return self.label_banks[key]
        if self.final:
            raise AsmError(f"undefined bank for '{name}'")
        expr.unresolved = True
        return 0

    @staticmethod
    def qualify(name, scope):
        if name.startswith("."):
            return f"{scope}{name}"
        return name

    def evaluate(self, text, scope, pc=None):
        expr = Expr(self, text, scope, None, pc)
        value = expr.parse()
        return value, expr.unresolved

    def load(self, name, base):
        path = base / name
        if not path.exists() and not path.suffix:
            path = base / (name + ".asm")
        lines = path.read_text(encoding="latin-1").splitlines()
        source = str(path.relative_to(self.root.parent))
        macro = None
        for number, raw in enumerate(lines, 1):
            line = strip_comment(raw)
            if macro is not None:
                if re.match(r"^\s*\.endm\b", line, re.I):
                    self.macros[macro[0]] = macro[1]
                    macro = None
                else:
                    macro[1].append(line)
                continue
            if not line.strip():
                continue
            m = re.match(r"^(" + IDENT + r"):?\s+\.macro\b", line, re.I)
            if m:
                macro = (m.group(1).lower(), [])
                continue
            m = re.match(r"^(" + IDENT + r"):?\s+\.func\s+(.+)$", line, re.I)
            if m:
                self.funcs[m.group(1)] = m.group(2).strip()
                continue
            self.parse_line(line, source, number, base, None)
        if macro is not None:
            raise AsmError(f"{source}: unterminated macro")

    def parse_line(self, line, source, number, base, args):
        m = re.match(r"^\s*(" + IDENT + r")\s*=\s*(.+)$", line)
        if m:
            if args is not None:
                raise AsmError(f"{source}:{number}: constant inside macro")
            self.consts[m.group(1)] = (m.group(2).strip(), None)
            return
        label = None
        body = line
        m = LABEL_RE.match(line)
        if m and (m.group(2) or not self.is_keyword(m.group(1))):
            label = m.group(1)
            body = line[m.end():]
        else:
            m = INDENTED_LABEL_RE.match(line)
            if m:
                label = m.group(1)
                body = line[m.end():]
        if label:
            self.statements.append(Statement("label", source, number, name=label))
        body = body.strip()
        if not body:
            return
        if args is not None:
            body = re.sub(r"\\(\d)", lambda mm: args[int(mm.group(1)) - 1] if int(mm.group(1)) <= len(args) else "", body)
        parts = body.split(None, 1)
        head = parts[0].lower()
        rest = parts[1].strip() if len(parts) > 1 else ""
        if head in DIRECTIVES:
            head = "." + head
        if head == ".include":
            self.load(rest.strip('"'), base)
            return
        if head in (".byte", ".db"):
            self.statements.append(Statement("byte", source, number, values=split_operands(rest)))
            return
        if head in (".word", ".dw"):
            self.statements.append(Statement("word", source, number, values=split_operands(rest)))
            return
        if head == ".ds":
            self.statements.append(Statement("ds", source, number, expr=rest))
            return
        if head == ".org":
            self.statements.append(Statement("org", source, number, expr=rest))
            return
        if head == ".bank":
            self.statements.append(Statement("bank", source, number, expr=rest))
            return
        if head in (".data", ".code"):
            self.statements.append(Statement(head[1:], source, number))
            return
        if head in (".incchr", ".inesprg", ".ineschr", ".inesmap", ".inesmir"):
            return
        if head in ("if", "endif", "fail", "else"):
            return
        if head in self.macros:
            margs = []
            for part in (split_operands(rest) if rest else []):
                if margs and part.strip().lower() in ("x", "y"):
                    margs[-1] = margs[-1] + "," + part.strip()
                else:
                    margs.append(part)
            for mline in self.macros[head]:
                if not mline.strip():
                    continue
                self.parse_line(mline, source, number, base, margs)
            return
        if head in OPCODES:
            self.statements.append(Statement("ins", source, number, mnem=head, operand=rest))
            return
        raise AsmError(f"{source}:{number}: unknown statement '{body}'")

    def is_keyword(self, word):
        w = word.lower()
        return w in OPCODES or w in self.macros or w.startswith(".") and w in (".byte", ".word", ".ds", ".org", ".bank", ".data", ".code", ".include", ".db", ".dw", ".incchr") or w in ("if", "endif", "fail", "else")

    def mode_of(self, mnem, operand, scope, pc):
        table = OPCODES[mnem]
        op = operand.strip()
        if not op or op.lower() == "a":
            return ("acc" if "acc" in table else "imp"), None
        if "rel" in table:
            return "rel", op
        if op.startswith("#"):
            return "imm", op[1:].strip()
        m = re.fullmatch(r"\[(.+)\]\s*,\s*[yY]", op)
        if m:
            return "indy", m.group(1).lstrip("<").strip()
        m = re.fullmatch(r"\[(.+),\s*[xX]\s*\]", op)
        if m:
            return "indx", m.group(1).lstrip("<").strip()
        m = re.fullmatch(r"\[(.+)\]", op)
        if m:
            if "ind" not in table:
                raise AsmError(f"{mnem} has no indirect mode")
            return "ind", m.group(1).strip()
        index = ""
        m = re.fullmatch(r"(.+?)\s*,\s*([xXyY])", op)
        if m:
            op, index = m.group(1).strip(), m.group(2).lower()
        zp = op.startswith("<")
        if zp:
            op = op[1:].strip()
        mode = ("zp" if zp else "abs") + index
        if mode not in table:
            if zp and ("abs" + index) in table:
                mode = "abs" + index
            else:
                raise AsmError(f"{mnem} has no mode {mode}")
        return mode, op

    def layout(self):
        pc = 0
        bank = 0
        section = "code"
        scope = ""
        self.items = []
        for st in self.statements:
            k = st.kind
            if k == "bank":
                bank = self.evaluate(st.expr, scope)[0]
                section = "code"
                continue
            if k == "data":
                section = "data"
                continue
            if k == "code":
                section = "code"
                continue
            if k == "org":
                pc = self.evaluate(st.expr, scope)[0]
                continue
            if k == "label":
                if not st.name.startswith("."):
                    scope = st.name
                key = self.qualify(st.name, scope)
                if not self.final:
                    if key in self.labels and self.labels[key] != pc:
                        raise AsmError(f"{st.source}:{st.line}: duplicate label {key}")
                    self.labels[key] = pc
                    self.label_banks[key] = bank if section == "code" else 0
                elif self.labels.get(key) != pc:
                    raise AsmError(f"{st.source}:{st.line}: label {key} moved between passes")
                st.scope = scope
                st.addr = pc
                st.bank = bank if section == "code" else None
                self.items.append(st)
                continue
            st.scope = scope
            st.addr = pc
            st.bank = bank if section == "code" else None
            if k == "ds":
                st.size = self.evaluate(st.expr, scope, pc)[0]
            elif k == "byte":
                size = 0
                for v in st.values:
                    if v.startswith('"'):
                        size += len(v.strip('"'))
                    else:
                        size += 1
                st.size = size
            elif k == "word":
                st.size = 2 * len(st.values)
            elif k == "ins":
                try:
                    st.mode, st.expr = self.mode_of(st.mnem, st.operand, scope, pc)
                except AsmError as e:
                    raise AsmError(f"{st.source}:{st.line}: {e}")
                st.size = SIZES[st.mode]
            if section == "data" and k != "ds":
                raise AsmError(f"{st.source}:{st.line}: output in data section")
            pc += st.size
            self.items.append(st)

    def assemble(self, banks):
        self.final = False
        self.layout()
        self.layout()
        self.final = True
        self.layout()
        rom = bytearray(banks * BANK_SIZE)
        used = bytearray(banks * BANK_SIZE)
        for st in self.items:
            if st.kind == "label" or st.bank is None or st.size == 0:
                continue
            data = self.encode(st)
            st.data = data
            offset = st.bank * BANK_SIZE + (st.addr & (BANK_SIZE - 1))
            if (st.addr & (BANK_SIZE - 1)) + len(data) > BANK_SIZE:
                raise AsmError(f"{st.source}:{st.line}: bank {st.bank} overflow")
            for i, b in enumerate(data):
                if used[offset + i]:
                    raise AsmError(f"{st.source}:{st.line}: overlapping output at bank {st.bank} ${st.addr + i:04X}")
                used[offset + i] = 1
                rom[offset + i] = b
        for i in range(len(rom)):
            if not used[i]:
                rom[i] = 0xFF
        self.used = bytes(used)
        self.rom = bytes(rom)
        return self.rom

    def encode(self, st):
        scope = st.scope
        if st.kind == "ds":
            return bytes(st.size)
        if st.kind == "byte":
            out = bytearray()
            for v in st.values:
                if v.startswith('"'):
                    out += v.strip('"').encode("latin-1")
                else:
                    out.append(self.evaluate(v, scope, st.addr)[0] & 0xFF)
            return bytes(out)
        if st.kind == "word":
            out = bytearray()
            st.words = []
            for v in st.values:
                w = self.evaluate(v, scope, st.addr)[0] & 0xFFFF
                st.words.append(w)
                out += bytes((w & 0xFF, w >> 8))
            return bytes(out)
        op = OPCODES[st.mnem][st.mode]
        if st.mode in ("imp", "acc"):
            st.value = None
            return bytes((op,))
        if st.mode == "rel":
            target = self.evaluate(st.expr, scope, st.addr)[0]
            delta = target - (st.addr + 2)
            if not -128 <= delta <= 127:
                raise AsmError(f"{st.source}:{st.line}: branch out of range")
            st.value = target
            return bytes((op, delta & 0xFF))
        value = self.evaluate(st.expr, scope, st.addr)[0]
        if SIZES[st.mode] == 2:
            st.value = value & 0xFF
            return bytes((op, st.value))
        st.value = value & 0xFFFF
        return bytes((op, st.value & 0xFF, st.value >> 8))
