import sys
import zlib
from pathlib import Path

from nesasm import Assembler, AsmError, BANK_SIZE, SIZES

BANKS = 32

BASE_CYCLES = {
    "imp": 2, "acc": 2, "imm": 2, "zp": 3, "zpx": 4, "zpy": 4, "abs": 4,
    "absx": 4, "absy": 4, "indx": 6, "indy": 5, "rel": 2, "ind": 5,
}
WRITE_CYCLES = {"zp": 3, "zpx": 4, "zpy": 4, "abs": 4, "absx": 5, "absy": 5, "indx": 6, "indy": 6}
RMW_CYCLES = {"zp": 5, "zpx": 6, "abs": 6, "absx": 7}
SPECIAL_CYCLES = {"jsr": 6, "rts": 6, "rti": 6, "pha": 3, "php": 3, "pla": 4, "plp": 4, "brk": 7}
BRANCHES = {"bcc": "!C", "bcs": "C", "beq": "Z", "bne": "!Z", "bmi": "N", "bpl": "!N", "bvc": "!V", "bvs": "V"}
WAIT_SAFE = {"lda", "ldx", "ldy", "cmp", "cpx", "cpy", "bit", "and", "ora", "eor", "nop", "clc", "sec", "clv",
             "tax", "tay", "txa", "tya"} | set(BRANCHES)
NO_FALLTHROUGH = {"jmp", "rts", "rti", "brk"}


def h(v, width=4):
    return f"0x{v:0{width}X}"


def cycles_of(st):
    m = st.mnem
    if m in SPECIAL_CYCLES:
        return SPECIAL_CYCLES[m]
    if m == "jmp":
        return 3 if st.mode == "abs" else 5
    if m in ("sta", "stx", "sty"):
        return WRITE_CYCLES[st.mode]
    if m in ("asl", "lsr", "rol", "ror", "inc", "dec") and st.mode != "acc":
        return RMW_CYCLES[st.mode]
    return BASE_CYCLES[st.mode]


class Bank:
    def __init__(self, number):
        self.number = number
        self.code = {}
        self.labels = set()
        self.base = None

    def add(self, st):
        base = st.addr & 0xE000
        if self.base is None:
            self.base = base
        elif self.base != base:
            raise AsmError(f"bank {self.number} has code in two windows (${self.base:04X} and ${base:04X})")
        self.code[st.addr] = st


class Translator:
    def __init__(self, asm):
        self.asm = asm
        self.banks = [Bank(i) for i in range(BANKS)]
        for st in asm.items:
            if st.bank is None:
                continue
            if st.kind == "ins":
                self.banks[st.bank].add(st)
        for st in asm.items:
            if st.kind == "label" and st.bank is not None and st.addr in self.banks[st.bank].code:
                self.banks[st.bank].labels.add(st.addr)
        self.entries = [set() for _ in range(BANKS)]
        words = set()
        for st in asm.items:
            if st.kind == "word" and st.bank is not None and st.words:
                words.update(st.words)
        for bank in self.banks:
            e = self.entries[bank.number]
            e |= bank.labels
            for addr, st in bank.code.items():
                if st.mnem == "jsr":
                    e.add(addr + 3)
            for w in words:
                if w in bank.code:
                    e.add(w)
        self.wait_loops = set()
        self.find_wait_loops()
        for bank in self.banks:
            for addr, st in bank.code.items():
                if st.mnem in BRANCHES and st.value <= addr:
                    self.entries[bank.number].add(st.value)
                if st.mnem == "jmp" and st.mode == "abs" and st.value <= addr and st.value in bank.code:
                    self.entries[bank.number].add(st.value)
            self.entries[bank.number] = {a for a in self.entries[bank.number] if a in bank.code}

    def find_wait_loops(self):
        for bank in self.banks:
            for addr, st in bank.code.items():
                if st.mnem == "jmp" and st.mode == "abs" and st.value == addr:
                    self.wait_loops.add((bank.number, addr))
                    continue
                if st.mnem not in BRANCHES or st.value > addr:
                    continue
                pc = st.value
                ok = True
                while pc < addr:
                    body = bank.code.get(pc)
                    if body is None or body.mnem not in WAIT_SAFE:
                        ok = False
                        break
                    if body.mnem in BRANCHES and not (st.value <= body.value <= addr + 2):
                        ok = False
                        break
                    pc += body.size
                if ok and pc == addr:
                    self.wait_loops.add((bank.number, addr))

    def read_expr(self, st):
        m, v = st.mode, st.value
        if m == "imm":
            return h(v, 2)
        if m == "zp":
            return f"ram[{h(v, 2)}]"
        if m == "zpx":
            return f"ram[(uint8_t)({h(v, 2)} + X)]"
        if m == "zpy":
            return f"ram[(uint8_t)({h(v, 2)} + Y)]"
        if m == "abs":
            if v < 0x2000:
                return f"ram[{h(v & 0x7FF)}]"
            if 0x6000 <= v < 0x8000:
                return f"wram[{h(v - 0x6000)}]"
            if v >= 0x8000:
                return f"PRG({h(v)})"
            return f"bus.readIo({h(v)})"
        if m in ("absx", "absy"):
            r = "X" if m == "absx" else "Y"
            if v + 0xFF < 0x800:
                return f"ram[{h(v)} + {r}]"
            if v >= 0x6000 and v + 0xFF < 0x8000:
                return f"wram[{h(v - 0x6000)} + {r}]"
            if v >= 0x8000 and v + 0xFF <= 0xFFFF:
                return f"PRG({h(v)} + {r})"
            return f"bus.read((uint16_t)({h(v)} + {r}))"
        if m == "indy":
            return f"bus.read((uint16_t)((ram[{h(v, 2)}] | (ram[(uint8_t)({h(v, 2)} + 1)] << 8)) + Y))"
        if m == "indx":
            return f"bus.read((uint16_t)(ram[(uint8_t)({h(v, 2)} + X)] | (ram[(uint8_t)({h(v, 2)} + X + 1)] << 8)))"
        raise AsmError(f"no read for mode {m}")

    def write_stmt(self, st, value):
        m, v = st.mode, st.value
        if m == "zp":
            return f"ram[{h(v, 2)}] = {value};"
        if m == "zpx":
            return f"ram[(uint8_t)({h(v, 2)} + X)] = {value};"
        if m == "zpy":
            return f"ram[(uint8_t)({h(v, 2)} + Y)] = {value};"
        if m == "abs":
            if v < 0x2000:
                return f"ram[{h(v & 0x7FF)}] = {value};"
            if 0x6000 <= v < 0x8000:
                return f"wram[{h(v - 0x6000)}] = {value};"
            return f"WRITE_IO({h(v)}, {value});"
        if m in ("absx", "absy"):
            r = "X" if m == "absx" else "Y"
            if v + 0xFF < 0x800:
                return f"ram[{h(v)} + {r}] = {value};"
            if v >= 0x6000 and v + 0xFF < 0x8000:
                return f"wram[{h(v - 0x6000)} + {r}] = {value};"
            return f"WRITE_IO((uint16_t)({h(v)} + {r}), {value});"
        if m == "indy":
            return f"WRITE_IO((uint16_t)((ram[{h(v, 2)}] | (ram[(uint8_t)({h(v, 2)} + 1)] << 8)) + Y), {value});"
        if m == "indx":
            return f"WRITE_IO((uint16_t)(ram[(uint8_t)({h(v, 2)} + X)] | (ram[(uint8_t)({h(v, 2)} + X + 1)] << 8)), {value});"
        raise AsmError(f"no write for mode {m}")

    def jump(self, bank, target, backward):
        if target in bank.code:
            if backward:
                return f"if (cyc >= limit) EXIT_YIELD({h(target)}); goto L{target:04X};"
            return f"goto L{target:04X};"
        return f"EXIT({h(target)});"

    def emit(self, bank, st):
        mn = st.mnem
        cyc = cycles_of(st)
        pre = f"cyc += {cyc}; "
        if mn in ("lda", "ldx", "ldy"):
            reg = mn[2].upper()
            return pre + f"{reg} = {self.read_expr(st)}; NZ({reg});"
        if mn in ("sta", "stx", "sty"):
            return pre + self.write_stmt(st, mn[2].upper())
        if mn in ("adc", "sbc"):
            inv = " ^ 0xFF" if mn == "sbc" else ""
            return pre + f"t = {self.read_expr(st)}{inv}; ADC(t);"
        if mn in ("and", "ora", "eor"):
            op = {"and": "&", "ora": "|", "eor": "^"}[mn]
            return pre + f"A {op}= {self.read_expr(st)}; NZ(A);"
        if mn in ("cmp", "cpx", "cpy"):
            reg = {"cmp": "A", "cpx": "X", "cpy": "Y"}[mn]
            return pre + f"t = {self.read_expr(st)}; CMP({reg}, t);"
        if mn == "bit":
            return pre + f"t = {self.read_expr(st)}; Z = !(A & t); N = (t & 0x80) != 0; V = (t & 0x40) != 0;"
        if mn in ("asl", "lsr", "rol", "ror"):
            body = {
                "asl": "C = (t & 0x80) != 0; t = (uint8_t)(t << 1);",
                "lsr": "C = (t & 0x01) != 0; t = (uint8_t)(t >> 1);",
                "rol": "u = (unsigned)(t << 1) | (C ? 1u : 0u); C = (t & 0x80) != 0; t = (uint8_t)u;",
                "ror": "u = (unsigned)(t >> 1) | (C ? 0x80u : 0u); C = (t & 0x01) != 0; t = (uint8_t)u;",
            }[mn]
            if st.mode == "acc":
                return pre + f"t = A; {body} A = t; NZ(A);"
            return pre + f"t = {self.read_expr(st)}; {body} {self.write_stmt(st, 't')} NZ(t);"
        if mn in ("inc", "dec"):
            d = "+" if mn == "inc" else "-"
            return pre + f"t = (uint8_t)({self.read_expr(st)} {d} 1); {self.write_stmt(st, 't')} NZ(t);"
        simple = {
            "inx": "X++; NZ(X);", "iny": "Y++; NZ(Y);", "dex": "X--; NZ(X);", "dey": "Y--; NZ(Y);",
            "tax": "X = A; NZ(X);", "tay": "Y = A; NZ(Y);", "txa": "A = X; NZ(A);", "tya": "A = Y; NZ(A);",
            "tsx": "X = S; NZ(X);", "txs": "S = X;",
            "clc": "C = false;", "sec": "C = true;", "cli": "c.I = false;", "sei": "c.I = true;",
            "cld": "c.D = false;", "sed": "c.D = true;", "clv": "V = false;", "nop": "",
            "pha": "PUSH(A);", "pla": "A = PULL(); NZ(A);",
            "php": "PUSH(PACK());", "plp": "t = PULL(); UNPACK(t);",
        }
        if mn in simple:
            return pre + simple[mn]
        if mn == "rti":
            return pre + "t = PULL(); UNPACK(t); u = PULL(); u |= (unsigned)PULL() << 8; EXIT_RTI(u);"
        if mn == "rts":
            return pre + "u = PULL(); u |= (unsigned)PULL() << 8; EXIT((uint16_t)(u + 1));"
        if mn == "brk":
            return pre + f"EXIT_FAULT({h(st.addr)});"
        if mn in BRANCHES:
            cond = BRANCHES[mn]
            target = st.value
            if (bank.number, st.addr) in self.wait_loops:
                return pre + f"if ({cond}) {{ cyc++; EXIT_YIELD({h(target)}); }}"
            return pre + f"if ({cond}) {{ cyc++; {self.jump(bank, target, target <= st.addr)} }}"
        if mn == "jmp":
            if st.mode == "ind":
                v = st.value
                hi = (v & 0xFF00) | ((v + 1) & 0xFF)
                return pre + f"u = READ16({h(v)}, {h(hi)}); EXIT((uint16_t)u);"
            if (bank.number, st.addr) in self.wait_loops:
                return pre + f"EXIT_YIELD({h(st.addr)});"
            return pre + self.jump(bank, st.value, st.value <= st.addr)
        if mn == "jsr":
            ret = st.addr + 2
            return pre + f"PUSH({h(ret >> 8, 2)}); PUSH({h(ret & 0xFF, 2)}); {self.jump(bank, st.value, False)}"
        raise AsmError(f"cannot translate {mn}")

    def generate_bank(self, bank):
        out = []
        w = out.append
        w('#include "Generated.hpp"')
        w("")
        w("namespace smb3")
        w("{")
        if not bank.code:
            w(f"uint32_t bank{bank.number:02d}(Cpu&, uint16_t)")
            w("{")
            w("    return kExitMiss;")
            w("}")
            w("}")
            w("")
            return "\n".join(out)
        w(f"uint32_t bank{bank.number:02d}(Cpu& c, uint16_t entry)")
        w("{")
        w("    BANK_PROLOGUE();")
        w("    switch (entry)")
        w("    {")
        for e in sorted(self.entries[bank.number]):
            w(f"    case {h(e)}: goto L{e:04X};")
        w("    default: return kExitMiss;")
        w("    }")
        addrs = sorted(bank.code)
        for i, addr in enumerate(addrs):
            st = bank.code[addr]
            w(f"L{addr:04X}: {self.emit(bank, st)}")
            if st.mnem not in NO_FALLTHROUGH:
                nxt = addr + st.size
                if i + 1 >= len(addrs) or addrs[i + 1] != nxt:
                    w(f"    EXIT({h(nxt & 0xFFFF)});")
        w("}")
        w("}")
        w("")
        return "\n".join(out)

    def generate_table(self, crc, vectors):
        out = []
        w = out.append
        w('#include "Generated.hpp"')
        w("")
        w("namespace smb3")
        w("{")
        w(f"const uint32_t kPrgCrc = 0x{crc:08X}u;")
        w(f"const uint16_t kNmiVector = {h(vectors[0])};")
        w(f"const uint16_t kResetVector = {h(vectors[1])};")
        w(f"const uint16_t kIrqVector = {h(vectors[2])};")
        w("")
        w("const BankFunction kBankFunctions[kBankCount] = {")
        w("    " + ", ".join(f"bank{i:02d}" for i in range(BANKS)))
        w("};")
        w("")
        w("const uint16_t kBankBase[kBankCount] = {")
        w("    " + ", ".join(h(b.base if b.base is not None else 0xFFFF) for b in self.banks))
        w("};")
        w("")
        w("const uint8_t kEntryBits[kBankCount][kBankSize / 8] = {")
        for bank in self.banks:
            bits = bytearray(BANK_SIZE // 8)
            for e in self.entries[bank.number]:
                o = e & (BANK_SIZE - 1)
                bits[o >> 3] |= 1 << (o & 7)
            rows = []
            for r in range(0, len(bits), 32):
                rows.append(", ".join(str(b) for b in bits[r:r + 32]))
            w("    {")
            w("        " + ",\n        ".join(rows))
            w("    },")
        w("};")
        w("}")
        w("")
        return "\n".join(out)


def write_source(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")


def main():
    if len(sys.argv) < 3:
        print("usage: smb3cpp.py <smb3.asm> <out dir> [out.prg]")
        return 2
    asm = Assembler(sys.argv[1])
    prg = asm.assemble(BANKS)
    vectors = tuple(prg[-6 + 2 * i] | (prg[-5 + 2 * i] << 8) for i in range(3))
    crc = zlib.crc32(prg) & 0xFFFFFFFF
    tr = Translator(asm)
    out = Path(sys.argv[2])
    for bank in tr.banks:
        write_source(out / f"Bank{bank.number:02d}.cpp", tr.generate_bank(bank))
    write_source(out / "BankTable.cpp", tr.generate_table(crc, vectors))
    if len(sys.argv) > 3:
        Path(sys.argv[3]).write_bytes(prg)
    return 0


if __name__ == "__main__":
    sys.exit(main())
