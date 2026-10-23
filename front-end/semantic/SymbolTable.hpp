#pragma once
#include "Symbol.hpp"
#include <unordered_map>
#include <vector>

class SymbolTable {
public:
    int tableid;
    int fatherId;
    std::unordered_map<std::string, Symbol> directory;
    std::vector<std::string> declarationOrder;

    explicit SymbolTable(int tableid,int fatherId) :
     tableid(tableid), fatherId(fatherId) {}
    
    bool addSymbol(Symbol symbol) {
        std::string name = symbol.ident.getName();
        auto result = directory.emplace(name, std::move(symbol));

        if (!result.second) {
            return false;
        }
        declarationOrder.push_back(std::move(name));
        return true;
    }   


};