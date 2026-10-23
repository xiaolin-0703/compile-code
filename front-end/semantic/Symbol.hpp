#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "../AST/Ident.hpp"
#include "../AST/Type.hpp"

inline BasicType toBasicType(BType type) {
    switch (type) {
        case BType::Int: return BasicType::Int;
        case BType::Char: return BasicType::Char;
    }
    throw std::runtime_error("不是Btype");
}

inline BasicType toBasicType(FuncType type) {
    switch (type) {
        case FuncType::Int: return BasicType::Int;
        case FuncType::Char: return BasicType::Char;
        case FuncType::Void: return BasicType::Void;
    }
    throw std::invalid_argument("Unknown FuncType");
}

enum class SymbolKind {
    Variable,
    Array,
    Function
};

struct ParamInfo {
    BType type = BType::Int;
    bool isArray = false;

    bool operator==(const ParamInfo& other) const {
        return type == other.type && isArray == other.isArray;
    }

    bool operator!=(const ParamInfo& other) const {
        return !(*this == other);
    }
};

class Symbol {
public:
    Ident ident;
    SymbolKind kind;
    BasicType type;
    bool isConst = false;
    bool isStatic = false;
    std::vector<ParamInfo> parameters;//函数的参数列表

    explicit Symbol(Ident ident, SymbolKind kind, BasicType type,
                    bool isConst = false, bool isStatic = false)
        : ident(std::move(ident)), kind(kind), type(type),
          isConst(isConst), isStatic(isStatic) {}

    explicit Symbol(Ident ident, SymbolKind kind, BType type,
                    bool isConst = false, bool isStatic = false)
        : Symbol(std::move(ident), kind, toBasicType(type), isConst, isStatic) {}

    explicit Symbol(Ident ident, FuncType returnType,
                    std::vector<ParamInfo> parameters = {})
        : Symbol(std::move(ident), SymbolKind::Function, toBasicType(returnType)) {
        this->parameters = std::move(parameters);
    }

    const std::string& getName() const {
        return ident.getName();
    }

    int getLineNum() const {
        return ident.getLineNum();
    }

    bool isVariable() const {
        return kind == SymbolKind::Variable;
    }

    bool isArray() const {
        return kind == SymbolKind::Array;
    }

    bool isFunction() const {
        return kind == SymbolKind::Function;
    }

    std::size_t getParamCount() const {
        return parameters.size();
    }

    std::string getTypeName() const {
        std::string name;
        switch (type) {
            case BasicType::Int: name = "Int"; break;
            case BasicType::Char: name = "Char"; break;
            case BasicType::Void: name = "Void"; break;
        }
        if (isFunction()) {
            return name + "Func";
        }
        if (type == BasicType::Void) {
            throw std::logic_error("Variables and arrays cannot have void type");
        }
        if (isConst) {
            name = "Const" + name;
        } else if (isStatic) {
            name = "Static" + name;
        }
        if (isArray()) {
            name += "Array";
        }
        return name;
    }
};
