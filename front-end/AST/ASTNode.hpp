#pragma once

#include <memory>

class ASTNode {
    public:
        virtual ~ASTNode() = default;
};


class BlockItem : public ASTNode {
    public:
        virtual ~BlockItem() = default;
};

using BlockItemPtr = std::unique_ptr<BlockItem>;