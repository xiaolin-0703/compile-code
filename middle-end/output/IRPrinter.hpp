#pragma once

#include "../ir/Instructions.hpp"
#include <ostream>
#include <unordered_map>

namespace ir {
// 打印器只读取 IR；最终编号在这里分配，优化器不依赖打印名称。
class IRPrinter {
private:
    std::unordered_map<const Value*, std::string> names;

    std::string reference(const Value* value) const {
        if (const auto* constant = dynamic_cast<const ConstantInt*>(value)) {
            return std::to_string(constant->getSignedValue());
        }
        return names.at(value);
    }
    std::string typed(const Value* value) const {
        return value->getType()->toString() + " " + reference(value);
    }
    std::string binaryName(Opcode opcode) const {
        switch (opcode) {
            case Opcode::Add: return "add";
            case Opcode::Sub: return "sub";
            case Opcode::Mul: return "mul";
            case Opcode::SDiv: return "sdiv";
            case Opcode::UDiv: return "udiv";
            case Opcode::SRem: return "srem";
            case Opcode::URem: return "urem";
            default: throw std::logic_error("Not a binary instruction");
        }
    }
    void printInstruction(const Instruction& instruction, std::ostream& out) const {
        out << "  ";
        if (!instruction.getType()->isVoid()) out << reference(&instruction) << " = ";
        auto operand = [&](std::size_t index) { return instruction.getOperand(index); };
        switch (instruction.getOpcode()) {
            case Opcode::Alloca:
                out << "alloca " << instruction.getType()->getElementType()->toString();
                break;
            case Opcode::Load:
                out << "load " << instruction.getType()->toString() << ", " << typed(operand(0));
                break;
            case Opcode::Store:
                out << "store " << typed(operand(0)) << ", " << typed(operand(1));
                break;
            case Opcode::Add: case Opcode::Sub: case Opcode::Mul:
            case Opcode::SDiv: case Opcode::UDiv: case Opcode::SRem: case Opcode::URem:
                out << binaryName(instruction.getOpcode()) << ' ' << typed(operand(0))
                    << ", " << reference(operand(1));
                break;
            case Opcode::ICmp:
                out << "icmp " << static_cast<const ICmpInst&>(instruction).getPredicate()
                    << ' ' << typed(operand(0)) << ", " << reference(operand(1));
                break;
            case Opcode::ZExt: case Opcode::Trunc:
                out << (instruction.getOpcode() == Opcode::ZExt ? "zext " : "trunc ")
                    << typed(operand(0)) << " to " << instruction.getType()->toString();
                break;
            case Opcode::GetElementPtr: {
                const Type* baseType = operand(0)->getType()->getElementType();
                out << "getelementptr " << baseType->toString() << ", " << typed(operand(0));
                // 数组对象先选中整个数组(0)，再选中元素；数组形参只有元素下标。
                if (baseType->isArray()) out << ", i32 0";
                out << ", " << typed(operand(1));
                break;
            }
            case Opcode::Call: {
                const auto* function = static_cast<const CallInst&>(instruction).getCallee();
                out << "call ";
                // 课程 libsysy.ll 的 char ABI 使用 signext，内部 char 值仍按无符号处理。
                if (function->getName() == "get_char") out << "signext ";
                out << function->getReturnType()->toString() << ' ' << reference(function) << '(';
                for (std::size_t i = 1; i < instruction.getOperandCount(); ++i) {
                    if (i != 1) out << ", ";
                    out << operand(i)->getType()->toString() << ' ';
                    if (function->getName() == "put_char") out << "signext ";
                    out << reference(operand(i));
                }
                out << ')';
                break;
            }
            case Opcode::Branch:
                if (instruction.getOperandCount() == 1) {
                    out << "br label " << reference(operand(0));
                } else {
                    out << "br " << typed(operand(0)) << ", label " << reference(operand(1))
                        << ", label " << reference(operand(2));
                }
                break;
            case Opcode::Return:
                out << "ret " << (instruction.getOperandCount() == 0 ? "void" : typed(operand(0)));
                break;
            case Opcode::Phi:
                out << "phi " << instruction.getType()->toString() << ' ';
                for (std::size_t i = 0; i < instruction.getOperandCount(); i += 2) {
                    if (i != 0) out << ", ";
                    out << "[ " << reference(operand(i)) << ", " << reference(operand(i + 1)) << " ]";
                }
                break;
        }
        out << '\n';
    }
public:
    void print(const Module& module, std::ostream& out) {
        names.clear();
        out << "; Generated SysY LLVM IR\n\n";
        for (const auto& global : module.getGlobals()) names[global.get()] = "@" + global->getName();
        for (const auto& function : module.getFunctions()) names[function.get()] = "@" + function->getName();
        for (const auto& global : module.getGlobals()) {
            const Type* type = global->getStoredType();
            const Type* element = type->isArray() ? type->getElementType() : type;
            const auto& values = global->getInitialValues();
            out << reference(global.get()) << " = " << (global->isConstant() ? "constant " : "global ")
                << type->toString() << ' ';
            if (std::all_of(values.begin(), values.end(), [](auto value) { return value == 0; })) {
                out << "zeroinitializer";
            } else if (!type->isArray()) {
                out << reference(module.getContext().getInt(element, values.at(0)));
            } else {
                out << '[';
                for (std::size_t i = 0; i < values.size(); ++i) {
                    if (i != 0) out << ", ";
                    out << element->toString() << ' '
                        << reference(module.getContext().getInt(element, values[i]));
                }
                out << ']';
            }
            out << '\n';
        }
        out << '\n';
        for (const auto& function : module.getFunctions()) {
            std::size_t valueNumber = 0, blockNumber = 0;
            for (const auto& argument : function->getArguments()) {
                names[argument.get()] = "%v" + std::to_string(valueNumber++);
            }
            for (const auto& block : function->getBlocks()) {
                names[block.get()] = "%bb" + std::to_string(blockNumber++);
                for (const auto& instruction : block->getInstructions()) {
                    if (!instruction->getType()->isVoid()) names[instruction.get()] = "%v" + std::to_string(valueNumber++);
                }
            }
            bool declaration = function->isDeclaration();
            out << (declaration ? "declare " : "define ");
            if (function->getName() == "get_char") out << "signext ";
            out << function->getReturnType()->toString() << ' ' << reference(function.get()) << '(';
            const auto& arguments = function->getArguments();
            for (std::size_t i = 0; i < arguments.size(); ++i) {
                if (i != 0) out << ", ";
                out << arguments[i]->getType()->toString();
                if (function->getName() == "put_char") out << " signext";
                if (!declaration) out << ' ' << reference(arguments[i].get());
            }
            out << ')';
            if (declaration) { out << "\n\n"; continue; }
            out << " {\n";
            for (const auto& block : function->getBlocks()) {
                out << reference(block.get()).substr(1) << ":\n";
                for (const auto& instruction : block->getInstructions()) printInstruction(*instruction, out);
            }
            out << "}\n\n";
        }
    }
};
} // namespace ir
