#pragma once

#include "../ir/Instructions.hpp"

namespace ir {
class IRBuilder {
private:
    IRContext& context;
    BasicBlock* block = nullptr;

    // template 让不同指令共用插入逻辑；unique_ptr 转交所有权后，返回非拥有指针。
    template <typename T, typename... Args>
    T* insert(Args&&... args) {
        if (block == nullptr) throw std::logic_error("No insertion block");
        auto instruction = std::make_unique<T>(std::forward<Args>(args)...);
        T* result = instruction.get();
        block->append(std::move(instruction));
        return result;
    }
public:
    explicit IRBuilder(IRContext& context) : context(context) {}
    void setBlock(BasicBlock* block) { this->block = block; }
    BasicBlock* getBlock() const { return block; }
    bool isTerminated() const { return block == nullptr || block->getTerminator() != nullptr; }
    AllocaInst* createAlloca(const Type* type) {
        auto instruction = std::make_unique<AllocaInst>(type);
        auto* result = instruction.get();
        block->getParent()->getEntry()->prependAlloca(std::move(instruction));
        return result;
    }
    Value* createLoad(Value* address) { return insert<LoadInst>(address); }
    void createStore(Value* value, Value* address) { insert<StoreInst>(value, address); }
    Value* createBinary(Opcode opcode, Value* left, Value* right) {
        return insert<BinaryInst>(opcode, left, right);
    }
    Value* createCompare(const std::string& predicate, Value* left, Value* right) {
        return insert<ICmpInst>(predicate, left, right);
    }
    Value* createCast(Value* value, const Type* type) {
        if (value->getType() == type) return value;
        if (auto* constant = dynamic_cast<ConstantInt*>(value)) {
            return context.getInt(type, constant->getBits());
        }
        Opcode opcode = value->getType()->getBitWidth() < type->getBitWidth() ? Opcode::ZExt : Opcode::Trunc;
        return insert<CastInst>(opcode, value, type);
    }
    Value* createGep(Value* address, Value* index) { return insert<GetElementPtrInst>(address, index); }
    Value* createCall(Function* function, const std::vector<Value*>& arguments) {
        return insert<CallInst>(function, arguments);
    }
    void createBranch(BasicBlock* target) { insert<BranchInst>(target); }
    void createBranch(Value* condition, BasicBlock* onTrue, BasicBlock* onFalse) {
        insert<BranchInst>(condition, onTrue, onFalse);
    }
    void createReturn(Value* value = nullptr) { insert<ReturnInst>(context, value); }
    PhiInst* createPhi(const Type* type) { return insert<PhiInst>(type); }
};
} // namespace ir
