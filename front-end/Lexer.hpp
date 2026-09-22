#ifndef CODE_LEXER_HPP
#define CODE_LEXER_HPP

#include <string>
#include <vector>

#include "compileError.hpp"
#include "Token.hpp"

class Lexer {
public:
    explicit Lexer(const std::string& code);

    void startLexing();

    const std::vector<Token>& getTokens() const;
    const std::vector<compileError>& getErrors() const;

private:
    std::string sourceCode;
    std::size_t currentIndex;
    int lineNum;
    std::size_t length;

    std::vector<Token> tokens;
    std::vector<compileError> errors;

    std::string getNextWord();

    void addToken(const std::string& word, int lineNum);
};

#endif
