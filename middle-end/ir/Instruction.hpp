#pragma once

#include "User.hpp"

namespace ir {
class BasicBlock;

// enum class 是有作用域的枚举，使用时写 Opcode::Add，避免名字冲突。
enum class Opcode {
    Alloca, Load, Store, Add, Sub, Mul, SDiv, UDiv, SRem, URem,
    ICmp, ZExt, Trunc, GetElementPtr, Call, Branch, Return, Phi
};

// 指令既能引用操作数，也能作为下一条指令的操作数。
class Instruction : public User {
private:
    Opcode opcode;
    BasicBlock* parent = nullptr; // 非拥有指针，基本块负责管理指令生命期。

public:
    Instruction(const Type* type, Opcode opcode) : User(type), opcode(opcode) {}
    Opcode getOpcode() const { return opcode; }
    BasicBlock* getParent() const { return parent; }
    void setParent(BasicBlock* block) { parent = block; }
    bool isTerminator() const { return opcode == Opcode::Branch || opcode == Opcode::Return; }
    bool hasSideEffects() const {
        return opcode == Opcode::Store || opcode == Opcode::Call || isTerminator();
    }
    bool readsMemory() const { return opcode == Opcode::Load || opcode == Opcode::Call; }
    bool writesMemory() const { return opcode == Opcode::Store || opcode == Opcode::Call; }
};
} // namespace ir
