#pragma once

#include "Type.hpp"

#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

namespace ir {

class Use;
class User;

// Value 表示“可被引用的 IR 对象”，不是源程序变量，也不是物理寄存器。
// 它不拥有使用者；常量、形参、指令结果等通过继承扩展。
class Value {
public:
    virtual ~Value() {
        // 批量销毁 IR 时，先断开所有 User 的引用，再释放定义。
        assert(uses.empty() && "Destroying a Value that still has users");
    }
    // = delete 禁止复制和移动，防止同一个 IR 定义出现不一致的使用关系。
    Value(const Value&) = delete;
    Value& operator=(const Value&) = delete;
    Value(Value&&) = delete;
    Value& operator=(Value&&) = delete;

    const Type* getType() const { return type; }
    const std::string& getName() const { return name; }
    void setName(std::string name) { this->name = std::move(name); }

    bool useEmpty() const { return uses.empty(); }
    std::size_t getUseCount() const { return uses.size(); }
    const std::unordered_set<Use*>& getUses() const { return uses; }

    // replacement 必须具有相同类型。支配关系由具体变换和 Verifier 检查。
    void replaceAllUsesWith(Value* replacement);

protected:
    explicit Value(const Type* type, std::string name = {})
        : type(type), name(std::move(name)) {
        if (type == nullptr) {
            throw std::invalid_argument("Value requires a non-null type");
        }
    }

private: // private 成员只能由本类及 friend 指定的类访问。
    friend class Use;
    void addUse(Use* use) { uses.insert(use); }
    void removeUse(Use* use) noexcept { uses.erase(use); }

    const Type* type;
    std::string name; // 可读名字提示；对象身份和最终编号不依赖它。
    std::unordered_set<Use*> uses;
};

// Value 和 Use 相互访问，定义放在同一头文件中，避免循环 include。
// 一个 Use 对应一个操作数位置。add x, x 对 x 有两个 Use。
class Use {
public:
    ~Use() { set(nullptr); }
    Use(const Use&) = delete;
    Use& operator=(const Use&) = delete;
    Use(Use&&) = delete;
    Use& operator=(Use&&) = delete;

    Value* get() const { return value; }
    User* getUser() const { return user; }
    std::size_t getOperandIndex() const { return operandIndex; }

private:
    friend class Value;
    friend class User;
    Use(User* user, std::size_t operandIndex, Value* value)
        : user(user), operandIndex(operandIndex) {
        set(value);
    }

    void set(Value* value) {
        if (value == this->value) return;
        // 先注册新引用。分配失败时，旧引用仍然有效。
        if (value != nullptr) value->addUse(this);
        if (this->value != nullptr) this->value->removeUse(this);
        this->value = value;
    }

    User* user;
    std::size_t operandIndex;
    Value* value = nullptr;
};

// 类外的头文件实现显式 inline，支持多个翻译单元同时包含。
inline void Value::replaceAllUsesWith(Value* replacement) {
    if (replacement == nullptr || replacement->getType() != type) {
        throw std::invalid_argument("Replacement must have the same IR type");
    }
    if (replacement == this) return;

    // Use::set 会更新使用列表，不能继续使用旧迭代器。
    while (!uses.empty()) {
        (*uses.begin())->set(replacement);
    }
}

} // namespace ir
