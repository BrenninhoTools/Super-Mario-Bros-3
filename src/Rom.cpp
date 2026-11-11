#include "Rom.hpp"

#include <SDL.h>

#include <cstdio>
#include <cstring>

#include "Generated.hpp"

namespace smb3
{
namespace
{
const size_t kPrgSize = 0x40000;
const size_t kChrSize = 0x20000;
}

uint32_t Rom::crc32(const uint8_t* data, size_t size)
{
    static uint32_t table[256];
    static bool ready = false;
    if (!ready)
    {
        for (uint32_t i = 0; i < 256; i++)
        {
            uint32_t c = i;
            for (int bit = 0; bit < 8; bit++)
            {
                c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
            }
            table[i] = c;
        }
        ready = true;
    }
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; i++)
    {
        crc = (crc >> 8) ^ table[(crc ^ data[i]) & 0xFF];
    }
    return ~crc;
}

bool Rom::loadFile(const std::string& path, std::string& error)
{
    SDL_RWops* file = SDL_RWFromFile(path.c_str(), "rb");
    if (!file)
    {
        error = "Could not open " + path;
        return false;
    }
    Sint64 size = SDL_RWsize(file);
    if (size <= 0 || size > 4 * 1024 * 1024)
    {
        SDL_RWclose(file);
        error = "Invalid ROM file size";
        return false;
    }
    std::vector<uint8_t> data((size_t)size);
    size_t read = SDL_RWread(file, data.data(), 1, data.size());
    SDL_RWclose(file);
    if (read != data.size())
    {
        error = "Could not read " + path;
        return false;
    }
    return loadMemory(data.data(), data.size(), error);
}

bool Rom::loadMemory(const uint8_t* data, size_t size, std::string& error)
{
    isLoaded = false;
    size_t offset = 0;
    if (size >= 16 && std::memcmp(data, "NES\x1A", 4) == 0)
    {
        bool cleanHeader = data[12] == 0 && data[13] == 0 && data[14] == 0 && data[15] == 0;
        int mapper = (data[6] >> 4) | (cleanHeader ? (data[7] & 0xF0) : 0);
        size_t prgBytes = (size_t)data[4] * 0x4000;
        size_t chrBytes = (size_t)data[5] * 0x2000;
        if (mapper != 4 || prgBytes != kPrgSize || chrBytes != kChrSize)
        {
            error = "This is not a Super Mario Bros. 3 ROM (wrong mapper or size)";
            return false;
        }
        offset = 16 + ((data[6] & 0x04) ? 512 : 0);
    }
    if (size < offset + kPrgSize + kChrSize)
    {
        error = "The ROM file is too small";
        return false;
    }
    uint32_t crc = crc32(data + offset, kPrgSize);
    if (crc != kPrgCrc)
    {
        char text[160];
        std::snprintf(text, sizeof(text), "Unsupported ROM (PRG CRC %08X). Use Super Mario Bros. 3 (USA) (Rev 1)", crc);
        error = text;
        return false;
    }
    prgData.assign(data + offset, data + offset + kPrgSize);
    chrData.assign(data + offset + kPrgSize, data + offset + kPrgSize + kChrSize);
    isLoaded = true;
    return true;
}
}
