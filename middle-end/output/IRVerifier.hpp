#pragma once

#include "../analysis/CFG.hpp"

namespace ir {
// 生成后检查图结构和 SSA 支配关系。throw 抛出内部错误，由编译入口统一报告。
class IRVerifier {
private:
    static void require(bool condition, const char* message) {
        if (!condition) throw std::logic_error(message);
    }
    void verifyTypes(const Instruction& instruction) const {
        const Type* type = instruction.getType();
        std::size_t count = instruction.getOperandCount();
        auto operandType = [&](std::size_t index) { return instruction.getOperand(index)->getType(); };
        switch (instruction.getOpcode()) {
            case Opcode::Alloca:
                require(count == 0 && type->isPointer() && type->getElementType()->isStorable(), "Invalid alloca");
                break;
            case Opcode::Load:
                require(count == 1 && operandType(0)->isPointer()
                    && type == operandType(0)->getElementType(), "Invalid load");
                break;
            case Opcode::Store:
                require(count == 2 && type->isVoid() && operandType(1)->isPointer()
                    && operandType(0) == operandType(1)->getElementType(), "Invalid store");
                break;
            case Opcode::Add: case Opcode::Sub: case Opcode::Mul:
            case Opcode::SDiv: case Opcode::UDiv: case Opcode::SRem: case Opcode::URem:
                require(count == 2 && type->isInteger() && operandType(0) == type && operandType(1) == type,
                        "Invalid binary instruction");
                break;
            case Opcode::ICmp:
                require(dynamic_cast<const ICmpInst*>(&instruction) != nullptr && count == 2
                    && type->isInteger(1) && operandType(0)->isInteger() && operandType(0) == operandType(1), "Invalid compare");
                break;
            case Opcode::ZExt: case Opcode::Trunc:
                require(count == 1 && type->isInteger() && operandType(0)->isInteger(), "Invalid cast");
                require(instruction.getOpcode() == Opcode::ZExt
                    ? operandType(0)->getBitWidth() < type->getBitWidth()
                    : operandType(0)->getBitWidth() > type->getBitWidth(), "Invalid cast width");
                break;
            case Opcode::GetElementPtr: {
                require(count == 2 && type->isPointer() && operandType(0)->isPointer()
                    && operandType(1)->isInteger(32), "Invalid GEP");
                const Type* base = operandType(0)->getElementType();
                require(type->getElementType() == (base->isArray() ? base->getElementType() : base), "Invalid GEP result");
                break;
            }
            case Opcode::Call: {
                require(dynamic_cast<const CallInst*>(&instruction) != nullptr && count > 0, "Invalid call");
                const auto* callee = dynamic_cast<const Function*>(instruction.getOperand(0));
                require(callee != nullptr && callee->getReturnType() == type, "Invalid callee");
                const auto& parameters = callee->getType()->getParamTypes();
                require(parameters.size() + 1 == count, "Invalid call argument count");
                for (std::size_t i = 0; i < parameters.size(); ++i) {
                    require(operandType(i + 1) == parameters[i], "Invalid call argument type");
                }
                break;
            }
            case Opcode::Branch:
                require(dynamic_cast<const BranchInst*>(&instruction) != nullptr && type->isVoid()
                    && (count == 1 || count == 3), "Invalid branch");
                if (count == 3) require(operandType(0)->isInteger(1) && operandType(1)->isLabel(), "Invalid conditional branch");
                require(operandType(count - 1)->isLabel(), "Branch target must be a label");
                break;
            case Opcode::Return:
                require(type->isVoid() && count <= 1, "Invalid return");
                break;
            case Opcode::Phi:
                require(type->isScalar() && count > 0 && count % 2 == 0, "Invalid phi");
                for (std::size_t i = 0; i < count; i += 2) {
                    require(operandType(i) == type && operandType(i + 1)->isLabel(), "Invalid phi incoming type");
                }
                break;
        }
    }
public:
    void verify(const Module& module) const {
        for (const auto& global : module.getGlobals()) {
            const Type* type = global->getStoredType();
            require(global->getInitialValues().size() == (type->isArray() ? type->getArrayLength() : 1),
                    "Invalid global initializer length");
        }
        for (const auto& function : module.getFunctions()) {
            if (function->isDeclaration()) continue;
            CFG cfg(*function);
            require(cfg.predecessors[function->getEntry()].empty(), "Entry block has predecessors");
            std::unordered_map<const Instruction*, std::size_t> positions;
            for (const auto& block : function->getBlocks()) {
                require(block->getParent() == function.get(), "Invalid block parent");
                require(block->getTerminator() != nullptr, "Basic block has no terminator");
                bool ordinarySeen = false;
                for (std::size_t i = 0; i < block->getInstructions().size(); ++i) {
                    const auto* instruction = block->getInstructions()[i].get();
                    positions[instruction] = i;
                    verifyTypes(*instruction);
                    require(instruction->getParent() == block.get(), "Invalid instruction parent");
                    require(!instruction->isTerminator() || i + 1 == block->getInstructions().size(), "Instruction after terminator");
                    if (instruction->getOpcode() == Opcode::Phi) require(!ordinarySeen, "Phi must be first");
                    else ordinarySeen = true;
                }
            }
            for (const auto& block : function->getBlocks()) {
                for (const auto& instruction : block->getInstructions()) {
                    bool phi = instruction->getOpcode() == Opcode::Phi;
                    if (phi) {
                        std::unordered_set<const BasicBlock*> incoming;
                        require(instruction->getOperandCount() % 2 == 0, "Invalid phi operand pairs");
                        for (std::size_t i = 1; i < instruction->getOperandCount(); i += 2) {
                            auto* previous = dynamic_cast<BasicBlock*>(instruction->getOperand(i));
                            require(previous != nullptr && incoming.insert(previous).second, "Duplicate phi incoming block");
                        }
                        const auto& predecessors = cfg.predecessors[block.get()];
                        require(incoming == std::unordered_set<const BasicBlock*>(predecessors.begin(), predecessors.end()),
                                "Phi incoming blocks differ from CFG");
                    }
                    if (instruction->getOpcode() == Opcode::Return) {
                        require(function->getReturnType()->isVoid() ? instruction->getOperandCount() == 0
                            : instruction->getOperandCount() == 1 && instruction->getOperand(0)->getType() == function->getReturnType(),
                            "Return type mismatch");
                    }
                    for (std::size_t i = 0; i < instruction->getOperandCount(); ++i) {
                        Value* operand = instruction->getOperand(i);
                        require(&operand->getType()->getContext() == &module.getContext(), "Cross-context operand");
                        std::size_t matchingUses = 0;
                        for (const Use* use : operand->getUses()) {
                            require(use->getUser()->getOperand(use->getOperandIndex()) == operand, "Stale use");
                            if (use->getUser() == instruction.get() && use->getOperandIndex() == i) ++matchingUses;
                        }
                        require(matchingUses == 1, "Missing or duplicate use");
                        if (auto* argument = dynamic_cast<Argument*>(operand)) {
                            require(argument->getParent() == function.get(), "Cross-function argument");
                        }
                        if (auto* label = dynamic_cast<BasicBlock*>(operand)) {
                            require(label->getParent() == function.get(), "Cross-function block");
                        }
                        auto* definition = dynamic_cast<Instruction*>(operand);
                        if (definition == nullptr) continue;
                        require(definition->getParent() != nullptr && definition->getParent()->getParent() == function.get(),
                                "Cross-function instruction");
                        const BasicBlock* useBlock = block.get();
                        if (phi && i % 2 == 0) useBlock = static_cast<BasicBlock*>(instruction->getOperand(i + 1));
                        if (!cfg.reachable.count(useBlock)) continue;
                        require(cfg.dominators[useBlock].count(definition->getParent()) != 0, "Definition does not dominate use");
                        if (definition->getParent() == useBlock && !phi) {
                            require(positions.at(definition) < positions.at(instruction.get()), "Use precedes definition");
                        }
                    }
                }
            }
        }
    }
};
} // namespace ir
