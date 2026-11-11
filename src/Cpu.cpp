#include "Cpu.hpp"

#include <cstdio>
#include <cstring>
#include <limits>

#include "Bus.hpp"
#include "Generated.hpp"

namespace smb3
{
namespace
{
enum Mode : uint8_t
{
    Imp,
    Acc,
    Imm,
    Zp,
    Zpx,
    Zpy,
    Abs,
    Absx,
    Absy,
    Indx,
    Indy,
    Rel,
    Ind
};

enum Op : uint8_t
{
    Bad,
    Adc, And, Asl, Bcc, Bcs, Beq, Bit, Bmi, Bne, Bpl, Brk, Bvc, Bvs, Clc, Cld, Cli, Clv, Cmp, Cpx, Cpy,
    Dec, Dex, Dey, Eor, Inc, Inx, Iny, Jmp, Jsr, Lda, Ldx, Ldy, Lsr, Nop, Ora, Pha, Php, Pla, Plp, Rol,
    Ror, Rti, Rts, Sbc, Sec, Sed, Sei, Sta, Stx, Sty, Tax, Tay, Tsx, Txa, Txs, Tya
};

struct Opcode
{
    Op op;
    Mode mode;
    uint8_t cycles;
};

struct OpcodeTable
{
    Opcode entries[256];

    void set(int code, Op op, Mode mode, int cycles)
    {
        entries[code] = {op, mode, (uint8_t)cycles};
    }

    OpcodeTable()
    {
        for (auto& e : entries)
        {
            e = {Bad, Imp, 2};
        }
        set(0x69, Adc, Imm, 2); set(0x65, Adc, Zp, 3); set(0x75, Adc, Zpx, 4); set(0x6D, Adc, Abs, 4);
        set(0x7D, Adc, Absx, 4); set(0x79, Adc, Absy, 4); set(0x61, Adc, Indx, 6); set(0x71, Adc, Indy, 5);
        set(0x29, And, Imm, 2); set(0x25, And, Zp, 3); set(0x35, And, Zpx, 4); set(0x2D, And, Abs, 4);
        set(0x3D, And, Absx, 4); set(0x39, And, Absy, 4); set(0x21, And, Indx, 6); set(0x31, And, Indy, 5);
        set(0x0A, Asl, Acc, 2); set(0x06, Asl, Zp, 5); set(0x16, Asl, Zpx, 6); set(0x0E, Asl, Abs, 6); set(0x1E, Asl, Absx, 7);
        set(0x90, Bcc, Rel, 2); set(0xB0, Bcs, Rel, 2); set(0xF0, Beq, Rel, 2); set(0x30, Bmi, Rel, 2);
        set(0xD0, Bne, Rel, 2); set(0x10, Bpl, Rel, 2); set(0x50, Bvc, Rel, 2); set(0x70, Bvs, Rel, 2);
        set(0x24, Bit, Zp, 3); set(0x2C, Bit, Abs, 4);
        set(0x00, Brk, Imp, 7);
        set(0x18, Clc, Imp, 2); set(0xD8, Cld, Imp, 2); set(0x58, Cli, Imp, 2); set(0xB8, Clv, Imp, 2);
        set(0xC9, Cmp, Imm, 2); set(0xC5, Cmp, Zp, 3); set(0xD5, Cmp, Zpx, 4); set(0xCD, Cmp, Abs, 4);
        set(0xDD, Cmp, Absx, 4); set(0xD9, Cmp, Absy, 4); set(0xC1, Cmp, Indx, 6); set(0xD1, Cmp, Indy, 5);
        set(0xE0, Cpx, Imm, 2); set(0xE4, Cpx, Zp, 3); set(0xEC, Cpx, Abs, 4);
        set(0xC0, Cpy, Imm, 2); set(0xC4, Cpy, Zp, 3); set(0xCC, Cpy, Abs, 4);
        set(0xC6, Dec, Zp, 5); set(0xD6, Dec, Zpx, 6); set(0xCE, Dec, Abs, 6); set(0xDE, Dec, Absx, 7);
        set(0xCA, Dex, Imp, 2); set(0x88, Dey, Imp, 2);
        set(0x49, Eor, Imm, 2); set(0x45, Eor, Zp, 3); set(0x55, Eor, Zpx, 4); set(0x4D, Eor, Abs, 4);
        set(0x5D, Eor, Absx, 4); set(0x59, Eor, Absy, 4); set(0x41, Eor, Indx, 6); set(0x51, Eor, Indy, 5);
        set(0xE6, Inc, Zp, 5); set(0xF6, Inc, Zpx, 6); set(0xEE, Inc, Abs, 6); set(0xFE, Inc, Absx, 7);
        set(0xE8, Inx, Imp, 2); set(0xC8, Iny, Imp, 2);
        set(0x4C, Jmp, Abs, 3); set(0x6C, Jmp, Ind, 5); set(0x20, Jsr, Abs, 6);
        set(0xA9, Lda, Imm, 2); set(0xA5, Lda, Zp, 3); set(0xB5, Lda, Zpx, 4); set(0xAD, Lda, Abs, 4);
        set(0xBD, Lda, Absx, 4); set(0xB9, Lda, Absy, 4); set(0xA1, Lda, Indx, 6); set(0xB1, Lda, Indy, 5);
        set(0xA2, Ldx, Imm, 2); set(0xA6, Ldx, Zp, 3); set(0xB6, Ldx, Zpy, 4); set(0xAE, Ldx, Abs, 4); set(0xBE, Ldx, Absy, 4);
        set(0xA0, Ldy, Imm, 2); set(0xA4, Ldy, Zp, 3); set(0xB4, Ldy, Zpx, 4); set(0xAC, Ldy, Abs, 4); set(0xBC, Ldy, Absx, 4);
        set(0x4A, Lsr, Acc, 2); set(0x46, Lsr, Zp, 5); set(0x56, Lsr, Zpx, 6); set(0x4E, Lsr, Abs, 6); set(0x5E, Lsr, Absx, 7);
        set(0xEA, Nop, Imp, 2);
        set(0x09, Ora, Imm, 2); set(0x05, Ora, Zp, 3); set(0x15, Ora, Zpx, 4); set(0x0D, Ora, Abs, 4);
        set(0x1D, Ora, Absx, 4); set(0x19, Ora, Absy, 4); set(0x01, Ora, Indx, 6); set(0x11, Ora, Indy, 5);
        set(0x48, Pha, Imp, 3); set(0x08, Php, Imp, 3); set(0x68, Pla, Imp, 4); set(0x28, Plp, Imp, 4);
        set(0x2A, Rol, Acc, 2); set(0x26, Rol, Zp, 5); set(0x36, Rol, Zpx, 6); set(0x2E, Rol, Abs, 6); set(0x3E, Rol, Absx, 7);
        set(0x6A, Ror, Acc, 2); set(0x66, Ror, Zp, 5); set(0x76, Ror, Zpx, 6); set(0x6E, Ror, Abs, 6); set(0x7E, Ror, Absx, 7);
        set(0x40, Rti, Imp, 6); set(0x60, Rts, Imp, 6);
        set(0xE9, Sbc, Imm, 2); set(0xE5, Sbc, Zp, 3); set(0xF5, Sbc, Zpx, 4); set(0xED, Sbc, Abs, 4);
        set(0xFD, Sbc, Absx, 4); set(0xF9, Sbc, Absy, 4); set(0xE1, Sbc, Indx, 6); set(0xF1, Sbc, Indy, 5);
        set(0x38, Sec, Imp, 2); set(0xF8, Sed, Imp, 2); set(0x78, Sei, Imp, 2);
        set(0x85, Sta, Zp, 3); set(0x95, Sta, Zpx, 4); set(0x8D, Sta, Abs, 4); set(0x9D, Sta, Absx, 5);
        set(0x99, Sta, Absy, 5); set(0x81, Sta, Indx, 6); set(0x91, Sta, Indy, 6);
        set(0x86, Stx, Zp, 3); set(0x96, Stx, Zpy, 4); set(0x8E, Stx, Abs, 4);
        set(0x84, Sty, Zp, 3); set(0x94, Sty, Zpx, 4); set(0x8C, Sty, Abs, 4);
        set(0xAA, Tax, Imp, 2); set(0xA8, Tay, Imp, 2); set(0xBA, Tsx, Imp, 2);
        set(0x8A, Txa, Imp, 2); set(0x9A, Txs, Imp, 2); set(0x98, Tya, Imp, 2);
    }
};

const OpcodeTable kOpcodes;
}

uint8_t Cpu::pack(bool brk) const
{
    return (uint8_t)((N ? 0x80 : 0) | (V ? 0x40 : 0) | 0x20 | (brk ? 0x10 : 0) | (D ? 0x08 : 0) | (I ? 0x04 : 0) | (Z ? 0x02 : 0) | (C ? 0x01 : 0));
}

void Cpu::unpack(uint8_t p)
{
    N = (p & 0x80) != 0;
    V = (p & 0x40) != 0;
    D = (p & 0x08) != 0;
    I = (p & 0x04) != 0;
    Z = (p & 0x02) != 0;
    C = (p & 0x01) != 0;
}

void Cpu::reset()
{
    A = X = Y = 0;
    S = 0xFD;
    C = Z = N = V = D = false;
    I = true;
    depth = 0;
    cycles = 0;
    faulted = false;
    faultText.clear();
    misses.clear();
    interpretedInstructions = 0;
    pc = (uint16_t)(bus.read(0xFFFC) | (bus.read(0xFFFD) << 8));
}

void Cpu::fault(const char* what, uint16_t address)
{
    char text[128];
    std::snprintf(text, sizeof(text), "CPU fault: %s at $%04X (banks %02X %02X %02X %02X)", what, address,
                  bus.prgBank[0], bus.prgBank[1], bus.prgBank[2], bus.prgBank[3]);
    faulted = true;
    faultText = text;
}

uint32_t Cpu::step()
{
    if (pc >= 0x8000)
    {
        int window = (pc >> 13) & 3;
        int bank = bus.prgBank[window];
        unsigned offset = pc & 0x1FFF;
        if (kBankBase[bank] == (pc & 0xE000) && ((kEntryBits[bank][offset >> 3] >> (offset & 7)) & 1))
        {
            uint32_t result = kBankFunctions[bank](*this, pc);
            if (!(result & kExitMiss))
            {
                return result;
            }
        }
    }
    if (trackMisses)
    {
        uint32_t key = (pc >= 0x8000 ? (uint32_t)bus.prgBank[(pc >> 13) & 3] << 16 : 0xFF0000u) | pc;
        misses[key]++;
    }
    return interpret();
}

void Cpu::runMain(int64_t untilCycle)
{
    if (faulted)
    {
        return;
    }
    limit = untilCycle;
    while (cycles < limit)
    {
        uint32_t result = step();
        pc = (uint16_t)result;
        if (result & kExitFault)
        {
            fault("BRK executed", pc);
            return;
        }
        if (faulted)
        {
            return;
        }
        if (result & kExitRti)
        {
            depth = depth > 0 ? depth - 1 : 0;
        }
    }
}

void Cpu::interrupt(uint16_t vector)
{
    if (faulted)
    {
        return;
    }
    uint8_t* ram = bus.ram;
    ram[0x100 + S--] = (uint8_t)(pc >> 8);
    ram[0x100 + S--] = (uint8_t)pc;
    ram[0x100 + S--] = pack(false);
    I = true;
    cycles += 7;
    pc = vector;
    int64_t savedLimit = limit;
    limit = std::numeric_limits<int64_t>::max();
    int outer = depth;
    depth++;
    int64_t guard = cycles + 4000000;
    while (depth > outer)
    {
        uint32_t result = step();
        pc = (uint16_t)result;
        if (result & kExitFault)
        {
            fault("BRK executed in interrupt", pc);
            break;
        }
        if (faulted)
        {
            break;
        }
        if (result & kExitRti)
        {
            depth--;
        }
        if (cycles > guard)
        {
            fault("interrupt handler did not return", pc);
            break;
        }
    }
    depth = outer;
    limit = savedLimit;
}

uint32_t Cpu::interpret()
{
    interpretedInstructions++;
    uint8_t* ram = bus.ram;
    uint8_t opcode = bus.read(pc);
    const Opcode& info = kOpcodes.entries[opcode];
    if (info.op == Bad)
    {
        fault("invalid opcode", pc);
        return pc;
    }
    uint16_t operand = (uint16_t)(pc + 1);
    uint16_t next = pc;
    uint16_t address = 0;
    switch (info.mode)
    {
    case Imp:
    case Acc:
        next = (uint16_t)(pc + 1);
        break;
    case Imm:
        address = operand;
        next = (uint16_t)(pc + 2);
        break;
    case Zp:
        address = bus.read(operand);
        next = (uint16_t)(pc + 2);
        break;
    case Zpx:
        address = (uint8_t)(bus.read(operand) + X);
        next = (uint16_t)(pc + 2);
        break;
    case Zpy:
        address = (uint8_t)(bus.read(operand) + Y);
        next = (uint16_t)(pc + 2);
        break;
    case Abs:
        address = (uint16_t)(bus.read(operand) | (bus.read((uint16_t)(operand + 1)) << 8));
        next = (uint16_t)(pc + 3);
        break;
    case Absx:
        address = (uint16_t)((bus.read(operand) | (bus.read((uint16_t)(operand + 1)) << 8)) + X);
        next = (uint16_t)(pc + 3);
        break;
    case Absy:
        address = (uint16_t)((bus.read(operand) | (bus.read((uint16_t)(operand + 1)) << 8)) + Y);
        next = (uint16_t)(pc + 3);
        break;
    case Indx:
    {
        uint8_t z = (uint8_t)(bus.read(operand) + X);
        address = (uint16_t)(ram[z] | (ram[(uint8_t)(z + 1)] << 8));
        next = (uint16_t)(pc + 2);
        break;
    }
    case Indy:
    {
        uint8_t z = bus.read(operand);
        address = (uint16_t)((ram[z] | (ram[(uint8_t)(z + 1)] << 8)) + Y);
        next = (uint16_t)(pc + 2);
        break;
    }
    case Rel:
    {
        int8_t offset = (int8_t)bus.read(operand);
        next = (uint16_t)(pc + 2);
        address = (uint16_t)(next + offset);
        break;
    }
    case Ind:
    {
        uint16_t pointer = (uint16_t)(bus.read(operand) | (bus.read((uint16_t)(operand + 1)) << 8));
        uint16_t high = (uint16_t)((pointer & 0xFF00) | ((pointer + 1) & 0xFF));
        address = (uint16_t)(bus.read(pointer) | (bus.read(high) << 8));
        next = (uint16_t)(pc + 3);
        break;
    }
    }
    cycles += info.cycles;

    auto nz = [this](uint8_t value) {
        Z = value == 0;
        N = (value & 0x80) != 0;
    };
    auto adc = [this, &nz](uint8_t value) {
        unsigned sum = (unsigned)A + value + (C ? 1u : 0u);
        V = (~(A ^ value) & (A ^ sum) & 0x80) != 0;
        C = sum > 0xFF;
        A = (uint8_t)sum;
        nz(A);
    };
    auto compare = [this, &nz](uint8_t reg, uint8_t value) {
        C = reg >= value;
        nz((uint8_t)(reg - value));
    };
    auto branch = [this, &next, address](bool condition) {
        if (condition)
        {
            cycles++;
            next = address;
        }
    };
    auto modify = [this, &info, address](uint8_t (*fn)(Cpu&, uint8_t)) {
        if (info.mode == Acc)
        {
            A = fn(*this, A);
            return A;
        }
        uint8_t value = fn(*this, bus.read(address));
        bus.write(address, value);
        return value;
    };

    switch (info.op)
    {
    case Adc: adc(bus.read(address)); break;
    case Sbc: adc((uint8_t)(bus.read(address) ^ 0xFF)); break;
    case And: A &= bus.read(address); nz(A); break;
    case Ora: A |= bus.read(address); nz(A); break;
    case Eor: A ^= bus.read(address); nz(A); break;
    case Cmp: compare(A, bus.read(address)); break;
    case Cpx: compare(X, bus.read(address)); break;
    case Cpy: compare(Y, bus.read(address)); break;
    case Bit:
    {
        uint8_t value = bus.read(address);
        Z = (A & value) == 0;
        N = (value & 0x80) != 0;
        V = (value & 0x40) != 0;
        break;
    }
    case Lda: A = bus.read(address); nz(A); break;
    case Ldx: X = bus.read(address); nz(X); break;
    case Ldy: Y = bus.read(address); nz(Y); break;
    case Sta: bus.write(address, A); break;
    case Stx: bus.write(address, X); break;
    case Sty: bus.write(address, Y); break;
    case Asl:
        nz(modify([](Cpu& c, uint8_t v) { c.C = (v & 0x80) != 0; return (uint8_t)(v << 1); }));
        break;
    case Lsr:
        nz(modify([](Cpu& c, uint8_t v) { c.C = (v & 0x01) != 0; return (uint8_t)(v >> 1); }));
        break;
    case Rol:
        nz(modify([](Cpu& c, uint8_t v) { bool carry = c.C; c.C = (v & 0x80) != 0; return (uint8_t)((v << 1) | (carry ? 1 : 0)); }));
        break;
    case Ror:
        nz(modify([](Cpu& c, uint8_t v) { bool carry = c.C; c.C = (v & 0x01) != 0; return (uint8_t)((v >> 1) | (carry ? 0x80 : 0)); }));
        break;
    case Inc:
    {
        uint8_t value = (uint8_t)(bus.read(address) + 1);
        bus.write(address, value);
        nz(value);
        break;
    }
    case Dec:
    {
        uint8_t value = (uint8_t)(bus.read(address) - 1);
        bus.write(address, value);
        nz(value);
        break;
    }
    case Inx: X++; nz(X); break;
    case Iny: Y++; nz(Y); break;
    case Dex: X--; nz(X); break;
    case Dey: Y--; nz(Y); break;
    case Tax: X = A; nz(X); break;
    case Tay: Y = A; nz(Y); break;
    case Txa: A = X; nz(A); break;
    case Tya: A = Y; nz(A); break;
    case Tsx: X = S; nz(X); break;
    case Txs: S = X; break;
    case Clc: C = false; break;
    case Sec: C = true; break;
    case Cli: I = false; break;
    case Sei: I = true; break;
    case Cld: D = false; break;
    case Sed: D = true; break;
    case Clv: V = false; break;
    case Nop: break;
    case Pha: ram[0x100 + S--] = A; break;
    case Php: ram[0x100 + S--] = pack(true); break;
    case Pla: A = ram[0x100 + ++S]; nz(A); break;
    case Plp: unpack(ram[0x100 + ++S]); break;
    case Bcc: branch(!C); break;
    case Bcs: branch(C); break;
    case Beq: branch(Z); break;
    case Bne: branch(!Z); break;
    case Bmi: branch(N); break;
    case Bpl: branch(!N); break;
    case Bvc: branch(!V); break;
    case Bvs: branch(V); break;
    case Jmp: next = address; break;
    case Jsr:
    {
        uint16_t ret = (uint16_t)(pc + 2);
        ram[0x100 + S--] = (uint8_t)(ret >> 8);
        ram[0x100 + S--] = (uint8_t)ret;
        next = address;
        break;
    }
    case Rts:
    {
        uint16_t lo = ram[0x100 + ++S];
        uint16_t hi = ram[0x100 + ++S];
        next = (uint16_t)(((hi << 8) | lo) + 1);
        break;
    }
    case Rti:
    {
        unpack(ram[0x100 + ++S]);
        uint16_t lo = ram[0x100 + ++S];
        uint16_t hi = ram[0x100 + ++S];
        return (uint32_t)((hi << 8) | lo) | kExitRti;
    }
    case Brk:
        return (uint32_t)pc | kExitFault;
    case Bad:
        break;
    }
    return next;
}
}
