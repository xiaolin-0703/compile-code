#pragma once
#include <string>

class Token {
public:
    Token(std::string _word,std::string _tokenType,int _lineNum):
        word(_word), tokenType(_tokenType), lineNum(_lineNum){}

    std::string getWord() const {
        return word;
    }

    std::string getType() const {
        return tokenType;
    }

    int getLineNum() const {
        return lineNum;
    }


private:
    std::string word;
    std::string tokenType;
    int lineNum;
};