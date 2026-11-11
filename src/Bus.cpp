#include "Bus.hpp"

#include <cstring>

namespace smb3
{
namespace
{
const uint8_t kOpenBus[0x2000] = {};
}

Bus::Bus(Ppu& ppu, Apu& apu) : ppu(ppu), apu(apu)
{
    for (int i = 0; i < 4; i++)
    {
        prgMap[i] = kOpenBus;
    }
    apu.setMemoryReader([this](uint16_t address) { return read(address); });
}

void Bus::attach(const uint8_t* prgData, size_t prgSize, const uint8_t* chrData, size_t chrSize)
{
    prg = prgData;
    chr = chrData;
    prgBanks = prgSize / 0x2000;
    chrBanks = chrSize / 0x400;
    reset();
}

void Bus::reset()
{
    std::memset(ram, 0, sizeof(ram));
    std::memset(wram, 0, sizeof(wram));
    command = 0;
    const uint8_t initial[8] = {0, 2, 4, 5, 6, 7, 0, 1};
    std::memcpy(registers, initial, sizeof(registers));
    irqLatch = irqCounter = 0;
    irqReload = irqEnabled = irqLine = false;
    ppu.setMirroring(true);
    updatePrg();
    updateChr();
}

void Bus::updatePrg()
{
    if (!prg || prgBanks < 2)
    {
        return;
    }
    size_t last = prgBanks - 1;
    size_t second = prgBanks - 2;
    size_t r6 = registers[6] % prgBanks;
    size_t r7 = registers[7] % prgBanks;
    size_t banks[4];
    if (command & 0x40)
    {
        banks[0] = second;
        banks[2] = r6;
    }
    else
    {
        banks[0] = r6;
        banks[2] = second;
    }
    banks[1] = r7;
    banks[3] = last;
    for (int i = 0; i < 4; i++)
    {
        prgBank[i] = (uint8_t)banks[i];
        prgMap[i] = prg + banks[i] * 0x2000;
    }
}

void Bus::updateChr()
{
    if (!chr || chrBanks == 0)
    {
        return;
    }
    size_t banks[8];
    banks[0] = registers[0] & 0xFE;
    banks[1] = registers[0] | 0x01;
    banks[2] = registers[1] & 0xFE;
    banks[3] = registers[1] | 0x01;
    banks[4] = registers[2];
    banks[5] = registers[3];
    banks[6] = registers[4];
    banks[7] = registers[5];
    int flip = (command & 0x80) ? 4 : 0;
    for (int i = 0; i < 8; i++)
    {
        ppu.setChrBank((i + flip) & 7, chr + (banks[i] % chrBanks) * 0x400);
    }
}

uint8_t Bus::readIo(uint16_t address)
{
    if (address < 0x4000)
    {
        return ppu.readRegister(address & 7);
    }
    switch (address)
    {
    case 0x4015:
        return apu.readStatus();
    case 0x4016:
        return pads[0].read() | 0x40;
    case 0x4017:
        return pads[1].read() | 0x40;
    default:
        break;
    }
    if (address >= 0x6000)
    {
        return read(address);
    }
    return 0;
}

void Bus::writeIo(uint16_t address, uint8_t value)
{
    if (address < 0x2000)
    {
        ram[address & 0x7FF] = value;
        return;
    }
    if (address < 0x4000)
    {
        ppu.writeRegister(address & 7, value);
        return;
    }
    if (address == 0x4014)
    {
        uint8_t page[256];
        uint16_t base = (uint16_t)(value << 8);
        for (int i = 0; i < 256; i++)
        {
            page[i] = read((uint16_t)(base + i));
        }
        ppu.dma(page);
        return;
    }
    if (address == 0x4016)
    {
        pads[0].strobe(value);
        pads[1].strobe(value);
        return;
    }
    if (address <= 0x4017)
    {
        apu.writeRegister(address, value);
        return;
    }
    if (address >= 0x6000 && address < 0x8000)
    {
        wram[address - 0x6000] = value;
        return;
    }
    if (address < 0x8000)
    {
        return;
    }
    bool odd = address & 1;
    switch (address & 0xE000)
    {
    case 0x8000:
        if (!odd)
        {
            command = value;
            updatePrg();
            updateChr();
        }
        else
        {
            registers[command & 7] = value;
            if ((command & 7) >= 6)
            {
                updatePrg();
            }
            else
            {
                updateChr();
            }
        }
        break;
    case 0xA000:
        if (!odd)
        {
            ppu.setMirroring(!(value & 1));
        }
        break;
    case 0xC000:
        if (!odd)
        {
            irqLatch = value;
        }
        else
        {
            irqCounter = 0;
            irqReload = true;
        }
        break;
    case 0xE000:
        if (!odd)
        {
            irqEnabled = false;
            irqLine = false;
        }
        else
        {
            irqEnabled = true;
        }
        break;
    default:
        break;
    }
}

void Bus::clockScanline()
{
    if (irqCounter == 0 || irqReload)
    {
        irqCounter = irqLatch;
        irqReload = false;
    }
    else
    {
        irqCounter--;
    }
    if (irqCounter == 0 && irqEnabled)
    {
        irqLine = true;
    }
}
}
