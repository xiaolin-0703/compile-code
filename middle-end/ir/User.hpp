#pragma once

#include "Use.hpp"
#include "Value.hpp"

#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ir {

// User 是“持有操作数的 Value”；后续 Instruction 继承它。
class User : public Value {
public:
    ~User() override { dropAllReferences(); }

    std::size_t getOperandCount() const { return operands.size(); }
    Value* getOperand(std::size_t index) const {
        return operands.at(index)->get();
    }

    void setOperand(std::size_t index, Value* value) {
        Use* use = operands.at(index).get();
        validateOperand(value);
        use->set(value);
    }

    void dropAllReferences() noexcept {
        // Use 的析构函数负责从 Value 的使用列表注销。
        operands.clear();
    }

protected: // protected 允许派生的 Instruction 创建操作数，但不向外部公开。
    explicit User(const Type* type, std::string name = {})
        : Value(type, std::move(name)) {}

    void addOperand(Value* value) {
        validateOperand(value);
        auto use = std::unique_ptr<Use>(new Use(this, operands.size(), value));
        operands.push_back(std::move(use));
    }

    void eraseOperand(std::size_t index) {
        if (index >= operands.size()) {
            throw std::out_of_range("Operand index out of range");
        }
        using Difference = decltype(operands)::difference_type;
        operands.erase(operands.begin() + static_cast<Difference>(index));
        for (std::size_t i = index; i < operands.size(); ++i) {
            operands[i]->operandIndex = i;
        }
    }

private:
    void validateOperand(Value* value) const {
        if (value == nullptr || value->getType()->isVoid()) {
            throw std::invalid_argument("Operand must be a non-void Value");
        }
        if (&value->getType()->getContext() != &getType()->getContext()) {
            throw std::invalid_argument("Operand must belong to the same IRContext");
        }
        // 具体指令还需要检查操作数类型和数量。
    }

    // unique_ptr 保证 vector 扩容时 Use 本身的地址不变。
    std::vector<std::unique_ptr<Use>> operands;
};

} // namespace ir
