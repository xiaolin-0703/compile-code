#pragma once

#include "Module.hpp"

namespace ir {
class AllocaInst : public Instruction {
public:
    explicit AllocaInst(const Type* allocatedType)
        : Instruction(allocatedType->getContext().getPointerType(allocatedType), Opcode::Alloca) {}
    const Type* getAllocatedType() const { return getType()->getElementType(); }
};

class LoadInst : public Instruction {
public:
    explicit LoadInst(Value* address)
        : Instruction(address->getType()->getElementType(), Opcode::Load) { addOperand(address); }
};

class StoreInst : public Instruction {
public:
    StoreInst(Value* value, Value* address)
        : Instruction(address->getType()->getContext().getVoidType(), Opcode::Store) {
        if (value->getType() != address->getType()->getElementType()) {
            throw std::logic_error("Store type mismatch");
        }
        addOperand(value);
        addOperand(address);
    }
};

class BinaryInst : public Instruction {
public:
    BinaryInst(Opcode opcode, Value* left, Value* right) : Instruction(left->getType(), opcode) {
        if (!left->getType()->isInteger() || left->getType() != right->getType()) {
            throw std::logic_error("Binary operand type mismatch");
        }
        addOperand(left);
        addOperand(right);
    }
};

class ICmpInst : public Instruction {
private:
    std::string predicate;
public:
    ICmpInst(std::string predicate, Value* left, Value* right)
        : Instruction(left->getType()->getContext().getInt1Type(), Opcode::ICmp),
          predicate(std::move(predicate)) {
        if (!left->getType()->isInteger() || left->getType() != right->getType()) {
            throw std::logic_error("Compare operand type mismatch");
        }
        addOperand(left);
        addOperand(right);
    }
    const std::string& getPredicate() const { return predicate; }
};

class CastInst : public Instruction {
public:
    CastInst(Opcode opcode, Value* value, const Type* type) : Instruction(type, opcode) {
        if (!value->getType()->isInteger() || !type->isInteger()) {
            throw std::logic_error("Cast requires integer types");
        }
        if ((opcode == Opcode::ZExt && value->getType()->getBitWidth() >= type->getBitWidth()) ||
            (opcode == Opcode::Trunc && value->getType()->getBitWidth() <= type->getBitWidth())) {
            throw std::logic_error("Invalid cast width");
        }
        addOperand(value);
    }
};

class GetElementPtrInst : public Instruction {
public:
    GetElementPtrInst(Value* address, Value* index)
        : Instruction(address->getType()->getContext().getPointerType(
              address->getType()->getElementType()->isArray()
                  ? address->getType()->getElementType()->getElementType()
                  : address->getType()->getElementType()), Opcode::GetElementPtr) {
        if (!index->getType()->isInteger(32)) throw std::logic_error("GEP index must be i32");
        addOperand(address);
        addOperand(index);
    }
};

class CallInst : public Instruction {
public:
    CallInst(Function* function, const std::vector<Value*>& arguments)
        : Instruction(function->getReturnType(), Opcode::Call) {
        const auto& types = function->getType()->getParamTypes();
        if (types.size() != arguments.size()) throw std::logic_error("Call argument count mismatch");
        addOperand(function);
        for (std::size_t i = 0; i < arguments.size(); ++i) {
            if (types[i] != arguments[i]->getType()) throw std::logic_error("Call argument type mismatch");
            addOperand(arguments[i]);
        }
    }
    Function* getCallee() const { return static_cast<Function*>(getOperand(0)); }
};

class BranchInst : public Instruction {
public:
    explicit BranchInst(BasicBlock* target)
        : Instruction(target->getType()->getContext().getVoidType(), Opcode::Branch) { addOperand(target); }
    BranchInst(Value* condition, BasicBlock* onTrue, BasicBlock* onFalse)
        : Instruction(condition->getType()->getContext().getVoidType(), Opcode::Branch) {
        if (!condition->getType()->isInteger(1)) throw std::logic_error("Branch condition must be i1");
        addOperand(condition);
        addOperand(onTrue);
        addOperand(onFalse);
    }
    bool isConditional() const { return getOperandCount() == 3; }
};

class ReturnInst : public Instruction {
public:
    ReturnInst(IRContext& context, Value* value = nullptr)
        : Instruction(context.getVoidType(), Opcode::Return) {
        if (value != nullptr) addOperand(value);
    }
};

class PhiInst : public Instruction {
public:
    explicit PhiInst(const Type* type) : Instruction(type, Opcode::Phi) {}
    void addIncoming(Value* value, BasicBlock* predecessor) {
        if (value->getType() != getType()) throw std::logic_error("Phi type mismatch");
        addOperand(value);
        addOperand(predecessor);
    }
};
} // namespace ir
