#pragma once

#include "../machine-ir/MachineIR.hpp"

namespace mips {

class LocalRegisterAllocator {
    // The printer reserves $t0–$t2 for operand loads and $t9 for large stack offsets.
    static constexpr std::array<Register, 6> temporaries{{
        {"$t3"}, {"$t4"}, {"$t5"}, {"$t6"}, {"$t7"}, {"$t8"}
    }};

    struct LiveRange {
        std::size_t first;
        std::size_t last;
        std::size_t region;
        bool crossesBoundary = false;
    };

    template <typename Visitor>
    static void visitRegisterOperands(Instruction& instruction, Visitor visit) {
        if (instruction.destination) visit(*instruction.destination);
        for (auto& operand : instruction.operands) {
            if (auto* value = std::get_if<Value>(&operand)) visit(*value);
            if (auto* memory = std::get_if<Memory>(&operand)) visit(memory->address);
        }
    }

    static bool endsRegion(const Instruction& instruction) {
        const auto opcode = instruction.opcode;
        return opcode == "j" || opcode == "beq" || opcode == "blez"
            || opcode == "jal" || opcode == "syscall";
    }

    static void allocate(Function& function) {
        std::vector<std::optional<LiveRange>> ranges(function.virtualRegisters);
        std::vector<std::size_t> firstAppearance;
        std::size_t region = 0;
        for (std::size_t i = 0; i < function.body.size(); ++i) {
            auto* instruction = std::get_if<Instruction>(&function.body[i]);
            if (!instruction) {
                ++region;
                continue;
            }
            visitRegisterOperands(*instruction, [&](const auto& operand) {
                const auto* virtualRegister = std::get_if<VirtualRegister>(&operand);
                if (!virtualRegister) return;
                auto& range = ranges[virtualRegister->id];
                if (!range) {
                    range = LiveRange{i, i, region};
                    firstAppearance.push_back(virtualRegister->id);
                } else {
                    range->last = i;
                    range->crossesBoundary |= range->region != region;
                }
            });
            if (endsRegion(*instruction)) ++region;
        }

        std::vector<Destination> locations(function.virtualRegisters, VirtualRegister{0});
        std::array<std::optional<std::size_t>, temporaries.size()> occupiedUntil{};
        std::size_t spillCount = 0;
        // A register is reusable only after the previous value's last instruction.
        for (const auto id : firstAppearance) {
            const auto& range = *ranges[id];
            if (!range.crossesBoundary) {
                for (std::size_t i = 0; i < temporaries.size(); ++i) {
                    if (occupiedUntil[i] && *occupiedUntil[i] >= range.first) continue;
                    occupiedUntil[i] = range.last;
                    locations[id] = temporaries[i];
                    break;
                }
            }
            if (auto* spill = std::get_if<VirtualRegister>(&locations[id])) spill->id = spillCount++;
        }

        // Rewrite uses and definitions together. The remaining virtual registers
        // are numbered densely so the printer allocates slots only for spills.
        for (auto& statement : function.body) {
            if (auto* instruction = std::get_if<Instruction>(&statement)) {
                visitRegisterOperands(*instruction, [&](auto& operand) {
                    if (const auto* virtualRegister = std::get_if<VirtualRegister>(&operand)) {
                        std::visit([&](auto location) { operand = location; }, locations[virtualRegister->id]);
                    }
                });
            }
        }
        function.virtualRegisters = spillCount;
    }

public:
    void run(Program& program) const {
        for (auto& function : program.functions) allocate(function);
    }
};

} // namespace mips
