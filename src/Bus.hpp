#pragma once

#include <cstddef>
#include <cstdint>

#include "Apu.hpp"
#include "Controller.hpp"
#include "Ppu.hpp"

namespace smb3
{
class Bus
{
public:
    Bus(Ppu& ppu, Apu& apu);

    void attach(const uint8_t* prgData, size_t prgSize, const uint8_t* chrData, size_t chrSize);
    void reset();

    uint8_t read(uint16_t address)
    {
        if (address < 0x2000)
        {
            return ram[address & 0x7FF];
        }
        if (address >= 0x8000)
        {
            return prgMap[(address >> 13) & 3][address & 0x1FFF];
        }
        if (address >= 0x6000)
        {
            return wram[address - 0x6000];
        }
        return readIo(address);
    }

    void write(uint16_t address, uint8_t value)
    {
        if (address < 0x2000)
        {
            ram[address & 0x7FF] = value;
            return;
        }
        if (address >= 0x6000 && address < 0x8000)
        {
            wram[address - 0x6000] = value;
            return;
        }
        writeIo(address, value);
    }

    uint8_t readIo(uint16_t address);
    void writeIo(uint16_t address, uint8_t value);

    void clockScanline();
    bool irqAsserted() const { return irqLine; }

    uint8_t ram[0x800] = {};
    uint8_t wram[0x2000] = {};
    const uint8_t* prgMap[4] = {};
    uint8_t prgBank[4] = {};
    Controller pads[2];

private:
    void updatePrg();
    void updateChr();

    Ppu& ppu;
    Apu& apu;
    const uint8_t* prg = nullptr;
    const uint8_t* chr = nullptr;
    size_t prgBanks = 0;
    size_t chrBanks = 0;
    uint8_t command = 0;
    uint8_t registers[8] = {};
    uint8_t irqLatch = 0;
    uint8_t irqCounter = 0;
    bool irqReload = false;
    bool irqEnabled = false;
    bool irqLine = false;
};
}
