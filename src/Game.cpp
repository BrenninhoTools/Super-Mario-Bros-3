#include "Game.hpp"

#include "Generated.hpp"

namespace smb3
{
namespace
{
const double kFrameRate = 60.0988;
const int64_t kCyclesPerFrame = 29781;
}

Game::Game() : bus(ppu, apu), cpu(bus), pixels(Ppu::kWidth * Ppu::kHeight, 0xFF000000u)
{
    apu.reset(sampleRate);
    ppu.setClock(&cpu.cycles);
}

void Game::setSampleRate(int rate)
{
    sampleRate = rate;
    apu.reset(rate);
}

bool Game::loadRom(const std::string& path, std::string& error)
{
    if (!rom.loadFile(path, error))
    {
        return false;
    }
    attach();
    return true;
}

bool Game::loadRom(const uint8_t* data, size_t size, std::string& error)
{
    if (!rom.loadMemory(data, size, error))
    {
        return false;
    }
    attach();
    return true;
}

void Game::attach()
{
    bus.attach(rom.prg(), rom.prgSize(), rom.chr(), rom.chrSize());
    reset();
}

void Game::reset()
{
    if (!rom.loaded())
    {
        return;
    }
    ppu.reset();
    bus.reset();
    apu.reset(sampleRate);
    sampleRemainder = 0.0;
    cpu.reset();
    frameCycle = 0;
}

void Game::endOfLine()
{
    if (!ppu.renderingEnabled())
    {
        return;
    }
    bus.clockScanline();
    if (bus.irqAsserted() && !cpu.I)
    {
        cpu.interrupt(kIrqVector);
    }
}

void Game::runFrame(uint8_t pad1, uint8_t pad2)
{
    if (!ready())
    {
        return;
    }
    bus.pads[0].setButtons(pad1);
    bus.pads[1].setButtons(pad2);
    if (cpu.cycles < frameCycle)
    {
        cpu.cycles = frameCycle;
    }
    frameCycle += kCyclesPerFrame;
    ppu.beginVBlank();
    if (ppu.nmiEnabled())
    {
        cpu.interrupt(kNmiVector);
    }
    ppu.renderFrame(pixels.data(), [this]() { endOfLine(); });
    cpu.runMain(frameCycle);
}

int Game::produceAudio(int16_t* out, int maxSamples, double rate)
{
    apu.setRateAdjust(rate);
    sampleRemainder += sampleRate * rate / kFrameRate;
    int count = (int)sampleRemainder;
    sampleRemainder -= count;
    if (count > maxSamples)
    {
        count = maxSamples;
    }
    apu.generate(out, count);
    return count;
}
}
