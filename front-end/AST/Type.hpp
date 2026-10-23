#pragma  once

enum class BasicType {
    Int,
    Char,
    Void
};

enum class BType {
    Int,
    Char
};

enum class FuncType {
    Int,
    Char,
    Void
};

enum class ExprType {
    Int,
    Char,
    IntArray,
    CharArray,
    Void,
    Error
};

struct ExprInfo {
    ExprType type;
    int line;
};
