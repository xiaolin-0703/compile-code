#pragma once

#include "Constant.hpp"
#include "Type.hpp"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ir {

// Context 拥有 Type 和 Constant；它必须比引用这些对象的 IR 活得更久。
class IRContext final {
private:
    Type* createType(TypeKind kind) {
        auto type = std::unique_ptr<Type>(new Type(*this, kind));
        Type* result = type.get();
        types.push_back(std::move(type));
        return result;
    }

    void requireLocalType(const Type* type) const {
        if (type == nullptr || &type->getContext() != this) {
            throw std::invalid_argument("Type must belong to this IRContext");
        }
    }

    // 成员按声明的逆序销毁：先销毁常量，再销毁类型。
    std::vector<std::unique_ptr<Type>> types;
    std::unordered_map<std::uint64_t, std::unique_ptr<ConstantInt>> constants;
    const Type* voidType = nullptr;
    const Type* labelType = nullptr;
    const Type* int1Type = nullptr;
    const Type* int8Type = nullptr;
    const Type* int32Type = nullptr;

public:
    IRContext() {
        voidType = createType(TypeKind::Void);
        labelType = createType(TypeKind::Label);
        Type* i1 = createType(TypeKind::Integer);
        i1->bitWidth = 1;
        int1Type = i1;
        Type* i8 = createType(TypeKind::Integer);
        i8->bitWidth = 8;
        int8Type = i8;
        Type* i32 = createType(TypeKind::Integer);
        i32->bitWidth = 32;
        int32Type = i32;
    }

    ~IRContext() = default;
    IRContext(const IRContext&) = delete;
    IRContext& operator=(const IRContext&) = delete;
    IRContext(IRContext&&) = delete;
    IRContext& operator=(IRContext&&) = delete;

    const Type* getVoidType() const { return voidType; }
    const Type* getLabelType() const { return labelType; }
    const Type* getInt1Type() const { return int1Type; }
    const Type* getInt8Type() const { return int8Type; }
    const Type* getInt32Type() const { return int32Type; }
    const Type* getIntegerType(unsigned bits) const {
        switch (bits) {
        case 1: return int1Type;
        case 8: return int8Type;
        case 32: return int32Type;
        default:
            throw std::invalid_argument("Supported integer widths are 1, 8 and 32");
        }
    }

    const Type* getPointerType(const Type* elementType) {
        requireLocalType(elementType);
        if (!elementType->isStorable() && !elementType->isFunction()) {
            throw std::invalid_argument("Invalid pointer element type");
        }
        for (const auto& type : types) {
            if (type->isPointer() && type->elementType == elementType) {
                return type.get();
            }
        }
        Type* type = createType(TypeKind::Pointer);
        type->elementType = elementType;
        return type;
    }

    const Type* getArrayType(const Type* elementType, std::size_t length) {
        requireLocalType(elementType);
        if (!elementType->isStorable()) {
            throw std::invalid_argument("Array element type must be storable");
        }
        for (const auto& type : types) {
            if (type->isArray() && type->elementType == elementType
                && type->arrayLength == length) {
                return type.get();
            }
        }
        Type* type = createType(TypeKind::Array);
        type->elementType = elementType;
        type->arrayLength = length;
        return type;
    }

    const Type* getFunctionType(
        const Type* returnType,
        const std::vector<const Type*>& paramTypes) {
        requireLocalType(returnType);
        if (!returnType->isVoid() && !returnType->isScalar()) {
            throw std::invalid_argument("Unsupported function return type");
        }
        for (const Type* paramType : paramTypes) {
            requireLocalType(paramType);
            if (!paramType->isScalar()) {
                throw std::invalid_argument("Function parameters must be scalar types");
            }
        }
        for (const auto& type : types) {
            if (type->isFunction() && type->returnType == returnType
                && type->paramTypes == paramTypes) {
                return type.get();
            }
        }
        // 先复制可能分配内存的参数表，成功后再发布类型。
        auto type = std::unique_ptr<Type>(new Type(*this, TypeKind::Function));
        type->returnType = returnType;
        type->paramTypes = paramTypes;
        const Type* result = type.get();
        types.push_back(std::move(type));
        return result;
    }

    ConstantInt* getInt(const Type* type, std::int64_t value) {
        requireLocalType(type);
        if (!type->isInteger()) {
            throw std::invalid_argument("ConstantInt requires an integer type");
        }
        const unsigned width = type->getBitWidth();
        const std::uint32_t bits = ConstantInt::normalize(width, value);
        const std::uint64_t key = (std::uint64_t{width} << 32) | bits;
        const auto found = constants.find(key);
        if (found != constants.end()) return found->second.get();

        auto constant = std::unique_ptr<ConstantInt>(new ConstantInt(type, bits));
        ConstantInt* result = constant.get();
        constants.emplace(key, std::move(constant));
        return result;
    }


};

} // namespace ir
