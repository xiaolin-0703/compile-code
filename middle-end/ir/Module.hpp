#pragma once

#include "IRContext.hpp"
#include "Instruction.hpp"
#include <algorithm>
#include <memory>
#include <unordered_map>

namespace ir {
class Function;

class Argument : public Value {
private:
    Function* parent;
public:
    Argument(const Type* type, Function* parent) : Value(type), parent(parent) {}
    Function* getParent() const { return parent; }
};

class BasicBlock : public Value {
private:
    Function* parent;
    std::vector<std::unique_ptr<Instruction>> instructions;
public:
    BasicBlock(IRContext& context, Function* parent)
        : Value(context.getLabelType()), parent(parent) {}
    // override 表示重写虚函数，编译器会检查签名是否正确。
    ~BasicBlock() override { dropAllReferences(); }
    Function* getParent() const { return parent; }
    const auto& getInstructions() const { return instructions; }
    Instruction* getTerminator() const {
        if (instructions.empty() || !instructions.back()->isTerminator()) return nullptr;
        return instructions.back().get();
    }
    Instruction* append(std::unique_ptr<Instruction> instruction) {
        if (getTerminator() != nullptr) throw std::logic_error("Cannot append after terminator");
        instruction->setParent(this);
        Instruction* result = instruction.get();
        instructions.push_back(std::move(instruction));
        return result;
    }
    Instruction* prependAlloca(std::unique_ptr<Instruction> instruction) {
        if (instruction->getOpcode() != Opcode::Alloca) throw std::logic_error("Expected alloca");
        instruction->setParent(this);
        Instruction* result = instruction.get();
        instructions.insert(instructions.begin(), std::move(instruction));
        return result;
    }
    void erase(Instruction* instruction) {
        if (!instruction->useEmpty()) throw std::logic_error("Instruction is still used");
        auto position = std::find_if(instructions.begin(), instructions.end(),
            [&](const auto& item) { return item.get() == instruction; });
        if (position == instructions.end()) throw std::logic_error("Instruction not in block");
        instructions.erase(position);
    }
    void dropAllReferences() {
        for (const auto& instruction : instructions) instruction->dropAllReferences();
    }
};

class Function : public Value {
private:
    std::vector<std::unique_ptr<Argument>> arguments;
    std::vector<std::unique_ptr<BasicBlock>> blocks;
public:
    Function(const Type* type, std::string name) : Value(type, std::move(name)) {
        for (const Type* parameter : type->getParamTypes()) {
            arguments.push_back(std::make_unique<Argument>(parameter, this));
        }
    }
    ~Function() override { dropAllReferences(); }
    const Type* getReturnType() const { return getType()->getReturnType(); }
    const auto& getArguments() const { return arguments; }
    const auto& getBlocks() const { return blocks; }
    bool isDeclaration() const { return blocks.empty(); }
    BasicBlock* createBlock() {
        auto block = std::make_unique<BasicBlock>(getType()->getContext(), this);
        BasicBlock* result = block.get();
        blocks.push_back(std::move(block));
        return result;
    }
    BasicBlock* getEntry() const { return blocks.at(0).get(); }
    void dropAllReferences() {
        // 先断开整个函数的引用，才能安全销毁有回边的控制流图。
        for (const auto& block : blocks) block->dropAllReferences();
    }
};

class GlobalVariable : public Value {
private:
    const Type* storedType;
    bool constant;
    std::vector<std::uint32_t> initialValues;
public:
    GlobalVariable(IRContext& context, const Type* storedType, std::string name,
                   bool constant, std::vector<std::uint32_t> initialValues)
        : Value(context.getPointerType(storedType), std::move(name)), storedType(storedType),
          constant(constant), initialValues(std::move(initialValues)) {}
    const Type* getStoredType() const { return storedType; }
    bool isConstant() const { return constant; }
    const auto& getInitialValues() const { return initialValues; }
};

class Module {
private:
    IRContext& context;
    std::vector<std::unique_ptr<GlobalVariable>> globals;
    std::vector<std::unique_ptr<Function>> functions;
    std::unordered_map<std::string, Value*> names;
public:
    explicit Module(IRContext& context) : context(context) {}
    ~Module() {
        // 函数之间也可能相互引用，必须在释放任意函数前统一断开。
        for (const auto& function : functions) function->dropAllReferences();
    }
    // = delete 禁止复制/移动，以免对象地址变化后使 IR 引用失效。
    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;
    IRContext& getContext() const { return context; }
    const auto& getGlobals() const { return globals; }
    const auto& getFunctions() const { return functions; }
    Function* addFunction(const std::string& name, const Type* returnType,
                          const std::vector<const Type*>& parameters) {
        const Type* type = context.getFunctionType(returnType, parameters);
        auto found = names.find(name);
        if (found != names.end()) {
            auto* function = dynamic_cast<Function*>(found->second);
            if (function == nullptr || function->getType() != type) {
                throw std::logic_error("Conflicting function: " + name);
            }
            return function;
        }
        auto function = std::make_unique<Function>(type, name);
        Function* result = function.get();
        functions.push_back(std::move(function));
        names[name] = result;
        return result;
    }
    Function* getFunction(const std::string& name) const {
        auto found = names.find(name);
        if (found == names.end()) throw std::logic_error("Unknown function: " + name);
        return dynamic_cast<Function*>(found->second);
    }
    GlobalVariable* addGlobal(const std::string& name, const Type* type, bool constant,
                              std::vector<std::uint32_t> initialValues) {
        if (names.count(name)) throw std::logic_error("Duplicate IR global: " + name);
        auto global = std::make_unique<GlobalVariable>(context, type, name, constant,
                                                       std::move(initialValues));
        GlobalVariable* result = global.get();
        globals.push_back(std::move(global));
        names[name] = result;
        return result;
    }
};
} // namespace ir
