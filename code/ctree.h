#ifndef C_TREE_H
#define C_TREE_H

#include <unordered_map>
#include "astgen.h"

// forward declarations
class CDeclStruct;
class CDeclVar;
class CDeclFunc;
class CStatScope;

// compile tree type node
enum class CTypeType {
    NONE,
    LITERAL, // autocastable primitive
    PRIMITIVE,
    POINTER,
    ARRAY,
    SLICE,
    FUNCTION,
    STRUCT,
    DATA // chunk of bytes
};

class CTypeNode {
    public:
    CTypeType objType;
    std::string name;
    std::unique_ptr<CTypeNode> direct; // ptr, arr, slice target & func return
    std::vector<std::unique_ptr<CTypeNode>> indirect; // func args
    int64_t length; // array length
    int typeSize; // total size in bytes
    int typeAlign; // align requirement in bytes
    CDeclStruct* structLnk; // struct link

    CTypeNode(): objType(CTypeType::NONE), name(""), direct(nullptr), indirect(), length(-1), typeSize(-1), typeAlign(-1), structLnk(nullptr) {}
    CTypeNode(CTypeType tp, const std::string& nm): objType(tp), name(nm), direct(nullptr), indirect(), length(-1), typeSize(-1), typeAlign(-1), structLnk(nullptr) {}
};

// compile tree expression node
enum class CExprType {
    NONE,
    LITERAL,
    LITERAL_DATA,
    SYN,
    VAR_USE,
    FUNC_USE,
    FUNC_CALL,
    // operations
    B_DOT, B_ARROW, B_INDEX,
    U_PLUS, U_MINUS, U_LOGIC_NOT, U_BIT_NOT, U_REF, U_DEREF,
    B_MUL, B_DIV, B_MOD, B_ADD, B_SUB, B_SHL, B_SHR,
    B_LT, B_LE, B_GT, B_GE, B_EQ, B_NE,
    B_BIT_AND, B_BIT_XOR, B_BIT_OR, B_LOGIC_AND, B_LOGIC_OR,
    // integrated func
    SIZEOF, CAST, MAKE, LEN
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

// compile tree statement node
enum class CStatType {
    NONE,
    RAW_C,
    RAW_IR,
    JMP_BREAK,
    JMP_CONTINUE,
    JMP_RETURN,
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
    bool isReturnable;

    CStatNode(): objType(CStatType::NONE), location(), uid(-1), isReturnable(false) {}
    CStatNode(CStatType tp): objType(tp), location(), uid(-1), isReturnable(false) {}

    virtual ~CStatNode() {}
};

// compile tree declaration node
enum class CDeclType {
    NONE,
    RAW,
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
};

// compile tree expression node implementation
class CExprLiteral : public CExprNode { // single literal
    public:
    Literal literal;

    CExprLiteral(): CExprNode(CExprType::LITERAL), literal() {}
    CExprLiteral(const Literal& lit): CExprNode(CExprType::LITERAL), literal(lit) {}
};

class CExprLiteralData : public CExprNode { // literal array or struct
    public:
    std::vector<std::unique_ptr<CExprNode>> elements;

    CExprLiteralData(): CExprNode(CExprType::LITERAL_DATA), elements() {}
};

class CExprSyn : public CExprNode { // expression requires multiple statements
    public:
    std::vector<std::unique_ptr<CStatNode>> preCond;
    std::unique_ptr<CExprNode> mainCond;

    CExprSyn(): CExprNode(CExprType::SYN), preCond(), mainCond(nullptr) {}
};

class CExprVarUse : public CExprNode { // variable name use
    public:
    CDeclVar* tgt;

    CExprVarUse(): CExprNode(CExprType::VAR_USE), tgt(nullptr) {}
    CExprVarUse(CDeclVar* var): CExprNode(CExprType::VAR_USE), tgt(var) {}
};

class CExprFuncUse : public CExprNode { // function name use
    public:
    CDeclFunc* tgt;

    CExprFuncUse(): CExprNode(CExprType::FUNC_USE), tgt(nullptr) {}
    CExprFuncUse(CDeclFunc* func): CExprNode(CExprType::FUNC_USE), tgt(func) {}
};

class CExprFuncCall : public CExprNode { // function call
    public:
    CDeclFunc* tgt;
    std::vector<std::unique_ptr<CExprNode>> args;

    CExprFuncCall(): CExprNode(CExprType::FUNC_CALL), tgt(nullptr) {}
    CExprFuncCall(CDeclFunc* func): CExprNode(CExprType::FUNC_CALL), tgt(func) {}
};

class CExprOper : public CExprNode { // operation
    public:
    CExprNode* left;
    CExprNode* right;
    int nameIdx; // for struct member access
    int memIdx; // for struct member access

    CExprOper(): CExprNode(CExprType::NONE), left(nullptr), right(nullptr), nameIdx(-1), memIdx(-1) {}
    CExprOper(CExprNode* expr, CExprType tp): CExprNode(tp), left(expr), right(nullptr), nameIdx(-1), memIdx(-1) {}
};

class CExprIFunc : public CExprNode { // integrated function
    public:
    CExprNode* left;
    CExprNode* right;
    std::unique_ptr<CTypeNode> castType; // for sizeof, cast

    CExprIFunc(): CExprNode(CExprType::NONE), left(nullptr), right(nullptr), castType(nullptr) {}
    CExprIFunc(CExprNode* expr, CExprType tp): CExprNode(tp), left(expr), right(nullptr), castType(nullptr) {}
};

// compile tree statement node implementation
class CStatRaw : public CStatNode { // raw C/IR code
    public:
    std::string raw;

    CStatRaw(): CStatNode(CStatType::RAW_C), raw(){}
    CStatRaw(bool isC, const std::string& str): CStatNode(isC ? CStatType::RAW_C : CStatType::RAW_IR), raw(str) {}
};

class CStatJump : public CStatNode { // goto for break, continue, return
    public:
    CStatScope* tgt;

    CStatJump(): CStatNode(CStatType::NONE), tgt(nullptr) {}
    CStatJump(CStatScope* scope, CStatType tp): CStatNode(tp), tgt(scope) {}
};

class CStatExpr : public CStatNode { // expression
    public:
    CExprNode* expr;

    CStatExpr(): CStatNode(CStatType::EXPR), expr(nullptr) {}
};

class CStatScope : public CStatNode { // scope
    public:
    std::vector<std::unique_ptr<CStatNode>> stats;
    std::vector<std::unique_ptr<CStatExpr>> defers; // clean up statements
    bool useStartLbl;
    bool useEndLbl;

    CStatScope(): CStatNode(CStatType::SCOPE), stats(), defers(), useStartLbl(false), useEndLbl(false) {}
};

class CStatIf : public CStatNode { // if
    public:
    CExprNode* cond;
    std::unique_ptr<CStatNode> ifBody;
    std::unique_ptr<CStatNode> elseBody;

    CStatIf(): CStatNode(CStatType::IF), cond(nullptr), ifBody(nullptr), elseBody(nullptr) {}
};

class CStatWhile : public CStatNode { // while
    public:
    CExprNode* cond;
    std::unique_ptr<CStatNode> body;

    CStatWhile(): CStatNode(CStatType::WHILE), cond(nullptr), body(nullptr) {}
};

class CStatSwitch : public CStatNode { // switch
    public:
    CExprNode* cond;
    std::vector<int64_t> caseCond;
    std::vector<std::unique_ptr<CStatNode>> caseBody;
    std::unique_ptr<CStatNode> defaultBody;
    std::vector<bool> caseBreak;

    CStatSwitch(): CStatNode(CStatType::SWITCH), cond(nullptr), caseBody(), defaultBody(nullptr), caseBreak() {}
};

// compile tree declaration node implementation
class CDeclRaw : public CDeclNode { // raw C/IR code
    public:
    CStatRaw raw;

    CDeclRaw(): CDeclNode(CDeclType::RAW), raw() {}
};

class CDeclVar : public CDeclNode { // variable
    public:
    Literal init;
    bool isDefine;
    bool isExtern;
    bool isParam;

    CDeclVar(): CDeclNode(CDeclType::VAR), init(), isDefine(false), isExtern(false), isParam(false) {}
};

class CDeclFunc : public CDeclNode { // function
    public:
    std::unique_ptr<CTypeNode> retType;
    std::vector<std::unique_ptr<CTypeNode>> paramTypes;
    std::vector<std::string> paramNames;
    std::unique_ptr<CStatNode> body;
    
    CDeclFunc(): CDeclNode(CDeclType::FUNC), retType(nullptr), paramTypes(), paramNames(), body(nullptr) {}
};

class CDeclStruct : public CDeclNode { // struct
    public:
    int structSize; // total size in bytes
    int structAlign; // align requirement in bytes
    std::vector<std::unique_ptr<CTypeNode>> memTypes;
    std::vector<std::string> memNames;
    std::vector<int> memOffsets;

    CDeclStruct(): CDeclNode(CDeclType::STRUCT), structSize(-1), structAlign(-1), memTypes(), memNames(), memOffsets() {}
    CDeclStruct(const std::string& nm): CDeclNode(CDeclType::STRUCT, nm), structSize(-1), structAlign(-1), memTypes(), memNames(), memOffsets() {}
};

class CDeclEnum : public CDeclNode { // enum
    public:
    int enumSize; // total size in bytes
    std::vector<std::string> memNames;
    std::vector<int64_t> memValues;

    CDeclEnum(): CDeclNode(CDeclType::ENUM), enumSize(-1), memNames(), memValues() {}
    CDeclEnum(const std::string& nm): CDeclNode(CDeclType::ENUM, nm), enumSize(-1), memNames(), memValues() {}
};

// single module of source
class CModule {
    public:
    std::string path; // debug path
    std::string uname; // debug unique name
    int appendIdx; // for linking index
    std::vector<std::unique_ptr<CDeclNode>> decls;
    std::unordered_map<std::string, CDeclNode*> nameMap; // map for name mangling

    CModule(): path(""), uname(""), appendIdx(-1), decls(), nameMap() {}
    CModule(const std::string& p, const std::string& u, int i): path(p), uname(u), appendIdx(i), decls(), nameMap() {}
};

// compile tree generator context info
class IncludeInfo {
    public:
    std::string path;
    std::string uname;
    std::vector<std::unique_ptr<CTypeNode>> args;

    IncludeInfo(): path(""), uname(""), args() {}
};

class ScopeInfo {
    public:
    int64_t uid; // unique id corresponding to CStatNode
    CStatScope* scope;
    std::unordered_map<std::string, CDeclVar*> nameMap; // local var name map

    ScopeInfo(): uid(-1), scope(nullptr), nameMap() {}
    ScopeInfo(CStatScope* s): uid(s->uid), scope(s), nameMap() {}
};

// compile tree generator
class CTreeGen {
    public:
    CompileMessage prt;
    int arch; // target architecture in bytes
    std::vector<std::unique_ptr<CModule>> modules;

    // global convertion context
    int64_t uidCount;
    std::unordered_map<std::string, int> idxMap;

    // local convertion context
    std::vector<IncludeInfo> includes;
    std::vector<ScopeInfo> scopes;
    std::vector<CStatWhile*> loops;
    CModule* curModule;
    CDeclFunc* curFunc;

    CTreeGen(): prt(3), arch(8), uidCount(0), modules(), idxMap(), includes(), scopes(), loops(), curModule(nullptr), curFunc(nullptr) {}
};

#endif // C_TREE_H