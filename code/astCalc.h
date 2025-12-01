#ifndef ASTCALC_H
#define ASTCALC_H

#include "astGen.h"

// instanciate templates
class ASTCalc {
    public:
    CompileMessage prt;
    int arch; // target architecture in bytes
    ASTGen* astGen;
    std::vector<std::unique_ptr<SrcFile>> srcTrees; // source files
    std::vector<std::vector<int>> srcSizes; // template args sizes
    std::vector<std::vector<int>> srcAligns; // template args aligns

    ASTCalc(): prt(3), arch(8), astGen(nullptr), srcTrees(), srcSizes(), srcAligns() {}
    ASTCalc(ASTGen& ast): prt(ast.prt), arch(ast.arch), astGen(&ast), srcTrees(), srcSizes(), srcAligns() {}

    int findSource(const std::string& path, std::vector<int>& tmpSizes, std::vector<int>& tmpAligns); // find source file index, -1 if not found
    int findSource(const std::string& uname); // find source file index by unique name, -1 if not found
    std::string complete(std::unique_ptr<SrcFile> src, std::vector<int>& tmpSizes, std::vector<int>& tmpAligns); // return error message or empty string if ok

    private:
    std::string getLocString(const Location& loc) { return std::format("{}:{}", astGen->srcFiles[loc.srcLoc]->path, loc.line); }
    bool completeType(SrcFile& src, TypeNode& tgt); // calculate type size, return true if modified
    bool completeStruct(SrcFile& src, DeclStructNode& tgt); // complete struct size, return true if modified
};

#endif // ASTCALC_H