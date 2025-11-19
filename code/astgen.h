#ifndef ASTGEN_H
#define ASTGEN_H

#include <format>
#include "baseFunc.h"
#include "tokenizer.h"

// AST node types
enum class ASTNodeType {
    NONE,
    // compiler order
    INCLUDE,
    DECL_TEMPLATE,
    RAW_C,
    RAW_IR,
    // type node
    TYPE,
    // expression
    LITERAL,
    LITERAL_KEY,
    LITERAL_ARRAY,
    NAME,
    OPERATION,
    FUNC_CALL,
    // long statement
    DECL_VAR,
    ASSIGN,
    // short statement
    EMPTY, // empty statement
    RETURN,
    DEFER,
    BREAK,
    CONTINUE,
    // control statement
    SCOPE,
    IF,
    WHILE,
    FOR,
    SWITCH,
    // function, struct, enum
    DECL_FUNC,
    DECL_STRUCT,
    DECL_ENUM
};

// parent of AST nodes
class ASTNode {
    public:
    ASTNodeType objType;
    Location location;
    std::string text; // used for name or raw code

    ASTNode(): objType(ASTNodeType::NONE), location(), text("") {}
    ASTNode(ASTNodeType t): objType(t), location(), text("") {}
    ASTNode(ASTNodeType tp, const std::string& tx): objType(tp), location(), text(tx) {}
    virtual ~ASTNode() = default;

    virtual std::string toString(int indent) { return std::string(indent, '  ') + std::format("AST {} {}", (int)objType, text); }
};

// include node
class IncludeNode: public ASTNode {
    public:
    std::string path; // normal or template source file path
    std::string& name; // import name
    std::vector<std::unique_ptr<TypeNode>> args; // template type arguments

    IncludeNode(): ASTNode(ASTNodeType::INCLUDE), path(""), name(text), args() {}

    std::string toString(int indent) {
        std::string result = std::string(indent, '  ') + std::format("INCLUDE {} {}", path, name);
        for (auto& arg : args) {
            result += "\n" + arg->toString(indent + 1);
        }
        return result;
    }
};

// template variable node
class DeclTemplateNode: public ASTNode {
    public:
    std::string& name; // template argument name

    DeclTemplateNode(): ASTNode(ASTNodeType::DECL_TEMPLATE), name(text) {}

    std::string toString(int indent) { return std::string(indent, '  ') + std::format("DECLTMP {}", name); }
};

// raw code node
class RawCodeNode: public ASTNode {
    public:
    std::string& code; // raw code of C or IR

    RawCodeNode(): ASTNode(ASTNodeType::NONE), code(text) {}
    RawCodeNode(ASTNodeType tp): ASTNode(tp), code(text) {}

    std::string toString(int indent) { return std::string(indent, '  ') + std::format("RAW {}", code); }
};

// type node
enum class TypeNodeType {
    NONE,
    PRIMITIVE,
    POINTER,
    ARRAY,
    SLICE,
    FUNCTION,
    NAME, // for struct, enum, template
    FOREIGN // from other source file
};

class TypeNode: public ASTNode {
    public:
    TypeNodeType subType;
    std::string& name; // type name
    std::string includeName; // include namespace for FOREIGN type
    std::unique_ptr<TypeNode> direct; // ptr, arr, slice target & func return
    std::vector<std::unique_ptr<TypeNode>> indirect; // func args
    int64_t length; // array length
    int typeSize; // total size in bytes
    int typeAlign; // align requirement in bytes

    TypeNode(): ASTNode(ASTNodeType::TYPE), subType(TypeNodeType::NONE), name(text), includeName(""), direct(nullptr), indirect(), length(-1), typeSize(-1), typeAlign(-1) {}
    TypeNode(TypeNodeType tp, const std::string& nm): ASTNode(ASTNodeType::TYPE, nm), subType(tp), name(text), includeName(""), direct(nullptr), indirect(), length(-1), typeSize(-1), typeAlign(-1) {}
    TypeNode(const std::string& incNm, const std::string& tpNm): ASTNode(ASTNodeType::TYPE, tpNm), subType(TypeNodeType::FOREIGN), name(text), includeName(incNm), direct(nullptr), indirect(), length(-1), typeSize(-1), typeAlign(-1) {}

    std::string toString(int indent) {
        std::string result = std::string(indent, '  ') + std::format("TYPE {} {} {} {} {} {}", name, includeName, subType, length, typeSize, typeAlign);
        if (direct) result += "\n" + direct->toString(indent + 1);
        for (auto& ind : indirect) {
            result += "\n" + ind->toString(indent + 1);
        }
        return result;
    }

    std::string toString() {
        std::string result;
        switch(subType) {
            case TypeNodeType::PRIMITIVE: case TypeNodeType::NAME:
                result = name;
                break;
            case TypeNodeType::FOREIGN:
                result = includeName + "." + name;
                break;
            case TypeNodeType::POINTER:
                result = direct->toString() + name;
                break;
            case TypeNodeType::ARRAY: case TypeNodeType::SLICE:
                if (direct && (direct->subType == TypeNodeType::ARRAY || direct->subType == TypeNodeType::SLICE)) { // nested array
                    int count = 1;
                    TypeNode* curr = direct.get();
                    while (curr->direct && (curr->direct->subType == TypeNodeType::ARRAY || curr->direct->subType == TypeNodeType::SLICE)) {
                        curr = curr->direct.get();
                        count += 1;
                    }
                    std::string temp = direct->toString();
                    int pos = temp.length();
                    while (count != 0) {
                        pos--;
                        if (temp[pos] == '[') count -= 1;
                    }
                    result = temp.substr(0, pos) + name + temp.substr(pos);
                } else {
                    result = direct->toString() + name;
                }
                break;
            case TypeNodeType::FUNCTION:
                result = direct->toString() + "(";
                for (auto& arg : indirect) {
                    result += arg->toString() + ",";
                }
                if (indirect.size() > 0) result.pop_back();
                result += ")";
                break;
            default: result = "unknown";
        }
        return result;
    }
};

// atomic expression for literal, name
class AtomicExprNode: public ASTNode {
    public:
    Literal literal; // literal
    std::vector<std::unique_ptr<ASTNode>> elements; // literal array
    std::string& word; // keyword(null, true, false) or name

    AtomicExprNode(): ASTNode(ASTNodeType::NONE), literal(), elements(), word(text) {}
    AtomicExprNode(Literal l): ASTNode(ASTNodeType::LITERAL), literal(l), elements(), word(text) {}
    AtomicExprNode(const std::string& name): ASTNode(ASTNodeType::NAME, name), literal(), elements(), word(text) {}

    std::string toString(int indent) {
        std::string result = std::string(indent, '  ');
        if (objType == ASTNodeType::LITERAL) result += std::format("LITERAL {}", literal.toString());
        if (objType == ASTNodeType::LITERAL_ARRAY) {
            result += "LITERAL_ARRAY";
            for (auto& element : elements) result += "\n" + element->toString(indent + 1);
        }
        if (objType == ASTNodeType::LITERAL_KEY) result += std::format("LITERAL_KEY {}", word);
        if (objType == ASTNodeType::NAME) result += std::format("NAME {}", word);
        return result;
    }
};

// operator node, ordered by priority
enum class OperationType {
    NONE,
    B_DOT, B_INDEX, T_SLICE,
    U_PLUS, U_MINUS, U_LOGIC_NOT, U_BIT_NOT, U_REF, U_DEREF,
    B_MUL, B_DIV, B_MOD,
    B_ADD, B_SUB,
    B_SHL, B_SHR,
    B_LT, B_LE, B_GT, B_GE,
    B_EQ, B_NE,
    B_BIT_AND,
    B_BIT_XOR,
    B_BIT_OR,
    B_LOGIC_AND,
    B_LOGIC_OR,
    // integrated func
    U_SIZEOF, B_CAST, B_MAKE, U_LEN
};

int getOperandNum(OperationType op) {
    switch (op) {
        case OperationType::NONE:
            return 0;
        case OperationType::T_SLICE:
            return 3;
        case OperationType::U_PLUS: case OperationType::U_MINUS: case OperationType::U_LOGIC_NOT: case OperationType::U_BIT_NOT:
        case OperationType::U_REF: case OperationType::U_DEREF: case OperationType::U_SIZEOF: case OperationType::U_LEN:
            return 1;
        default:
            return 2;
    }
}

class OperationNode: public ASTNode {
    public:
    OperationType subType;
    std::unique_ptr<ASTNode> operand0; // for unary
    std::unique_ptr<ASTNode> operand1; // for binary
    std::unique_ptr<ASTNode> operand2; // for triple

    OperationNode(): ASTNode(ASTNodeType::OPERATION), subType(OperationType::NONE), operand0(nullptr), operand1(nullptr), operand2(nullptr) {}
    OperationNode(OperationType tp): ASTNode(ASTNodeType::OPERATION), subType(tp), operand0(nullptr), operand1(nullptr), operand2(nullptr) {}

    std::string toString(int indent) {
        std::string result = std::string(indent, '  ') + std::format("OPERATION {}", subType);
        if (operand0 != nullptr) result += "\n" + operand0->toString(indent + 1);
        if (operand1 != nullptr) result += "\n" + operand1->toString(indent + 1);
        if (operand2 != nullptr) result += "\n" + operand2->toString(indent + 1);
        return result;
    }
};

// function call node
class FuncCallNode: public ASTNode {
    public:
    std::unique_ptr<ASTNode> funcExpr; // function expression
    std::vector<std::unique_ptr<ASTNode>> args; // argument expressions

    FuncCallNode(): ASTNode(ASTNodeType::FUNC_CALL), funcExpr(nullptr), args() {}

    std::string toString(int indent) {
        std::string result = std::string(indent, '  ') + "FUNC_CALL";
        result += "\n" + funcExpr->toString(indent + 1);
        for (auto& arg : args) {
            result += "\n" + arg->toString(indent + 1);
        }
        return result;
    }
};

// statement node
class LongStatNode: public ASTNode {
    public:
    std::unique_ptr<TypeNode> varType; // declare type
    std::unique_ptr<ASTNode> varName; // declare name, assign lvalue
    std::unique_ptr<ASTNode> varExpr; // initialize, assign rvalue
    bool isDefine;
    bool isExtern;
    bool isExported;

    LongStatNode(): ASTNode(ASTNodeType::NONE), varType(nullptr), varName(nullptr), varExpr(nullptr), isDefine(false), isExtern(false), isExported(false) {}
    LongStatNode(ASTNodeType tp): ASTNode(tp), varType(nullptr), varName(nullptr), varExpr(nullptr), isDefine(false), isExtern(false), isExported(false) {}

    std::string toString(int indent) {
        std::string result = std::string(indent, '  ') + std::format("LONGSTAT {} {} {} {}", objType, isDefine, isExtern, isExported);
        if (varType) result += "\n" + varType->toString(indent + 1);
        if (varName) result += "\n" + varName->toString(indent + 1);
        if (varExpr) result += "\n" + varExpr->toString(indent + 1);
        return result;
    }
};

class ShortStatNode: public ASTNode {
    public:
    std::unique_ptr<ASTNode> statExpr; // for return, defer

    ShortStatNode(): ASTNode(ASTNodeType::NONE), statExpr(nullptr) {}
    ShortStatNode(ASTNodeType tp): ASTNode(tp), statExpr(nullptr) {}

    std::string toString(int indent) {
        std::string result = std::string(indent, '  ') + std::format("SHORTSTAT {}", objType);
        if (statExpr) result += "\n" + statExpr->toString(indent + 1);
        return result;
    }
};

// control statement node
class ScopeNode: public ASTNode {
    public:
    std::vector<std::unique_ptr<ASTNode>> body;
    ScopeNode* parent;

    ScopeNode(): ASTNode(ASTNodeType::SCOPE), body(), parent(nullptr) {}
    ScopeNode(ScopeNode* p): ASTNode(ASTNodeType::SCOPE), body(), parent(p) {}

    std::string toString(int indent) {
        std::string result = std::string(indent, '  ') + "SCOPE";
        for (auto& node : body) result += "\n" + node->toString(indent + 1);
        return result;
    }

    ASTNode* findVarByName(const std::string& name); // find variable declaration (decl_var, for, decl_func), nullptr if not found
    Literal findDefinedLiteral(const std::string& name); // find defined literal variable, type NONE if not found
};

class IfNode: public ASTNode {
    public:
    std::unique_ptr<ASTNode> cond;
    std::unique_ptr<ASTNode> ifBody;
    std::unique_ptr<ASTNode> elseBody;

    IfNode(): ASTNode(ASTNodeType::IF), cond(nullptr), ifBody(nullptr), elseBody(nullptr) {}

    std::string toString(int indent) {
        std::string result = std::string(indent, '  ') + "IF";
        if (cond) result += "\n" + cond->toString(indent + 1);
        if (ifBody) result += "\n" + ifBody->toString(indent + 1);
        if (elseBody) result += "\n" + elseBody->toString(indent + 1);
        return result;
    }
};

class WhileNode: public ASTNode {
    public:
    std::unique_ptr<ASTNode> cond;
    std::unique_ptr<ASTNode> body;

    WhileNode(): ASTNode(ASTNodeType::WHILE), cond(nullptr), body(nullptr) {}

    std::string toString(int indent) {
        std::string result = std::string(indent, '  ') + "WHILE";
        if (cond) result += "\n" + cond->toString(indent + 1);
        if (body) result += "\n" + body->toString(indent + 1);
        return result;
    }
};

class ForNode: public ASTNode {
    public:
    std::unique_ptr<ASTNode> init;
    std::unique_ptr<ASTNode> cond;
    std::unique_ptr<ASTNode> step;
    std::unique_ptr<ASTNode> body;

    ForNode(): ASTNode(ASTNodeType::FOR), init(nullptr), cond(nullptr), step(nullptr), body(nullptr) {}

    std::string toString(int indent) {
        std::string result = std::string(indent, '  ') + "FOR";
        if (init) result += "\n" + init->toString(indent + 1);
        if (cond) result += "\n" + cond->toString(indent + 1);
        if (step) result += "\n" + step->toString(indent + 1);
        if (body) result += "\n" + body->toString(indent + 1);
        return result;
    }
};

class SwitchNode: public ASTNode {
    public:
    std::unique_ptr<ASTNode> cond;
    std::vector<std::unique_ptr<ASTNode>> caseExprs;
    std::vector<std::unique_ptr<ScopeNode>> caseBodies;
    std::unique_ptr<ScopeNode> defaultBody;

    SwitchNode(): ASTNode(ASTNodeType::SWITCH), cond(nullptr), caseExprs(), caseBodies() {defaultBody = nullptr;}

    std::string toString(int indent) {
        std::string result = std::string(indent, '  ') + "SWITCH";
        if (cond) result += "\n" + cond->toString(indent + 1);
        for (auto& expr : caseExprs) result += "\n" + expr->toString(indent + 1);
        for (auto& body : caseBodies) result += "\n" + body->toString(indent + 1);
        if (defaultBody) result += "\n" + defaultBody->toString(indent + 1);
        return result;
    }
};

// declaration nodes
class DeclFuncNode: public ASTNode {
    public:
    std::string& name; // mangled name (struct.func)
    std::string structNm; // for method
    std::string funcNm; // for method
    std::vector<std::unique_ptr<TypeNode>> paramTypes;
    std::vector<std::string> paramNames;
    std::unique_ptr<TypeNode> retType;
    std::unique_ptr<ASTNode> body;
    bool isVaArg;
    bool isExported;

    DeclFuncNode(): ASTNode(ASTNodeType::DECL_FUNC), name(text), structNm(""), funcNm(""), paramTypes(), paramNames(), retType(nullptr), body(nullptr), isVaArg(false), isExported(false) {}

    std::string toString(int indent) {
        std::string result = std::string(indent, '  ') + std::format("DECLFUNC {} {} {}", name, isVaArg, isExported);
        if (retType) result += "\n" + retType->toString(indent + 1);
        for (auto& type : paramTypes) result += "\n" + type->toString(indent + 1);
        if (body) result += "\n" + body->toString(indent + 1);
        return result;
    }
};

class DeclStructNode: public ASTNode {
    public:
    std::string& name;
    int structSize; // total size in bytes
    int structAlign; // align requirement in bytes
    std::vector<std::unique_ptr<TypeNode>> memTypes;
    std::vector<std::string> memNames;
    std::vector<int> memOffsets;
    bool isExported;

    DeclStructNode(): ASTNode(ASTNodeType::DECL_STRUCT), name(text), structSize(-1), structAlign(-1), memTypes(), memNames(), memOffsets(), isExported(false) {}

    std::string toString(int indent) {
        std::string result = std::string(indent, '  ') + std::format("DECLSTRUCT {} {} {} {}", name, structSize, structAlign, isExported);
        for (size_t i = 0; i < memNames.size(); i++) {
            result += "\n" + std::string(indent + 1, '  ') + std::to_string(memOffsets[i]) + "\n" + memTypes[i]->toString(indent + 1);
        }
        return result;
    }
};

class DeclEnumNode: public ASTNode {
    public:
    std::string& name;
    int enumSize; // size in bytes
    std::vector<std::string> memNames;
    std::vector<int64_t> memValues;
    bool isExported;

    DeclEnumNode(): ASTNode(ASTNodeType::DECL_ENUM), name(text), enumSize(-1), memNames(), memValues(), isExported(false) {}

    std::string toString(int indent) {
        std::string result = std::string(indent, '  ') + std::format("DECLENUM {} {} {}", name, enumSize, isExported);
        for (size_t i = 0; i < memNames.size(); i++) {
            result += "\n" + std::string(indent + 1, '  ') + memNames[i] + " " + std::to_string(memValues[i]);
        }
        return result;
    }
};

// abstract source file
class SrcFile {
    public:
    std::string path;
    std::string uniqueName; // for non-duplicate compile
    std::unique_ptr<ScopeNode> code;
    bool isFinished;

    SrcFile(): path(""), uniqueName(""), isFinished(false), code() { code = std::make_unique<ScopeNode>(nullptr); }
    SrcFile(const std::string& fpath): path(fpath), uniqueName(""), isFinished(false), code() { code = std::make_unique<ScopeNode>(nullptr); }
    SrcFile(const std::string& fpath, const std::string& uname): path(fpath), uniqueName(uname), isFinished(false), code() { code = std::make_unique<ScopeNode>(nullptr); }

    std::string toString() {
        std::string result = std::format("SrcFile {} {}", path, uniqueName);
        if (code) result += "\n" + code->toString(0);
        return result;
    }

    ASTNode* findNodeByName(ASTNodeType tp, const std::string& name, bool checkExported); // find toplevel node by name (include, tmp, var, func, struct, enum)
    std::unique_ptr<TypeNode> parseType(TokenProvider& tp, ScopeNode& current, int arch); // parse type from tokens
};

// parser class
class ASTGen {
    public:
    CompileMessage prt;
    int arch; // target architecture in bytes
    std::vector<std::unique_ptr<SrcFile>> srcFiles;

    ASTGen(): prt(3), arch(8), srcFiles() {}
    ASTGen(int p, int a): prt(p), arch(a), srcFiles() {}

    std::string toString() {
        std::string result = "ASTGen";
        for (auto& src : srcFiles) result += "\n\n\n" + src->toString();
        return result;
    }

    std::string parse(const std::string& path); // returns error message or empty if ok

    private:
    std::string getLocString(const Location& loc) { return std::format("{}:{}", srcFiles[loc.srcLoc]->path, loc.line); }
    int findSource(const std::string& path); // find source file index, -1 if not found
    bool isTypeStart(TokenProvider& tp, SrcFile& src);

    std::unique_ptr<RawCodeNode> parseRawCode(TokenProvider& tp); // parse raw code
    std::unique_ptr<DeclStructNode> parseStruct(TokenProvider& tp, ScopeNode& current, SrcFile& src, bool isExported); // parse struct declaration
    std::unique_ptr<DeclEnumNode> parseEnum(TokenProvider& tp, ScopeNode& current, SrcFile& src, bool isExported); // parse enum declaration
    std::unique_ptr<DeclFuncNode> parseFunc(TokenProvider& tp, ScopeNode& current, SrcFile& src, std::unique_ptr<TypeNode> retType, bool isVaArg, bool isExported); // parse function declaration

    std::unique_ptr<ASTNode> parseAtomicExpr(TokenProvider& tp, ScopeNode& current, SrcFile& src); // parse atomic expression
    std::unique_ptr<ASTNode> parsePrattExpr(TokenProvider& tp, ScopeNode& current, SrcFile& src, int level); // parse pratt expression
    std::unique_ptr<LongStatNode> parseVarDecl(TokenProvider& tp, ScopeNode& current, SrcFile& src, std::unique_ptr<TypeNode> varType, bool isDefine, bool isExtern, bool isExported); // parse variable declaration
    std::unique_ptr<LongStatNode> parseVarAssign(TokenProvider& tp, ScopeNode& current, SrcFile& src, std::unique_ptr<ASTNode> lvalue); // parse variable assignment

    std::unique_ptr<ASTNode> parseStatement(TokenProvider& tp, ScopeNode& current, SrcFile& src); // parse general statement
    std::unique_ptr<ScopeNode> parseScope(TokenProvider& tp, ScopeNode& current, SrcFile& src); // parse scope
    std::unique_ptr<ASTNode> parseTopLevel(TokenProvider& tp, ScopeNode& current, SrcFile& src); // parse toplevel declaration
};

#endif // ASTGEN_H
