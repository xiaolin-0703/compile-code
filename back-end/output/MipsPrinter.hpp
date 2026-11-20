#pragma once

#include "../machine-ir/MachineIR.hpp"

#include <algorithm>
#include <limits>
#include <ostream>
#include <stdexcept>

namespace mips {

class MipsPrinter {
    struct Frame {
        std::vector<std::int64_t> locals;
        std::int64_t virtualBase;
        std::int64_t returnAddress;
        std::int64_t size;

        static std::int64_t align(std::int64_t offset, std::int64_t alignment) {
            return (offset + alignment - 1) / alignment * alignment;
        }

        Frame(const Function& function, bool isMain) {
            std::int64_t offset = 4 * std::int64_t(function.outgoingWords);
            for (const auto& object : function.locals) {
                offset = align(offset, object.alignment);
                locals.push_back(offset);
                offset += object.size;
            }
            virtualBase = align(offset, 4);
            returnAddress = virtualBase + 4 * std::int64_t(function.virtualRegisters);
            size = align(returnAddress + (!isMain && function.savesReturnAddress ? 4 : 0), 8);
            if (size > std::numeric_limits<std::int32_t>::max()) {
                throw std::logic_error("MIPS stack frame exceeds the address range");
            }
        }

        std::int64_t offset(VirtualRegister reg) const { return virtualBase + 4 * std::int64_t(reg.id); }

        std::int64_t offset(StackAddress address) const {
            switch (address.area) {
                case StackArea::Local: return locals.at(address.index);
                case StackArea::Incoming: return size + 4 * std::int64_t(address.index);
                case StackArea::Outgoing: return 4 * std::int64_t(address.index);
            }
            throw std::logic_error("Unknown MIPS stack area");
        }
    };

    class FunctionPrinter {
        std::ostream& out;
        const Frame frame;

        void addStackOffset(std::string_view reg, std::int64_t offset) {
            if (offset == 0 && reg == "$sp") return;
            if (fitsSignedImmediate(offset)) out << "    addiu " << reg << ", $sp, " << offset << '\n';
            else {
                out << "    li $t9, " << offset << '\n';
                out << "    addu " << reg << ", $sp, $t9\n";
            }
        }

        std::string stackMemory(std::int64_t offset) {
            if (fitsSignedImmediate(offset)) return std::to_string(offset) + "($sp)";
            addStackOffset("$t9", offset);
            return "0($t9)";
        }

        void stackAccess(std::string_view opcode, std::string_view reg, std::int64_t offset) {
            const auto address = stackMemory(offset);
            out << "    " << opcode << ' ' << reg << ", " << address << '\n';
        }

        void materialize(const Value& value, std::string_view into) {
            if (const auto* reg = std::get_if<Register>(&value)) {
                if (reg->name != into) out << "    move " << into << ", " << reg->name << '\n';
            } else if (const auto* reg = std::get_if<VirtualRegister>(&value)) {
                stackAccess("lw", into, frame.offset(*reg));
            } else if (const auto* immediate = std::get_if<std::int64_t>(&value)) {
                if (*immediate == 0) out << "    move " << into << ", $zero\n";
                else out << "    li " << into << ", " << *immediate << '\n';
            } else if (const auto* address = std::get_if<StackAddress>(&value)) {
                addStackOffset(into, frame.offset(*address));
            } else {
                out << "    la " << into << ", " << std::get<Label>(value).name << '\n';
            }
        }

        std::string_view sourceRegister(const Value& value, std::string_view scratch) {
            if (const auto* reg = std::get_if<Register>(&value)) return reg->name;
            if (const auto* immediate = std::get_if<std::int64_t>(&value); immediate && *immediate == 0) return "$zero";
            materialize(value, scratch);
            return scratch;
        }

        std::string render(const Operand& operand, std::size_t index) {
            static constexpr std::array<std::string_view, 2> scratch{"$t0", "$t1"};
            if (const auto* value = std::get_if<Value>(&operand)) {
                return std::string(sourceRegister(*value, scratch.at(index)));
            }
            if (const auto* memory = std::get_if<Memory>(&operand)) {
                if (const auto* address = std::get_if<StackAddress>(&memory->address)) {
                    return stackMemory(frame.offset(*address));
                }
                return "0(" + std::string(sourceRegister(memory->address, scratch.at(index))) + ")";
            }
            if (const auto* label = std::get_if<Label>(&operand)) return label->name;
            return std::to_string(std::get<std::int64_t>(operand));
        }

        void print(const Instruction& instruction) {
            const Register* physical = nullptr;
            const VirtualRegister* spilled = nullptr;
            if (instruction.destination) {
                physical = std::get_if<Register>(&*instruction.destination);
                spilled = std::get_if<VirtualRegister>(&*instruction.destination);
            }
            const auto destination = physical ? physical->name : std::string_view("$t2");
            if (instruction.opcode == "move") {
                materialize(std::get<Value>(instruction.operands.front()), destination);
            } else {
                // Operand loads come before the instruction consuming them.
                std::vector<std::string> operands;
                if (instruction.destination) operands.emplace_back(destination);
                for (std::size_t i = 0; i < instruction.operands.size(); ++i) {
                    operands.push_back(render(instruction.operands[i], i));
                }
                out << "    " << instruction.opcode;
                for (std::size_t i = 0; i < operands.size(); ++i) out << (i == 0 ? " " : ", ") << operands[i];
                out << '\n';
            }
            if (spilled) stackAccess("sw", destination, frame.offset(*spilled));
        }

    public:
        FunctionPrinter(std::ostream& out, const Function& function, bool isMain)
            : out(out), frame(function, isMain) {}

        void print(const Function& function, bool isMain) {
            out << function.label.name << ":\n";
            addStackOffset("$sp", -frame.size);
            const bool saveRa = !isMain && function.savesReturnAddress;
            if (saveRa) stackAccess("sw", "$ra", frame.returnAddress);

            for (const auto& statement : function.body) {
                if (const auto* label = std::get_if<Label>(&statement)) out << label->name << ":\n";
                else print(std::get<Instruction>(statement));
            }
            if (saveRa) stackAccess("lw", "$ra", frame.returnAddress);
            addStackOffset("$sp", frame.size);
            if (isMain) out << "    li $v0, 10\n    syscall\n";
            else out << "    jr $ra\n";
            out << '\n';
        }
    };

    static void printGlobal(std::ostream& out, const GlobalData& global) {
        if (global.isWord) out << ".align 2\n";
        out << global.label.name << ":\n";
        if (std::all_of(global.values.begin(), global.values.end(), [](auto value) { return value == 0; })) {
            out << "    .space " << global.values.size() * (global.isWord ? 4 : 1) << '\n';
            return;
        }
        for (std::size_t i = 0; i < global.values.size(); ++i) {
            const auto value = global.values[i];
            const std::int64_t signedWord = value < 0x80000000u ? value : std::int64_t(value) - 0x100000000LL;
            out << (i % 16 == 0 ? (global.isWord ? "    .word " : "    .byte ") : ", ");
            out << (global.isWord ? signedWord : value & 255u);
            if (i % 16 == 15 || i + 1 == global.values.size()) out << '\n';
        }
    }

public:
    void print(const Program& program, std::ostream& out) const {
        if (!program.globals.empty()) {
            out << ".data\n";
            for (const auto& global : program.globals) printGlobal(out, global);
            out << '\n';
        }
        out << ".text\n.globl main\n";
        for (const auto& function : program.functions) {
            const bool isMain = function.label.name == "main";
            FunctionPrinter(out, function, isMain).print(function, isMain);
        }
    }
};

} // namespace mips
