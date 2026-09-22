#pragma once
#include <iostream>

class compileError {
    private:
         int lineNum;
         char errorType;

    public:
        compileError(int _lineNum,char errorType) : lineNum(_lineNum), errorType(errorType) {}

         int getLineNum() const {
            return lineNum;
        }

         char getErrorType() const  {
            return errorType;
        }

        void outError() {
            std::cout << lineNum << " " << errorType << std::endl;
        }
};