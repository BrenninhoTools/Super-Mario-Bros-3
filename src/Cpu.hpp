#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace smb3
{
class Bus;

class Cpu
{
public:
    explicit Cpu(Bus& bus) : bus(bus) {}

    void reset();
    void runMain(int64_t untilCycle);
    void interrupt(uint16_t vector);

    bool hasFaulted() const { return faulted; }
    const std::string& faultMessage() const { return faultText; }

    Bus& bus;
    uint8_t A = 0;
    uint8_t X = 0;
    uint8_t Y = 0;
    uint8_t S = 0xFD;
    bool C = false;
    bool Z = false;
    bool N = false;
    bool V = false;
    bool I = true;
    bool D = false;
    uint16_t pc = 0;
    int64_t cycles = 0;
    int64_t limit = 0;

    bool trackMisses = false;
    std::map<uint32_t, uint64_t> misses;
    uint64_t interpretedInstructions = 0;

private:
    uint32_t step();
    uint32_t interpret();
    void fault(const char* what, uint16_t address);
    uint8_t pack(bool brk) const;
    void unpack(uint8_t p);

    int depth = 0;
    bool faulted = false;
    std::string faultText;
};
}
