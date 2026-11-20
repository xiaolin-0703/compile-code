#pragma once

#include "../machine-ir/MachineIR.hpp"
#include "../../middle-end/ir/Instructions.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace mips {

class MipsLowering {
    const ir::Module& module;
    const bool optimize;
    std::unordered_map<const ir::GlobalVariable*, Label> globalLabels;
    std::unordered_map<const ir::Function*, Label> functionLabels;
    std::unordered_map<const ir::BasicBlock*, Label> blockLabels;
    std::unordered_map<const ir::Value*, Value> values;
    Function function;
    std::size_t nextLabel = 0;

    static std::int64_t alignment(const ir::Type* type) {
        if (type->isArray()) return alignment(type->getElementType());
        if (type->isInteger(1) || type->isInteger(8)) return 1;
        if (type->isInteger(32) || type->isPointer()) return 4;
        throw std::logic_error("Unsupported MIPS storage type: " + type->toString());
    }

    static std::int64_t sizeOf(const ir::Type* type) {
        if (!type->isArray()) return alignment(type);
        const auto elementSize = sizeOf(type->getElementType());
        if (elementSize == 0) return 0;
        if (type->getArrayLength() > std::numeric_limits<std::int32_t>::max() / std::uint64_t(elementSize)) {
            throw std::logic_error("MIPS array exceeds the address range");
        }
        return static_cast<std::int64_t>(type->getArrayLength()) * elementSize;
    }

    static bool isByteAccess(const ir::Type* type) {
        if (!type->isScalar()) throw std::logic_error("MIPS load/store requires a scalar");
        return sizeOf(type) == 1;
    }

    VirtualRegister newRegister() { return {function.virtualRegisters++}; }

    Label localLabel() {
        return {function.label.name + "_local_" + std::to_string(nextLabel++)};
    }

    void mark(const Label& label) { function.body.emplace_back(label); }

    void emit(std::string_view opcode, std::vector<Operand> operands = {}) {
        function.body.emplace_back(Instruction{opcode, std::nullopt, std::move(operands)});
    }

    void define(std::string_view opcode, Destination destination, std::vector<Operand> operands) {
        function.body.emplace_back(Instruction{opcode, destination, std::move(operands)});
    }

    Value read(const ir::Value* value) const {
        if (const auto* integer = dynamic_cast<const ir::ConstantInt*>(value)) {
            return integer->getType()->isInteger(32)
                ? integer->getSignedValue() : std::int64_t(integer->getBits());
        }
        if (const auto* global = dynamic_cast<const ir::GlobalVariable*>(value)) {
            return globalLabels.at(global);
        }
        const auto found = values.find(value);
        if (found != values.end()) return found->second;
        if (optimize) {
            const auto* cast = dynamic_cast<const ir::Instruction*>(value);
            if (cast && cast->getOpcode() == ir::Opcode::ZExt) {
                // The source already holds a zero-extended i1 or i8.
                return read(cast->getOperand(0));
            }
        }
        throw std::logic_error("Missing MIPS operand");
    }

    VirtualRegister result(const ir::Instruction& instruction) const {
        return std::get<VirtualRegister>(values.at(&instruction));
    }

    static std::int64_t mask(const ir::Type* type) {
        return (std::int64_t{1} << type->getBitWidth()) - 1;
    }

    Value signExtend(Value value, const ir::Type* type) {
        if (type->isInteger(32)) return value;
        const std::int64_t shift = 32 - type->getBitWidth();
        const auto extended = newRegister();
        define("sll", extended, {value, shift});
        define("sra", extended, {Value(extended), shift});
        return extended;
    }

    void lowerBinary(const ir::Instruction& instruction) {
        using ir::Opcode;
        const Opcode opcode = instruction.getOpcode();
        const auto* type = instruction.getType();
        const auto destination = result(instruction);
        auto left = read(instruction.getOperand(0));
        auto right = read(instruction.getOperand(1));

        switch (opcode) {
            case Opcode::Add: case Opcode::Sub: {
                if (optimize) {
                    if (opcode == Opcode::Add && std::holds_alternative<std::int64_t>(left)) {
                        std::swap(left, right);
                    }
                    if (const auto* constant = std::get_if<std::int64_t>(&right)) {
                        const auto immediate = opcode == Opcode::Add ? *constant : -*constant;
                        if (fitsSignedImmediate(immediate)) {
                            define("addiu", destination, {left, immediate});
                            break;
                        }
                    }
                }
                define(opcode == Opcode::Add ? "addu" : "subu", destination, {left, right});
                break;
            }
            case Opcode::Mul: define("mul", destination, {left, right}); break;
            case Opcode::SDiv: case Opcode::SRem: case Opcode::UDiv: case Opcode::URem: {
                const bool isSigned = opcode == Opcode::SDiv || opcode == Opcode::SRem;
                if (isSigned) {
                    left = signExtend(left, type);
                    right = signExtend(right, type);
                }
                emit(isSigned ? "div" : "divu", {left, right});
                define(opcode == Opcode::SDiv || opcode == Opcode::UDiv ? "mflo" : "mfhi", destination, {});
                break;
            }
            default: throw std::logic_error("Expected a binary IR instruction");
        }
        // Registers always hold i1/i8 in zero-extended form.
        if (!type->isInteger(32)) define("andi", destination, {Value(destination), mask(type)});
    }

    void lowerCompare(const ir::ICmpInst& instruction) {
        const auto& predicate = instruction.getPredicate();
        const auto destination = result(instruction);
        auto left = read(instruction.getOperand(0));
        auto right = read(instruction.getOperand(1));
        if (predicate == "eq" || predicate == "ne") {
            define(predicate == "eq" ? "seq" : "sne", destination, {left, right});
            return;
        }
        if (predicate.size() != 3 || (predicate[0] != 's' && predicate[0] != 'u')) {
            throw std::logic_error("Unsupported MIPS comparison: " + predicate);
        }
        const bool isSigned = predicate.front() == 's';
        const auto relation = predicate.substr(1);
        if (relation != "lt" && relation != "le" && relation != "gt" && relation != "ge") {
            throw std::logic_error("Unsupported MIPS comparison: " + predicate);
        }
        if (isSigned) {
            const auto* type = instruction.getOperand(0)->getType();
            left = signExtend(left, type);
            right = signExtend(right, type);
        }
        if (relation == "gt" || relation == "le") std::swap(left, right);
        define(isSigned ? "slt" : "sltu", destination, {left, right});
        if (relation == "le" || relation == "ge") {
            define("xori", destination, {Value(destination), std::int64_t{1}});
        }
    }

    void lowerGep(const ir::Instruction& instruction) {
        const auto base = read(instruction.getOperand(0));
        auto index = read(instruction.getOperand(1));
        const auto stride = sizeOf(instruction.getType()->getElementType());
        const auto destination = result(instruction);
        if (const auto* offset = std::get_if<std::int64_t>(&index)) {
            const auto bytes = *offset * stride;
            if (bytes == 0) {
                define("move", destination, {base});
                return;
            }
            if (optimize && fitsSignedImmediate(bytes)) {
                define("addiu", destination, {base, bytes});
                return;
            }
        }
        if (stride != 1) {
            const auto scaled = newRegister();
            if (stride == 4) define("sll", scaled, {index, std::int64_t{2}});
            else define("mul", scaled, {index, Value(stride)});
            index = scaled;
        }
        define("addu", destination, {base, index});
    }

    void lowerCall(const ir::CallInst& instruction) {
        const auto* callee = instruction.getCallee();
        const auto count = callee->getArguments().size();
        const auto registers = std::min(count, argumentRegisters.size());
        for (std::size_t i = 0; i < registers; ++i) {
            define("move", argumentRegisters[i], {read(instruction.getOperand(i + 1))});
        }
        function.outgoingWords = std::max(function.outgoingWords, count - registers);
        for (std::size_t i = registers; i < count; ++i) {
            emit("sw", {read(instruction.getOperand(i + 1)), Memory{StackAddress{StackArea::Outgoing, i - registers}}});
        }

        if (!callee->isDeclaration()) {
            function.savesReturnAddress = true;
            emit("jal", {functionLabels.at(callee)});
        } else {
            const auto& name = callee->getName();
            std::int64_t syscall;
            if (name == "put_int") syscall = 1;
            else if (name == "put_string") syscall = 4;
            else if (name == "get_int") syscall = 5;
            else if (name == "get_string") syscall = 8;
            else if (name == "put_char") syscall = 11;
            else if (name == "get_char") syscall = 12;
            else throw std::logic_error("Unknown MIPS runtime function: " + name);

            const auto done = syscall == 8 ? std::optional<Label>(localLabel()) : std::nullopt;
            if (done) emit("blez", {Value(argumentRegisters[1]), *done});
            define("move", returnRegister, {Value(syscall)});
            emit("syscall");
            if (done) mark(*done);
        }
        if (instruction.getType()->isVoid()) return;
        if (instruction.getType()->isInteger(8)) {
            define("andi", result(instruction), {Value(returnRegister), mask(instruction.getType())});
        } else {
            define("move", result(instruction), {Value(returnRegister)});
        }
    }

    void emitEdge(const ir::BasicBlock* from, const ir::BasicBlock* to) {
        // Read all incoming values before overwriting any phi destination on this edge.
        std::vector<std::pair<VirtualRegister, VirtualRegister>> copies;
        for (const auto& phi : to->getInstructions()) {
            if (phi->getOpcode() != ir::Opcode::Phi) break;
            for (std::size_t i = 0; i < phi->getOperandCount(); i += 2) {
                if (phi->getOperand(i + 1) == from) {
                    const auto temporary = newRegister();
                    define("move", temporary, {read(phi->getOperand(i))});
                    copies.emplace_back(result(*phi), temporary);
                    break;
                }
            }
        }
        for (const auto& [destination, source] : copies) define("move", destination, {Value(source)});
        emit("j", {blockLabels.at(to)});
    }

    void lowerBranch(const ir::BranchInst& branch) {
        if (!branch.isConditional()) {
            emitEdge(branch.getParent(), static_cast<const ir::BasicBlock*>(branch.getOperand(0)));
            return;
        }
        const auto* onTrue = static_cast<const ir::BasicBlock*>(branch.getOperand(1));
        const auto* onFalse = static_cast<const ir::BasicBlock*>(branch.getOperand(2));
        const bool trueHasPhi = !onTrue->getInstructions().empty()
            && onTrue->getInstructions().front()->getOpcode() == ir::Opcode::Phi;
        const auto trueEdge = trueHasPhi ? localLabel() : blockLabels.at(onTrue);
        const auto falseEdge = localLabel();
        // Only the nearby dispatch label needs to fit a conditional branch offset.
        emit("beq", {read(branch.getOperand(0)), Value(zeroRegister), falseEdge});
        emit("j", {trueEdge});
        mark(falseEdge);
        emitEdge(branch.getParent(), onFalse);
        if (trueHasPhi) {
            mark(trueEdge);
            emitEdge(branch.getParent(), onTrue);
        }
    }

    void lowerInstruction(const ir::Instruction& instruction) {
        using ir::Opcode;
        const auto* type = instruction.getType();
        switch (instruction.getOpcode()) {
            case Opcode::Alloca: case Opcode::Phi: break;
            case Opcode::Load: {
                define(isByteAccess(type) ? "lbu" : "lw", result(instruction),
                    {Memory{read(instruction.getOperand(0))}});
                break;
            }
            case Opcode::Store: {
                auto* source = instruction.getOperand(0);
                emit(isByteAccess(source->getType()) ? "sb" : "sw",
                    {read(source), Memory{read(instruction.getOperand(1))}});
                break;
            }
            case Opcode::Add: case Opcode::Sub: case Opcode::Mul:
            case Opcode::SDiv: case Opcode::UDiv: case Opcode::SRem: case Opcode::URem:
                lowerBinary(instruction);
                break;
            case Opcode::ICmp: lowerCompare(static_cast<const ir::ICmpInst&>(instruction)); break;
            case Opcode::Trunc:
                define("andi", result(instruction), {read(instruction.getOperand(0)), mask(type)});
                break;
            case Opcode::ZExt:
                if (!optimize) define("move", result(instruction), {read(instruction.getOperand(0))});
                break;
            case Opcode::GetElementPtr: lowerGep(instruction); break;
            case Opcode::Call: lowerCall(static_cast<const ir::CallInst&>(instruction)); break;
            case Opcode::Branch: lowerBranch(static_cast<const ir::BranchInst&>(instruction)); break;
            case Opcode::Return:
                if (instruction.getOperandCount() != 0 && function.label.name != "main") {
                    define("move", returnRegister, {read(instruction.getOperand(0))});
                }
                emit("j", {function.exit});
                break;
        }
    }

    Function lowerFunction(const ir::Function& source) {
        function = Function{};
        function.label = functionLabels.at(&source);
        function.exit = {function.label.name + "_return"};
        nextLabel = 0;
        blockLabels.clear();
        values.clear();
        for (std::size_t i = 0; i < source.getBlocks().size(); ++i) {
            const auto* block = source.getBlocks()[i].get();
            blockLabels.emplace(block, Label{function.label.name + "_bb_" + std::to_string(i)});
        }
        for (std::size_t i = 0; i < source.getArguments().size(); ++i) {
            const auto destination = newRegister();
            values.emplace(source.getArguments()[i].get(), destination);
            if (i < argumentRegisters.size()) define("move", destination, {Value(argumentRegisters[i])});
            else define("lw", destination, {Memory{StackAddress{StackArea::Incoming, i - argumentRegisters.size()}}});
        }
        for (const auto& block : source.getBlocks()) {
            for (const auto& instruction : block->getInstructions()) {
                if (const auto* alloca = dynamic_cast<const ir::AllocaInst*>(instruction.get())) {
                    const auto* type = alloca->getAllocatedType();
                    values.emplace(alloca, StackAddress{StackArea::Local, function.locals.size()});
                    function.locals.push_back({sizeOf(type), alignment(type)});
                } else if (!instruction->getType()->isVoid()
                           && (!optimize || instruction->getOpcode() != ir::Opcode::ZExt)) {
                    values.emplace(instruction.get(), newRegister());
                }
            }
        }
        for (const auto& block : source.getBlocks()) {
            mark(blockLabels.at(block.get()));
            for (const auto& instruction : block->getInstructions()) lowerInstruction(*instruction);
        }
        mark(function.exit);
        return std::move(function);
    }

public:
    explicit MipsLowering(const ir::Module& module, bool optimize = true)
        : module(module), optimize(optimize) {}

    Program lower() {
        Program program;
        for (const auto& global : module.getGlobals()) {
            Label label{"data_" + std::to_string(program.globals.size())};
            globalLabels.emplace(global.get(), label);
            program.globals.push_back({label, alignment(global->getStoredType()) == 4, global->getInitialValues()});
        }
        const auto* main = module.getFunction("main");
        functionLabels.emplace(main, Label{"main"});
        for (const auto& source : module.getFunctions()) {
            if (source.get() == main || source->isDeclaration()) continue;
            functionLabels.emplace(source.get(), Label{"fn_" + std::to_string(functionLabels.size())});
        }
        program.functions.push_back(lowerFunction(*main));
        for (const auto& source : module.getFunctions()) {
            if (source.get() != main && !source->isDeclaration()) program.functions.push_back(lowerFunction(*source));
        }
        return program;
    }
};

} // namespace mips
