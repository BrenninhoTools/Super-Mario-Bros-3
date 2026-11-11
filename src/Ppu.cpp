#include "Ppu.hpp"

#include <cstring>

namespace smb3
{
namespace
{
const uint8_t kBlankChr[0x400] = {};
const int64_t kVBlankCycles = 2273;

const uint32_t kNesPalette[64] = {
    0x666666, 0x002A88, 0x1412A7, 0x3B00A4, 0x5C007E, 0x6E0040, 0x6C0600, 0x561D00,
    0x333500, 0x0B4800, 0x005200, 0x004F08, 0x00404D, 0x000000, 0x000000, 0x000000,
    0xADADAD, 0x155FD9, 0x4240FF, 0x7527FE, 0xA01ACC, 0xB71E7B, 0xB53120, 0x994E00,
    0x6B6D00, 0x388700, 0x0C9300, 0x008F32, 0x007C8D, 0x000000, 0x000000, 0x000000,
    0xFFFEFF, 0x64B0FF, 0x9290FF, 0xC676FF, 0xF36AFF, 0xFE6ECC, 0xFE8170, 0xEA9E22,
    0xBCBE00, 0x88D800, 0x5CE430, 0x45E082, 0x48CDDE, 0x4F4F4F, 0x000000, 0x000000,
    0xFFFEFF, 0xC0DFFF, 0xD3D2FF, 0xE8C8FF, 0xFBC2FF, 0xFEC4EA, 0xFECCC5, 0xF7D8A5,
    0xE4E594, 0xCFEF96, 0xBDF4AB, 0xB3F3CC, 0xB5EBF2, 0xB8B8B8, 0x000000, 0x000000};
}

Ppu::Ppu()
{
    for (int i = 0; i < 8; i++)
    {
        chr[i] = kBlankChr;
    }
    reset();
}

void Ppu::reset()
{
    std::memset(nametables, 0, sizeof(nametables));
    std::memset(palette, 0x0F, sizeof(palette));
    std::memset(oam, 0xF8, sizeof(oam));
    oamAddress = 0;
    ctrl = mask = status = 0;
    v = t = 0;
    fineX = 0;
    w = false;
    readBuffer = 0;
    inFrame = false;
}

uint16_t Ppu::nametableIndex(uint16_t address) const
{
    if (verticalMirroring)
    {
        return address & 0x7FF;
    }
    return (uint16_t)(((address >> 1) & 0x400) | (address & 0x3FF));
}

uint8_t Ppu::readVram(uint16_t address)
{
    address &= 0x3FFF;
    if (address < 0x2000)
    {
        return chrByte(address);
    }
    if (address < 0x3F00)
    {
        return nametables[nametableIndex(address)];
    }
    uint8_t index = address & 0x1F;
    if ((index & 0x13) == 0x10)
    {
        index &= 0x0F;
    }
    return palette[index];
}

void Ppu::writeVram(uint16_t address, uint8_t value)
{
    address &= 0x3FFF;
    if (address < 0x2000)
    {
        return;
    }
    if (address < 0x3F00)
    {
        nametables[nametableIndex(address)] = value;
        return;
    }
    uint8_t index = address & 0x1F;
    if ((index & 0x13) == 0x10)
    {
        index &= 0x0F;
    }
    palette[index] = value & 0x3F;
}

void Ppu::incrementX()
{
    if ((v & 0x001F) == 31)
    {
        v &= (uint16_t)~0x001F;
        v ^= 0x0400;
    }
    else
    {
        v++;
    }
}

void Ppu::incrementY()
{
    if ((v & 0x7000) != 0x7000)
    {
        v += 0x1000;
        return;
    }
    v &= (uint16_t)~0x7000;
    int y = (v & 0x03E0) >> 5;
    if (y == 29)
    {
        y = 0;
        v ^= 0x0800;
    }
    else if (y == 31)
    {
        y = 0;
    }
    else
    {
        y++;
    }
    v = (uint16_t)((v & ~0x03E0) | (y << 5));
}

void Ppu::copyX()
{
    v = (uint16_t)((v & ~0x041F) | (t & 0x041F));
}

void Ppu::incrementAddress()
{
    if (inFrame && renderingEnabled())
    {
        incrementX();
        incrementY();
        return;
    }
    v = (uint16_t)((v + ((ctrl & 0x04) ? 32 : 1)) & 0x7FFF);
}

uint8_t Ppu::readRegister(int reg)
{
    switch (reg)
    {
    case 2:
    {
        if ((status & 0x80) && clock && *clock - vblankStart >= kVBlankCycles)
        {
            status &= 0x7F;
        }
        uint8_t value = status;
        status &= 0x7F;
        w = false;
        return value;
    }
    case 4:
        return oam[oamAddress];
    case 7:
    {
        uint16_t address = v & 0x3FFF;
        uint8_t value;
        if (address >= 0x3F00)
        {
            value = readVram(address);
            readBuffer = readVram((uint16_t)(address - 0x1000));
        }
        else
        {
            value = readBuffer;
            readBuffer = readVram(address);
        }
        incrementAddress();
        return value;
    }
    default:
        return 0;
    }
}

void Ppu::writeRegister(int reg, uint8_t value)
{
    switch (reg)
    {
    case 0:
        ctrl = value;
        t = (uint16_t)((t & 0xF3FF) | ((value & 0x03) << 10));
        break;
    case 1:
        mask = value;
        break;
    case 3:
        oamAddress = value;
        break;
    case 4:
        oam[oamAddress++] = value;
        break;
    case 5:
        if (!w)
        {
            t = (uint16_t)((t & 0xFFE0) | (value >> 3));
            fineX = value & 7;
        }
        else
        {
            t = (uint16_t)((t & 0x8C1F) | ((value & 0xF8) << 2) | ((value & 0x07) << 12));
        }
        w = !w;
        break;
    case 6:
        if (!w)
        {
            t = (uint16_t)((t & 0x00FF) | ((value & 0x3F) << 8));
        }
        else
        {
            t = (uint16_t)((t & 0xFF00) | value);
            v = t;
        }
        w = !w;
        break;
    case 7:
        writeVram(v, value);
        incrementAddress();
        break;
    default:
        break;
    }
}

void Ppu::dma(const uint8_t* page)
{
    for (int i = 0; i < 256; i++)
    {
        oam[(uint8_t)(oamAddress + i)] = page[i];
    }
}

void Ppu::beginVBlank()
{
    status |= 0x80;
    if (clock)
    {
        vblankStart = *clock;
    }
}

void Ppu::renderLine(int line, uint32_t* row)
{
    uint8_t background[kWidth + 16];
    uint8_t sprite[kWidth];
    std::memset(background, 0, sizeof(background));

    bool showBackground = (mask & 0x08) != 0;
    bool showSprites = (mask & 0x10) != 0;

    if (showBackground)
    {
        uint16_t address = v;
        uint16_t patternBase = (ctrl & 0x10) ? 0x1000 : 0;
        int fineY = (address >> 12) & 7;
        for (int tile = 0; tile < 33; tile++)
        {
            uint8_t index = nametables[nametableIndex((uint16_t)(0x2000 | (address & 0x0FFF)))];
            uint16_t attributeAddress = (uint16_t)(0x23C0 | (address & 0x0C00) | ((address >> 4) & 0x38) | ((address >> 2) & 0x07));
            uint8_t attribute = nametables[nametableIndex(attributeAddress)];
            int shift = ((address >> 4) & 4) | (address & 2);
            uint8_t group = (uint8_t)(((attribute >> shift) & 3) << 2);
            uint16_t pattern = (uint16_t)(patternBase + index * 16 + fineY);
            uint8_t lo = chrByte(pattern);
            uint8_t hi = chrByte((uint16_t)(pattern + 8));
            uint8_t* out = background + tile * 8;
            for (int px = 0; px < 8; px++)
            {
                int bit = 7 - px;
                uint8_t color = (uint8_t)(((lo >> bit) & 1) | (((hi >> bit) & 1) << 1));
                out[px] = color ? (uint8_t)(group | color) : 0;
            }
            if ((address & 0x001F) == 31)
            {
                address = (uint16_t)((address & ~0x001F) ^ 0x0400);
            }
            else
            {
                address++;
            }
        }
        if (fineX)
        {
            std::memmove(background, background + fineX, kWidth);
        }
        if (!(mask & 0x02))
        {
            std::memset(background, 0, 8);
        }
    }

    uint32_t colors[32];
    uint8_t grayscale = (mask & 0x01) ? 0x30 : 0x3F;
    for (int i = 0; i < 32; i++)
    {
        colors[i] = 0xFF000000u | kNesPalette[palette[i] & grayscale];
    }

    bool anySprite = false;
    if (showSprites && line > 0)
    {
        int height = (ctrl & 0x20) ? 16 : 8;
        int found = 0;
        int minX = (mask & 0x04) ? 0 : 8;
        for (int i = 0; i < 64; i++)
        {
            const uint8_t* entry = &oam[i * 4];
            int rowInSprite = line - (entry[0] + 1);
            if (rowInSprite < 0 || rowInSprite >= height)
            {
                continue;
            }
            if (++found > 8)
            {
                status |= 0x20;
                if (spriteLimit)
                {
                    break;
                }
            }
            if (!anySprite)
            {
                std::memset(sprite, 0, sizeof(sprite));
                anySprite = true;
            }
            uint8_t tile = entry[1];
            uint8_t attributes = entry[2];
            int sx = entry[3];
            if (attributes & 0x80)
            {
                rowInSprite = height - 1 - rowInSprite;
            }
            uint16_t address;
            if (height == 16)
            {
                address = (uint16_t)(((tile & 1) ? 0x1000 : 0) + (tile & 0xFE) * 16);
                if (rowInSprite >= 8)
                {
                    address = (uint16_t)(address + 16);
                    rowInSprite -= 8;
                }
            }
            else
            {
                address = (uint16_t)(((ctrl & 0x08) ? 0x1000 : 0) + tile * 16);
            }
            uint8_t lo = chrByte((uint16_t)(address + rowInSprite));
            uint8_t hi = chrByte((uint16_t)(address + rowInSprite + 8));
            if (!(lo | hi))
            {
                continue;
            }
            uint8_t paletteBits = (uint8_t)(0x10 | ((attributes & 3) << 2));
            uint8_t behind = (attributes & 0x20) ? 0x80 : 0;
            bool flip = (attributes & 0x40) != 0;
            for (int px = 0; px < 8; px++)
            {
                int x = sx + px;
                if (x >= kWidth)
                {
                    break;
                }
                if (x < minX)
                {
                    continue;
                }
                int bit = flip ? px : 7 - px;
                int color = ((lo >> bit) & 1) | (((hi >> bit) & 1) << 1);
                if (!color)
                {
                    continue;
                }
                if (i == 0 && background[x] && x != 255 && showBackground)
                {
                    status |= 0x40;
                }
                if (!sprite[x])
                {
                    sprite[x] = (uint8_t)(behind | paletteBits | color);
                }
            }
        }
    }

    uint32_t backdrop = colors[0];
    if (!anySprite)
    {
        for (int x = 0; x < kWidth; x++)
        {
            row[x] = background[x] ? colors[background[x]] : backdrop;
        }
        return;
    }
    for (int x = 0; x < kWidth; x++)
    {
        uint8_t s = sprite[x];
        uint8_t b = background[x];
        if (s && (!(s & 0x80) || !b))
        {
            row[x] = colors[s & 0x1F];
        }
        else
        {
            row[x] = b ? colors[b] : backdrop;
        }
    }
}

void Ppu::renderFrame(uint32_t* pixels, const std::function<void()>& endOfLine)
{
    status &= (uint8_t)~0x60;
    inFrame = true;
    if (renderingEnabled())
    {
        v = t;
        endOfLine();
    }
    for (int line = 0; line < kHeight; line++)
    {
        uint32_t* row = pixels + line * kWidth;
        if (renderingEnabled())
        {
            renderLine(line, row);
            incrementY();
            copyX();
            endOfLine();
        }
        else
        {
            uint32_t backdrop = 0xFF000000u | kNesPalette[palette[0] & 0x3F];
            for (int x = 0; x < kWidth; x++)
            {
                row[x] = backdrop;
            }
        }
    }
    inFrame = false;
}
}
