#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace ir {
    class IRContext;
    enum class TypeKind {
        Void,
        Integer,
        Pointer,
        Array,
        Function,
        Label
    };

    // IR 类型由 IRContext 创建并复用；创建后不可修改。
    // 同一个 Context 中，相同类型对应同一个 Type 对象。
    class Type {

    private:
        friend class IRContext; // friend 允许 Context 使用私有构造函数并设置类型字段。
        Type(IRContext& context, TypeKind kind)
            : context(&context), kind(kind) {}

        IRContext* context;
        TypeKind kind;
        unsigned bitWidth = 0;
        const Type* elementType = nullptr;
        std::size_t arrayLength = 0;
        const Type* returnType = nullptr;
        std::vector<const Type*> paramTypes;


    public:
        Type(const Type&) = delete;
        Type& operator=(const Type&) = delete;
        Type(Type&&) = delete;
        Type& operator=(Type&&) = delete;

        IRContext& getContext() const { return *context; }
        TypeKind getKind() const { return kind; }

        bool isVoid() const { return kind == TypeKind::Void; }
        bool isInteger() const { return kind == TypeKind::Integer; }
        bool isInteger(unsigned bits) const {
            return isInteger() && bitWidth == bits;
        }
        bool isPointer() const { return kind == TypeKind::Pointer; }
        bool isArray() const { return kind == TypeKind::Array; }
        bool isFunction() const { return kind == TypeKind::Function; }
        bool isLabel() const { return kind == TypeKind::Label; }

        // 本项目第一版支持作为标量操作数的类型。
        bool isScalar() const { return isInteger() || isPointer(); }
        bool isStorable() const { return isScalar() || isArray(); }

        unsigned getBitWidth() const {
            if (!isInteger()) {
                throw std::logic_error("getBitWidth requires an integer type");
            }
            return bitWidth;
        }

        const Type* getElementType() const {
            if (!isPointer() && !isArray()) {
                throw std::logic_error("getElementType requires a pointer or array type");
            }
            return elementType;
        }

        std::size_t getArrayLength() const {
            if (!isArray()) {
                throw std::logic_error("getArrayLength requires an array type");
            }
            return arrayLength;
        }

        const Type* getReturnType() const {
            if (!isFunction()) {
                throw std::logic_error("getReturnType requires a function type");
            }
            return returnType;
        }

        const std::vector<const Type*>& getParamTypes() const {
            if (!isFunction()) {
                throw std::logic_error("getParamTypes requires a function type");
            }
            return paramTypes;
        }

        // 类型的文本表示，不负责打印指令或给 Value 编号。
        std::string toString() const {
            switch (kind) {
                case TypeKind::Void:
                    return "void";
                case TypeKind::Integer:
                    return "i" + std::to_string(bitWidth);
                case TypeKind::Pointer:
                    return elementType->toString() + "*";
                case TypeKind::Array:
                    return "[" + std::to_string(arrayLength) + " x "
                        + elementType->toString() + "]";
                case TypeKind::Function: {
                    std::string result = returnType->toString() + " (";
                    for (std::size_t i = 0; i < paramTypes.size(); ++i) {
                        if (i != 0) result += ", ";
                        result += paramTypes[i]->toString();
                    }
                    return result + ")";
                }
                case TypeKind::Label:
                    return "label";
            }
            throw std::logic_error("Unknown IR type");
        }



    }; // namespace ir
}
