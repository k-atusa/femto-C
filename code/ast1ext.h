#ifndef AST1EXT_H
#define AST1EXT_H

#include "ast1.h"

// instanciate templates
class A1Ext {
    public:
    CompileMessage prt;
    int arch; // target architecture in bytes
    A1Gen* ast1;
    std::vector<std::unique_ptr<A1Module>> modules; // source files

    A1Ext(): prt(3), arch(8), ast1(nullptr), modules() {}
    A1Ext(A1Gen& ast): prt(ast.prt), arch(ast.arch), ast1(&ast), modules() {}

    std::string toString() {
        std::string result = "A1Ext";
        for (auto& mod : modules) result += "\n\n\n" + mod->toString();
        return result;
    }

    int findModule(const std::string& uname); // find module by unique name
    int findModule(const std::string& path, std::vector<A1Type*> args); // find module by path and template args
    bool completeType(A1Module& mod, A1Type& tgt); // calculate sizes and aligns

    std::string complete(std::unique_ptr<A1Module> mod, std::vector<std::unique_ptr<A1Type>> args); // instanciate templates

    private:
    bool completeStruct(A1Module& mod, A1DeclStruct& tgt); // calculate struct sizes and aligns
    void standardizeType(A1Module& mod, A1Type& tgt); // make incName absolute uname to module
};

#endif // AST1EXT_H