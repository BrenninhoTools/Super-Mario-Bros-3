#pragma once

#include <cstdint>

#include "Bus.hpp"
#include "Cpu.hpp"

namespace smb3
{
constexpr int kBankCount = 32;
constexpr int kBankSize = 0x2000;

constexpr uint32_t kExitYield = 0x10000u;
constexpr uint32_t kExitRti = 0x20000u;
constexpr uint32_t kExitFault = 0x40000u;
constexpr uint32_t kExitMiss = 0x80000u;

using BankFunction = uint32_t (*)(Cpu&, uint16_t);

extern const uint32_t kPrgCrc;
extern const uint16_t kNmiVector;
extern const uint16_t kResetVector;
extern const uint16_t kIrqVector;
extern const BankFunction kBankFunctions[kBankCount];
extern const uint16_t kBankBase[kBankCount];
extern const uint8_t kEntryBits[kBankCount][kBankSize / 8];

uint32_t bank00(Cpu&, uint16_t);
uint32_t bank01(Cpu&, uint16_t);
uint32_t bank02(Cpu&, uint16_t);
uint32_t bank03(Cpu&, uint16_t);
uint32_t bank04(Cpu&, uint16_t);
uint32_t bank05(Cpu&, uint16_t);
uint32_t bank06(Cpu&, uint16_t);
uint32_t bank07(Cpu&, uint16_t);
uint32_t bank08(Cpu&, uint16_t);
uint32_t bank09(Cpu&, uint16_t);
uint32_t bank10(Cpu&, uint16_t);
uint32_t bank11(Cpu&, uint16_t);
uint32_t bank12(Cpu&, uint16_t);
uint32_t bank13(Cpu&, uint16_t);
uint32_t bank14(Cpu&, uint16_t);
uint32_t bank15(Cpu&, uint16_t);
uint32_t bank16(Cpu&, uint16_t);
uint32_t bank17(Cpu&, uint16_t);
uint32_t bank18(Cpu&, uint16_t);
uint32_t bank19(Cpu&, uint16_t);
uint32_t bank20(Cpu&, uint16_t);
uint32_t bank21(Cpu&, uint16_t);
uint32_t bank22(Cpu&, uint16_t);
uint32_t bank23(Cpu&, uint16_t);
uint32_t bank24(Cpu&, uint16_t);
uint32_t bank25(Cpu&, uint16_t);
uint32_t bank26(Cpu&, uint16_t);
uint32_t bank27(Cpu&, uint16_t);
uint32_t bank28(Cpu&, uint16_t);
uint32_t bank29(Cpu&, uint16_t);
uint32_t bank30(Cpu&, uint16_t);
uint32_t bank31(Cpu&, uint16_t);
}

#define BANK_PROLOGUE()                  \
    Bus& bus = c.bus;                    \
    uint8_t* const ram = bus.ram;        \
    uint8_t* const wram = bus.wram;      \
    uint8_t A = c.A;                     \
    uint8_t X = c.X;                     \
    uint8_t Y = c.Y;                     \
    uint8_t S = c.S;                     \
    bool C = c.C;                        \
    bool Z = c.Z;                        \
    bool N = c.N;                        \
    bool V = c.V;                        \
    int64_t cyc = c.cycles;              \
    const int64_t limit = c.limit;       \
    uint8_t t = 0;                       \
    unsigned u = 0;                      \
    (void)t;                             \
    (void)wram;                          \
    (void)u

#define SAVE() (c.A = A, c.X = X, c.Y = Y, c.S = S, c.C = C, c.Z = Z, c.N = N, c.V = V, c.cycles = cyc)
#define EXIT(pc) do { SAVE(); return (uint32_t)(uint16_t)(pc); } while (0)
#define EXIT_YIELD(pc) do { SAVE(); return (uint32_t)(uint16_t)(pc) | kExitYield; } while (0)
#define EXIT_RTI(pc) do { SAVE(); return (uint32_t)(uint16_t)(pc) | kExitRti; } while (0)
#define EXIT_FAULT(pc) do { SAVE(); return (uint32_t)(uint16_t)(pc) | kExitFault; } while (0)

#define NZ(v) (Z = !(v), N = ((v) & 0x80) != 0)
#define ADC(v) do { u = (unsigned)A + (v) + (C ? 1u : 0u); V = (~(A ^ (v)) & (A ^ u) & 0x80) != 0; C = u > 0xFF; A = (uint8_t)u; NZ(A); } while (0)
#define CMP(r, v) do { C = (r) >= (v); t = (uint8_t)((r) - (v)); NZ(t); } while (0)
#define PUSH(v) (ram[0x100 + S--] = (uint8_t)(v))
#define PULL() (ram[0x100 + ++S])
#define PACK() (uint8_t)((N ? 0x80 : 0) | (V ? 0x40 : 0) | 0x30 | (c.D ? 0x08 : 0) | (c.I ? 0x04 : 0) | (Z ? 0x02 : 0) | (C ? 0x01 : 0))
#define UNPACK(p) (N = ((p) & 0x80) != 0, V = ((p) & 0x40) != 0, c.D = ((p) & 0x08) != 0, c.I = ((p) & 0x04) != 0, Z = ((p) & 0x02) != 0, C = ((p) & 0x01) != 0)
#define PRG(a) (bus.prgMap[((a) >> 13) & 3][(a) & 0x1FFF])
#define READ16(lo, hi) (unsigned)(bus.read(lo) | (bus.read(hi) << 8))
#define WRITE_IO(a, v) bus.write((uint16_t)(a), (v))
