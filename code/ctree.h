#ifndef CTREE_H
#define CTREE_H

#include "astgen.h"

// compile tree type node
enum class CTypeType {
    NONE,
    PRIMITIVE,
    POINTER,
    ARRAY,
    SLICE,
    FUNCTION,
    STRUCT,
    L_INT, L_FLOAT // literal type or logical operation result
};

class CTypeNode {
    public:
    CTypeType objType;
    std::string name;
    CDeclStruct* structLnk; // link to struct declaration
    std::unique_ptr<CTypeNode> direct; // ptr, arr, slice target & func return
    std::vector<std::unique_ptr<CTypeNode>> indirect; // func args
    int64_t length; // array length
    int typeSize; // total size in bytes
    int typeAlign; // align requirement in bytes

    CTypeNode(): objType(CTypeType::NONE), name(""), structLnk(nullptr), direct(nullptr), indirect(), length(-1), typeSize(-1), typeAlign(-1) {}
};

// compile tree declaration node
enum class CDeclType {
    NONE,
    VAR,
    FUNC,
    STRUCT,
    ENUM
};

class CDeclNode {
    public:
    CDeclType objType;
    Location location;
    std::string name;
    int uid; // unique id
    std::unique_ptr<CTypeNode> declType;
    bool isExported;

    CDeclNode(): objType(CDeclType::NONE), location(), name(""), uid(-1), declType(nullptr), isExported(false) {}
    CDeclNode(CDeclType tp): objType(tp), location(), name(""), uid(-1), declType(nullptr), isExported(false) {}
    CDeclNode(CDeclType tp, const std::string& nm): objType(tp), uid(-1), location(), name(nm), declType(nullptr), isExported(false) {}

    virtual ~CDeclNode() {}
};

class CDeclVar : public CDeclNode {
    public:
    Literal init;
    bool isDefine;
    bool isExtern;
    bool isParam;

    CDeclVar(): CDeclNode(CDeclType::VAR), init(), isDefine(false), isExtern(false), isParam(false) {}
    CDeclVar(const std::string& nm): CDeclNode(CDeclType::VAR, nm), init(), isDefine(false), isExtern(false), isParam(false) {}
};

class CDeclFunc : public CDeclNode {
    public:
    std::unique_ptr<CTypeNode> retType;
    std::vector<std::unique_ptr<CTypeNode>> paramTypes;
    std::vector<std::string> paramNames;
    std::unique_ptr<CStatNode> body;

    CDeclFunc(): CDeclNode(CDeclType::FUNC), retType(nullptr), paramTypes(), paramNames(), body(nullptr) {}
    CDeclFunc(const std::string& nm): CDeclNode(CDeclType::FUNC, nm), retType(nullptr), paramTypes(), paramNames(), body(nullptr) {}
};

class CDeclStruct : public CDeclNode {
    public:
    int structSize; // total size in bytes
    int structAlign; // align requirement in bytes
    std::vector<std::unique_ptr<CTypeNode>> memTypes;
    std::vector<std::string> memNames;
    std::vector<int> memOffsets;

    CDeclStruct(): CDeclNode(CDeclType::STRUCT), structSize(-1), structAlign(-1), memTypes(), memNames(), memOffsets() {}
    CDeclStruct(const std::string& nm): CDeclNode(CDeclType::STRUCT, nm), structSize(-1), structAlign(-1), memTypes(), memNames(), memOffsets() {}
};

class CDeclEnum : public CDeclNode {
    public:
    int enumSize; // total size in bytes
    std::vector<std::string> memNames;
    std::vector<int64_t> memValues;

    CDeclEnum(): CDeclNode(CDeclType::ENUM), enumSize(-1), memNames(), memValues() {}
    CDeclEnum(const std::string& nm): CDeclNode(CDeclType::ENUM, nm), enumSize(-1), memNames(), memValues() {}
};

// compile tree expression node
enum class CExprType {
    NONE,
    LITERAL,
    LITERAL_KEY,
    LITERAL_ARRAY,
    VAR_USE,
    FUNC_CALL,
    // operations
    B_DOT, B_ARROW, B_INDEX,
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

class CExprNode {
    public:
    CExprType objType;
    Location location;
    std::unique_ptr<CTypeNode> exprType;
    bool isLValue;

    CExprNode(): objType(CExprType::NONE), location(), exprType(nullptr), isLValue(false) {}
    CExprNode(CExprType tp): objType(tp), location(), exprType(nullptr), isLValue(false) {}

    virtual ~CExprNode() {}
};

class CExprLiteral : public CExprNode {
    public:
    Literal literal; // literal
    std::vector<std::unique_ptr<CExprNode>> elements; // literal array
    std::string word; // literal keyword

    CExprLiteral(): CExprNode(), literal(), elements(), word() {}
};

class CExprVar : public CExprNode {
    public:
    CDeclVar* var;

    CExprVar(): CExprNode(CExprType::VAR_USE), var(nullptr) {}
};

class CExprFunc : public CExprNode {
    public:
    CDeclFunc* func;
    std::vector<std::unique_ptr<CExprNode>> args;

    CExprFunc(): CExprNode(CExprType::FUNC_CALL), func(nullptr), args() {}
};

class CExprOp : public CExprNode {
    public:
    std::unique_ptr<CExprNode> left;
    std::unique_ptr<CExprNode> right;
    int bytePos; // byte offset for dot & arrow
    int idxPos; // index offset for dot & arrow

    CExprOp(): CExprNode(), left(nullptr), right(nullptr), bytePos(-1), idxPos(-1) {}
};

// compile tree statement node
enum class CStatType {
    NONE,
    RAW_C,
    RAW_IR,
    BREAK,
    CONTINUE,
    RETURN,
    EXPR,
    ASSIGN,
    SCOPE,
    IF,
    WHILE,
    SWITCH
};

class CStatNode {
    public:
    CStatType objType;
    Location location;
    int uid; // unique id
    std::unique_ptr<CTypeNode> statType;
    bool isReturnable;

    CStatNode(): objType(CStatType::NONE), location(), uid(-1), statType(nullptr), isReturnable(false) {}
    CStatNode(CStatType tp): objType(tp), location(), uid(-1), statType(nullptr), isReturnable(false) {}

    virtual ~CStatNode() {}
};

class CStatRaw : public CStatNode {
    public:
    std::string code; // raw code

    CStatRaw(): CStatNode(), code("") {}
};

class CStatCtrl : public CStatNode {
    public:
    std::unique_ptr<CExprNode> expr; // return value
    CStatWhile* target; // target for break & continue

    CStatCtrl(): CStatNode(), expr(nullptr), target(nullptr) {}
};

class CStatExpr : public CStatNode {
    public:
    std::unique_ptr<CExprNode> expr;

    CStatExpr(): CStatNode(CStatType::EXPR), expr(nullptr) {}
};

class CStatAssign : public CStatNode {
    public:
    std::unique_ptr<CExprNode> left;
    std::unique_ptr<CExprNode> right;

    CStatAssign(): CStatNode(CStatType::ASSIGN), left(nullptr), right(nullptr) {}
};

class CStatScope : public CStatNode {
    public:
    std::vector<std::unique_ptr<CDeclNode>> decls;
    std::vector<std::unique_ptr<CStatNode>> stats;

    CStatScope(): CStatNode(CStatType::SCOPE), decls(), stats() {}
    CStatScope(): CStatNode(CStatType::SCOPE), decls(), stats() {}
};

class CStatIf : public CStatNode {
    public:
    std::unique_ptr<CExprNode> cond;
    std::unique_ptr<CStatNode> ifStat;
    std::unique_ptr<CStatNode> elseStat;

    CStatIf(): CStatNode(CStatType::IF), cond(nullptr), ifStat(nullptr), elseStat(nullptr) {}
};

class CStatWhile : public CStatNode {
    public:
    std::unique_ptr<CExprNode> cond;
    std::unique_ptr<CStatNode> stat;
    bool isTgtBreak;
    bool isTgtContinue;

    CStatWhile(): CStatNode(CStatType::WHILE), cond(nullptr), stat(nullptr), isTgtBreak(false), isTgtContinue(false) {}
};

class CStatSwitch : public CStatNode {
    public:
    std::unique_ptr<CExprNode> cond;
    std::vector<int64_t> caseConds;
    std::vector<bool> caseFalls;
    std::vector<std::vector<std::unique_ptr<CStatNode>>> caseBodies;
    std::vector<std::unique_ptr<CStatNode>> defaultBody;

    CStatSwitch(): CStatNode(CStatType::SWITCH), cond(nullptr), caseConds(), caseFalls(), caseBodies(), defaultBody() {}
};



#endif