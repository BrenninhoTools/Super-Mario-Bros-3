#pragma once

#include <cstdint>
#include <functional>

namespace smb3
{
class Ppu
{
public:
    static constexpr int kWidth = 256;
    static constexpr int kHeight = 240;

    Ppu();

    void reset();
    void setChrBank(int slot, const uint8_t* data) { chr[slot] = data; }
    void setMirroring(bool vertical) { verticalMirroring = vertical; }

    uint8_t readRegister(int reg);
    void writeRegister(int reg, uint8_t value);
    void dma(const uint8_t* page);

    void setClock(const int64_t* source) { clock = source; }
    void beginVBlank();
    bool nmiEnabled() const { return (ctrl & 0x80) != 0; }
    bool renderingEnabled() const { return (mask & 0x18) != 0; }
    void renderFrame(uint32_t* pixels, const std::function<void()>& endOfLine);

    bool spriteLimit = true;

private:
    uint8_t chrByte(uint16_t address) const { return chr[(address >> 10) & 7][address & 0x3FF]; }
    uint16_t nametableIndex(uint16_t address) const;
    uint8_t readVram(uint16_t address);
    void writeVram(uint16_t address, uint8_t value);
    void incrementAddress();
    void incrementX();
    void incrementY();
    void copyX();
    void renderLine(int line, uint32_t* row);

    const uint8_t* chr[8];
    uint8_t nametables[0x800] = {};
    uint8_t palette[0x20] = {};
    uint8_t oam[0x100] = {};
    uint8_t oamAddress = 0;
    uint8_t ctrl = 0;
    uint8_t mask = 0;
    uint8_t status = 0;
    uint16_t v = 0;
    uint16_t t = 0;
    uint8_t fineX = 0;
    bool w = false;
    uint8_t readBuffer = 0;
    bool verticalMirroring = true;
    bool inFrame = false;
    const int64_t* clock = nullptr;
    int64_t vblankStart = 0;
};
}
