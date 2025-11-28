#ifndef CTREE_H
#define CTREE_H

#include <unordered_map>
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

    std::unique_ptr<CTypeNode> clone() {
        std::unique_ptr<CTypeNode> newType = std::make_unique<CTypeNode>();
        newType->objType = objType;
        newType->name = name;
        newType->structLnk = structLnk;
        if (direct != nullptr) {
            newType->direct = direct->clone();
        }
        for (auto& ind : indirect) {
            newType->indirect.push_back(ind->clone());
        }
        newType->length = length;
        newType->typeSize = typeSize;
        newType->typeAlign = typeAlign;
        return newType;
    }
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
    std::string name; // unique name
    int uid; // unique id
    std::unique_ptr<CTypeNode> declType;
    bool isExported;

    CDeclNode(): objType(CDeclType::NONE), location(), name(""), uid(-1), declType(nullptr), isExported(false) {}
    CDeclNode(CDeclType tp): objType(tp), location(), name(""), uid(-1), declType(nullptr), isExported(false) {}
    CDeclNode(CDeclType tp, const std::string& nm): objType(tp), uid(-1), location(), name(nm), declType(nullptr), isExported(false) {}

    virtual ~CDeclNode() {}

    virtual std::unique_ptr<CDeclNode> clone() {
        std::unique_ptr<CDeclNode> newDecl = std::make_unique<CDeclNode>(objType);
        newDecl->location = location;
        newDecl->name = name;
        newDecl->uid = uid;
        if (declType != nullptr) {
            newDecl->declType = declType->clone();
        }
        newDecl->isExported = isExported;
        return newDecl;
    }
};

class CDeclVar : public CDeclNode {
    public:
    Literal init;
    bool isDefine;
    bool isExtern;
    bool isParam;

    CDeclVar(): CDeclNode(CDeclType::VAR), init(), isDefine(false), isExtern(false), isParam(false) {}
    CDeclVar(const std::string& nm): CDeclNode(CDeclType::VAR, nm), init(), isDefine(false), isExtern(false), isParam(false) {}

    virtual std::unique_ptr<CDeclNode> clone() {
        std::unique_ptr<CDeclVar> newDecl = std::make_unique<CDeclVar>();
        newDecl->location = location;
        newDecl->name = name;
        newDecl->uid = uid;
        if (declType != nullptr) {
            newDecl->declType = declType->clone();
        }
        newDecl->init = init;
        newDecl->isDefine = isDefine;
        newDecl->isExtern = isExtern;
        newDecl->isParam = isParam;
        return newDecl;
    }
};

class CDeclFunc : public CDeclNode {
    public:
    std::unique_ptr<CTypeNode> retType;
    std::vector<std::unique_ptr<CTypeNode>> paramTypes;
    std::vector<std::string> paramNames;
    std::unique_ptr<CStatNode> body;

    CDeclFunc(): CDeclNode(CDeclType::FUNC), retType(nullptr), paramTypes(), paramNames(), body(nullptr) {}
    CDeclFunc(const std::string& nm): CDeclNode(CDeclType::FUNC, nm), retType(nullptr), paramTypes(), paramNames(), body(nullptr) {}

    virtual std::unique_ptr<CDeclNode> clone() {
        std::unique_ptr<CDeclFunc> newDecl = std::make_unique<CDeclFunc>();
        newDecl->location = location;
        newDecl->name = name;
        newDecl->uid = uid;
        if (retType != nullptr) {
            newDecl->retType = retType->clone();
        }
        for (auto& type : paramTypes) {
            newDecl->paramTypes.push_back(type->clone());
        }
        for (auto& nm : paramNames) {
            newDecl->paramNames.push_back(nm);
        }
        if (body != nullptr) {
            newDecl->body = body->clone();
        }
        return newDecl;
    }
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

    virtual std::unique_ptr<CDeclNode> clone() {
        std::unique_ptr<CDeclStruct> newDecl = std::make_unique<CDeclStruct>();
        newDecl->location = location;
        newDecl->name = name;
        newDecl->uid = uid;
        if (declType != nullptr) {
            newDecl->declType = declType->clone();
        }
        newDecl->structSize = structSize;
        newDecl->structAlign = structAlign;
        for (auto& type : memTypes) {
            newDecl->memTypes.push_back(type->clone());
        }
        for (auto& nm : memNames) {
            newDecl->memNames.push_back(nm);
        }
        for (auto& offset : memOffsets) {
            newDecl->memOffsets.push_back(offset);
        }
        return newDecl;
    }
};

class CDeclEnum : public CDeclNode {
    public:
    int enumSize; // total size in bytes
    std::vector<std::string> memNames;
    std::vector<int64_t> memValues;

    CDeclEnum(): CDeclNode(CDeclType::ENUM), enumSize(-1), memNames(), memValues() {}
    CDeclEnum(const std::string& nm): CDeclNode(CDeclType::ENUM, nm), enumSize(-1), memNames(), memValues() {}

    virtual std::unique_ptr<CDeclNode> clone() {
        std::unique_ptr<CDeclEnum> newDecl = std::make_unique<CDeclEnum>();
        newDecl->location = location;
        newDecl->name = name;
        newDecl->uid = uid;
        if (declType != nullptr) {
            newDecl->declType = declType->clone();
        }
        newDecl->enumSize = enumSize;
        for (auto& nm : memNames) {
            newDecl->memNames.push_back(nm);
        }
        for (auto& value : memValues) {
            newDecl->memValues.push_back(value);
        }
        return newDecl;
    }
};

// compile tree expression node
enum class CExprType {
    NONE,
    LITERAL,
    LITERAL_KEY,
    LITERAL_ARRAY,
    VAR_USE,
    FUNC_USE,
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

    virtual std::unique_ptr<CExprNode> clone() {
        std::unique_ptr<CExprNode> newExpr = std::make_unique<CExprNode>(objType);
        newExpr->location = location;
        if (exprType != nullptr) {
            newExpr->exprType = exprType->clone();
        }
        newExpr->isLValue = isLValue;
        return newExpr;
    }
};

class CExprLiteral : public CExprNode {
    public:
    Literal literal; // literal
    std::vector<std::unique_ptr<CExprNode>> elements; // literal array
    std::string word; // literal keyword

    CExprLiteral(): CExprNode(), literal(), elements(), word() {}

    virtual std::unique_ptr<CExprNode> clone() {
        std::unique_ptr<CExprLiteral> newExpr = std::make_unique<CExprLiteral>();
        newExpr->location = location;
        newExpr->literal = literal;
        for (auto& element : elements) {
            newExpr->elements.push_back(element->clone());
        }
        newExpr->word = word;
        return newExpr;
    }
};

class CExprUse : public CExprNode {
    public:
    CDeclVar* var;
    CDeclFunc* func;

    CExprUse(): CExprNode(CExprType::NONE), var(nullptr), func(nullptr) {}

    virtual std::unique_ptr<CExprNode> clone() { // pointer cloning is not supported
        std::unique_ptr<CExprUse> newExpr = std::make_unique<CExprUse>();
        newExpr->location = location;
        if (var != nullptr) {
            newExpr->var = var;
        }
        if (func != nullptr) {
            newExpr->func = func;
        }
        return newExpr;
    }
};

class CExprCall : public CExprNode {
    public:
    CDeclFunc* func;
    std::vector<std::unique_ptr<CExprNode>> args;

    CExprCall(): CExprNode(CExprType::FUNC_CALL), func(nullptr), args() {}

    virtual std::unique_ptr<CExprNode> clone() { // pointer cloning is not supported
        std::unique_ptr<CExprCall> newExpr = std::make_unique<CExprCall>();
        newExpr->location = location;
        if (func != nullptr) {
            newExpr->func = func;
        }
        for (auto& arg : args) {
            newExpr->args.push_back(arg->clone());
        }
        return newExpr;
    }
};

class CExprOp : public CExprNode {
    public:
    std::unique_ptr<CExprNode> left;
    std::unique_ptr<CExprNode> right;
    int bytePos; // byte offset for dot & arrow
    int idxPos; // index offset for dot & arrow

    CExprOp(): CExprNode(), left(nullptr), right(nullptr), bytePos(-1), idxPos(-1) {}

    virtual std::unique_ptr<CExprNode> clone() {
        std::unique_ptr<CExprOp> newExpr = std::make_unique<CExprOp>();
        newExpr->location = location;
        if (left != nullptr) {
            newExpr->left = left->clone();
        }
        if (right != nullptr) {
            newExpr->right = right->clone();
        }
        newExpr->bytePos = bytePos;
        newExpr->idxPos = idxPos;
        return newExpr;
    }
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

    virtual std::unique_ptr<CStatNode> clone() {
        std::unique_ptr<CStatNode> newStat = std::make_unique<CStatNode>(objType);
        newStat->location = location;
        if (statType != nullptr) {
            newStat->statType = statType->clone();
        }
        newStat->isReturnable = isReturnable;
        return newStat;
    }
};

class CStatRaw : public CStatNode {
    public:
    std::string code; // raw code

    CStatRaw(): CStatNode(), code("") {}

    virtual std::unique_ptr<CStatNode> clone() {
        std::unique_ptr<CStatRaw> newStat = std::make_unique<CStatRaw>();
        newStat->location = location;
        newStat->code = code;
        return newStat;
    }
};

class CStatCtrl : public CStatNode {
    public:
    std::unique_ptr<CExprNode> expr; // return value
    CStatWhile* target; // target for break & continue

    CStatCtrl(): CStatNode(), expr(nullptr), target(nullptr) {}

    virtual std::unique_ptr<CStatNode> clone() { // pointer cloning is not supported
        std::unique_ptr<CStatCtrl> newStat = std::make_unique<CStatCtrl>();
        newStat->location = location;
        if (expr != nullptr) {
            newStat->expr = expr->clone();
        }
        if (target != nullptr) {
            newStat->target = target;
        }
        return newStat;
    }
};

class CStatExpr : public CStatNode {
    public:
    std::unique_ptr<CExprNode> expr;

    CStatExpr(): CStatNode(CStatType::EXPR), expr(nullptr) {}

    virtual std::unique_ptr<CStatNode> clone() {
        std::unique_ptr<CStatExpr> newStat = std::make_unique<CStatExpr>();
        newStat->location = location;
        if (expr != nullptr) {
            newStat->expr = expr->clone();
        }
        return newStat;
    }
};

class CStatAssign : public CStatNode {
    public:
    std::unique_ptr<CExprNode> left;
    std::unique_ptr<CExprNode> right;

    CStatAssign(): CStatNode(CStatType::ASSIGN), left(nullptr), right(nullptr) {}

    virtual std::unique_ptr<CStatNode> clone() {
        std::unique_ptr<CStatAssign> newStat = std::make_unique<CStatAssign>();
        newStat->location = location;
        if (left != nullptr) {
            newStat->left = left->clone();
        }
        if (right != nullptr) {
            newStat->right = right->clone();
        }
        return newStat;
    }
};

class CStatScope : public CStatNode {
    public:
    std::vector<std::unique_ptr<CDeclNode>> decls;
    std::vector<std::unique_ptr<CStatNode>> stats;

    CStatScope(): CStatNode(CStatType::SCOPE), decls(), stats() {}
    CStatScope(): CStatNode(CStatType::SCOPE), decls(), stats() {}

    virtual std::unique_ptr<CStatNode> clone() {
        std::unique_ptr<CStatScope> newStat = std::make_unique<CStatScope>();
        newStat->location = location;
        for (auto& decl : decls) {
            newStat->decls.push_back(decl->clone());
        }
        for (auto& stat : stats) {
            newStat->stats.push_back(stat->clone());
        }
        return newStat;
    }
};

class CStatIf : public CStatNode {
    public:
    std::unique_ptr<CExprNode> cond;
    std::unique_ptr<CStatNode> ifStat;
    std::unique_ptr<CStatNode> elseStat;

    CStatIf(): CStatNode(CStatType::IF), cond(nullptr), ifStat(nullptr), elseStat(nullptr) {}

    virtual std::unique_ptr<CStatNode> clone() {
        std::unique_ptr<CStatIf> newStat = std::make_unique<CStatIf>();
        newStat->location = location;
        if (cond != nullptr) {
            newStat->cond = cond->clone();
        }
        if (ifStat != nullptr) {
            newStat->ifStat = ifStat->clone();
        }
        if (elseStat != nullptr) {
            newStat->elseStat = elseStat->clone();
        }
        return newStat;
    }
};

class CStatWhile : public CStatNode {
    public:
    std::unique_ptr<CExprNode> cond;
    std::unique_ptr<CStatNode> stat;
    bool isTgtBreak;
    bool isTgtContinue;

    CStatWhile(): CStatNode(CStatType::WHILE), cond(nullptr), stat(nullptr), isTgtBreak(false), isTgtContinue(false) {}

    virtual std::unique_ptr<CStatNode> clone() {
        std::unique_ptr<CStatWhile> newStat = std::make_unique<CStatWhile>();
        newStat->location = location;
        if (cond != nullptr) {
            newStat->cond = cond->clone();
        }
        if (stat != nullptr) {
            newStat->stat = stat->clone();
        }
        newStat->isTgtBreak = isTgtBreak;
        newStat->isTgtContinue = isTgtContinue;
        return newStat;
    }
};

class CStatSwitch : public CStatNode {
    public:
    std::unique_ptr<CExprNode> cond;
    std::vector<int64_t> caseConds;
    std::vector<bool> caseFalls;
    std::vector<std::vector<std::unique_ptr<CStatNode>>> caseBodies;
    std::vector<std::unique_ptr<CStatNode>> defaultBody;

    CStatSwitch(): CStatNode(CStatType::SWITCH), cond(nullptr), caseConds(), caseFalls(), caseBodies(), defaultBody() {}

    virtual std::unique_ptr<CStatNode> clone() {
        std::unique_ptr<CStatSwitch> newStat = std::make_unique<CStatSwitch>();
        newStat->location = location;
        if (cond != nullptr) {
            newStat->cond = cond->clone();
        }
        newStat->caseConds = caseConds;
        newStat->caseFalls = caseFalls;
        for (auto& body : caseBodies) {
            std::vector<std::unique_ptr<CStatNode>> newBody;
            for (auto& stat : body) {
                newBody.push_back(stat->clone());
            }
            newStat->caseBodies.push_back(newBody);
        }
        for (auto& stat : defaultBody) {
            newStat->defaultBody.push_back(stat->clone());
        }
        return newStat;
    }
};

// chunk of ctree nodes
class CModule {
    public:
    std::string path; // for debug
    int appendIdx; // for linking index
    std::vector<std::unique_ptr<CDeclNode>> decls;
    std::vector<std::unique_ptr<CStatNode>> stats;
    std::unordered_map<std::string, CDeclNode*> nameMap; // map for name mangling

    CModule(): path(""), appendIdx(-1), decls(), stats(), nameMap() {}
    CModule(const std::string& p, int i): path(p), appendIdx(i), decls(), stats(), nameMap() {}
};

// compile tree generator
class IncludeInfo {
    public:
    std::string name;
    std::string path;
    std::vector<int> tmpSizes;
    std::vector<int> tmpAligns;

    IncludeInfo(): name(""), path(""), tmpSizes(), tmpAligns() {}
};

class ScopeInfo {
    public:
    int64_t uid; // unique id corresponding to CStatNode
    std::unordered_map<std::string, CDeclVar*> nameMap; // local var name map
    std::vector<std::unique_ptr<CStatExpr>> defers; // deferred statements

    ScopeInfo(): uid(-1), nameMap(), defers() {}
};

class CTreeGen {
    public:
    CompileMessage prt;
    int arch; // target architecture in bytes

    // global convertion context
    int64_t uidCount;
    std::vector<std::unique_ptr<CModule>> modules;
    std::unordered_map<std::string, int> idxMap; // append index map

    // local convertion context
    std::vector<IncludeInfo> includes; // include info
    std::vector<ScopeInfo> scopes; // scope info

    CTreeGen(): prt(3), arch(8), uidCount(0), modules(), idxMap(), includes(), scopes() {}
};

#endif