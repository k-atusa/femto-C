#ifndef AST2_H
#define AST2_H

#include <algorithm>
#include <unordered_map>
#include "ast1ext.h"

// forward declarations
class A2Type;
class A2StatScope;
class A2StatLoop;
bool isTypeEqual(A2Type* a, A2Type* b);

// AST2 type node
enum class A2TypeType {
    NONE,
    PRIMITIVE,
    POINTER,
    ARRAY,
    SLICE,
    FUNCTION,
    STRUCT,
    ENUM
};

class A2Type {
    public:
    A2TypeType objType;
    Location location;
    std::string name;
    std::string modUname; // uname of type module
    std::unique_ptr<A2Type> direct; // ptr, arr, slice target & func return
    std::vector<std::unique_ptr<A2Type>> indirect; // func args
    int64_t arrLen; // array length
    int typeSize; // total size in bytes
    int typeAlign; // align requirement in bytes
    
    A2Type(): objType(A2TypeType::NONE), location(), name(""), modUname(""), direct(nullptr), indirect(), arrLen(-1), typeSize(-1), typeAlign(-1) {}
    A2Type(A2TypeType tp, const std::string& nm): objType(tp), location(), name(nm), modUname(""), direct(nullptr), indirect(), arrLen(-1), typeSize(-1), typeAlign(-1) {}
    A2Type(A2TypeType tp, const std::string& modNm, const std::string& tpNm): objType(tp), location(), name(tpNm), modUname(modNm), direct(nullptr), indirect(), arrLen(-1), typeSize(-1), typeAlign(-1) {}

    std::unique_ptr<A2Type> Clone() {
        std::unique_ptr<A2Type> newType = std::make_unique<A2Type>();
        newType->objType = objType;
        newType->location = location;
        newType->name = name;
        newType->modUname = modUname;
        if (direct != nullptr) {
            newType->direct = direct->Clone();
        }
        for (auto& ind : indirect) {
            newType->indirect.push_back(ind->Clone());
        }
        newType->arrLen = arrLen;
        newType->typeSize = typeSize;
        newType->typeAlign = typeAlign;
        return newType;
    }

    std::string toString() {
        switch (objType) {
            case A2TypeType::PRIMITIVE: case A2TypeType::STRUCT: case A2TypeType::ENUM:
                return name;
            case A2TypeType::POINTER: case A2TypeType::ARRAY: case A2TypeType::SLICE:
                return direct->toString() + name;
            case A2TypeType::FUNCTION:
            {
                std::string res = direct->toString() + "(";
                for (int i = 0; i < indirect.size(); i++) {
                    if (i > 0) res += ",";
                    res += indirect[i]->toString();
                }
                res += ")";
                return res;
            }
            default:
                return "unknown";
        }
    }

    std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("A2Type {} {} {} {} {} {}", (int)objType, name, modUname, arrLen, typeSize, typeAlign);
        if (direct) result += "\n" + direct->toString(indent + 1);
        for (auto& ind : indirect) {
            result += "\n" + ind->toString(indent + 1);
        }
        return result;
    }
};

// AST2 expression node
enum class A2ExprType {
    NONE,
    LITERAL,
    LITERAL_DATA,
    OPERATION,
    VAR_NAME,
    FUNC_NAME,
    STRUCT_NAME, // for inc.struct
    ENUM_NAME, // for inc.enum
    FUNC_CALL,
    FPTR_CALL
};

class A2Expr {
    public:
    A2ExprType objType;
    Location location;
    A2Type* exprType;
    bool isLvalue;
    bool isConst;

    A2Expr(): objType(A2ExprType::NONE), location(), exprType(nullptr), isLvalue(false), isConst(false) {}
    A2Expr(A2ExprType t): objType(t), location(), exprType(nullptr), isLvalue(false), isConst(false) {}
    virtual ~A2Expr() = default;

    virtual std::string toString(int indent) { return std::string(indent * 2, ' ') + std::format("A2Expr {}", (int)objType); }
};

// AST2 statement node
enum class A2StatType {
    NONE,
    RAW_C,
    RAW_IR,
    EXPR,
    DECL,
    ASSIGN,
    ASSIGN_ADD,
    ASSIGN_SUB,
    ASSIGN_MUL,
    ASSIGN_DIV,
    ASSIGN_MOD,
    RETURN,
    BREAK,
    CONTINUE,
    SCOPE,
    IF,
    LOOP,
    SWITCH
};

class A2Stat {
    public:
    A2StatType objType;
    Location location;
    int64_t uid;
    bool isReturnable;

    A2Stat(): objType(A2StatType::NONE), location(), uid(0), isReturnable(false) {}
    A2Stat(A2StatType t): objType(t), location(), uid(0), isReturnable(false) {}
    virtual ~A2Stat() = default;

    virtual std::string toString(int indent) { return std::string(indent * 2, ' ') + std::format("A2Stat {}", (int)objType); }
};

// AST2 declaration node
enum class A2DeclType {
    NONE,
    RAW_C,
    RAW_IR,
    VAR,
    FUNC,
    STRUCT,
    ENUM
};

class A2Decl {
    public:
    A2DeclType objType;
    Location location;
    std::string modUname;
    std::string name; // declaration name
    int64_t uid;
    std::unique_ptr<A2Type> type; // declaration type
    bool isExported;

    A2Decl(): objType(A2DeclType::NONE), location(), modUname(), name(), uid(0), type(nullptr), isExported(false) {}
    A2Decl(A2DeclType t): objType(t), location(), modUname(), name(), uid(0), type(nullptr), isExported(false) {}
    A2Decl(A2DeclType t, std::string nm): objType(t), location(), modUname(), name(nm), uid(0), type(nullptr), isExported(false) {}
    virtual ~A2Decl() = default;

    virtual std::string toString(int indent) { 
        std::string result = std::string(indent * 2, ' ') + std::format("A2Decl {} {}.{}", (int)objType, modUname, name);
        if (type) result += "\n" + type->toString(indent + 1);
        return result;
    }
};

// AST2 expression node implementation
class A2ExprLiteral : public A2Expr { // literal value
    public:
    Literal value;

    A2ExprLiteral(): A2Expr(A2ExprType::LITERAL), value() {}
    A2ExprLiteral(Literal v): A2Expr(A2ExprType::LITERAL), value(v) {}
    ~A2ExprLiteral() override = default;

    std::string toString(int indent) override { return std::string(indent * 2, ' ') + std::format("A2ExprLiteral {}", value.toString()); }
};

class A2ExprLiteralData : public A2Expr { // literal data chunk
    public:
    std::vector<std::unique_ptr<A2Expr>> elements;

    A2ExprLiteralData(): A2Expr(A2ExprType::LITERAL_DATA) {}
    ~A2ExprLiteralData() override = default;

    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A2ExprLiteralData");
        for (auto& element : elements) {
            result += "\n" + element->toString(indent + 1);
        }
        return result;
    }
};

enum class A2ExprOpType {
    NONE,
    B_DOT, B_ARROW, B_INDEX, T_SLICE,
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
    T_COND,
    // integrated func
    U_SIZEOF, B_CAST, B_MAKE, U_LEN
};

class A2ExprOperation : public A2Expr { // operation
    public:
    A2ExprOpType subType;
    std::unique_ptr<A2Type> typeOperand; // for sizeof(type), cast<type>
    std::unique_ptr<A2Expr> operand0;
    std::unique_ptr<A2Expr> operand1;
    std::unique_ptr<A2Expr> operand2;
    int accessPos; // struct member index

    A2ExprOperation(): A2Expr(A2ExprType::OPERATION), subType(A2ExprOpType::NONE), typeOperand(), operand0(), operand1(), operand2(), accessPos(-1) {}
    A2ExprOperation(A2ExprOpType t): A2Expr(A2ExprType::OPERATION), subType(t), typeOperand(), operand0(), operand1(), operand2(), accessPos(-1) {}
    ~A2ExprOperation() override = default;

    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A2ExprOperation {} {}", (int)subType, accessPos);
        if (typeOperand) result += "\n" + typeOperand->toString(indent + 1);
        if (operand0) result += "\n" + operand0->toString(indent + 1);
        if (operand1) result += "\n" + operand1->toString(indent + 1);
        if (operand2) result += "\n" + operand2->toString(indent + 1);
        return result;
    }
};

class A2ExprName: public A2Expr { // variable or function name
    public:
    A2Decl* decl;

    A2ExprName(): A2Expr(A2ExprType::NONE), decl(nullptr) {}
    A2ExprName(A2ExprType tp, A2Decl* d): A2Expr(tp), decl(d) {}
    ~A2ExprName() override = default;

    std::string toString(int indent) override { return std::string(indent * 2, ' ') + std::format("A2ExprName {}", decl->name); }
};

class A2ExprFuncCall : public A2Expr { // static function call
    public:
    A2Decl* func;
    std::vector<std::unique_ptr<A2Expr>> args;

    A2ExprFuncCall(): A2Expr(A2ExprType::FUNC_CALL), func(nullptr), args() {}
    ~A2ExprFuncCall() override = default;

    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A2ExprFuncCall");
        if (func) result += "\n" + func->toString(indent + 1);
        for (auto& arg : args) {
            result += "\n" + arg->toString(indent + 1);
        }
        return result;
    }
};

class A2ExprFptrCall : public A2Expr { // function pointer call
    public:
    std::unique_ptr<A2Expr> fptr;
    std::vector<std::unique_ptr<A2Expr>> args;

    A2ExprFptrCall(): A2Expr(A2ExprType::FPTR_CALL), fptr(), args() {}
    ~A2ExprFptrCall() override = default;

    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A2ExprFptrCall");
        if (fptr) result += "\n" + fptr->toString(indent + 1);
        for (auto& arg : args) {
            result += "\n" + arg->toString(indent + 1);
        }
        return result;
    }
};

// AST2 statement node implementation
class A2StatRaw : public A2Stat { // raw code statement
    public:
    std::string code;

    A2StatRaw(): A2Stat(A2StatType::NONE), code() {}
    A2StatRaw(A2StatType tp): A2Stat(tp), code() {}
    ~A2StatRaw() override = default;

    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A2StatRaw {} {}", (int)objType, code);
        return result;
    }
};

class A2StatExpr : public A2Stat { // expression statement
    public:
    std::unique_ptr<A2Expr> expr;

    A2StatExpr(): A2Stat(A2StatType::EXPR), expr() {}
    ~A2StatExpr() override = default;

    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A2StatExpr");
        if (expr) result += "\n" + expr->toString(indent + 1);
        return result;
    }
};

class A2StatDecl : public A2Stat { // declaration statement
    public:
    std::unique_ptr<A2Decl> decl;

    A2StatDecl(): A2Stat(A2StatType::DECL), decl() {}
    ~A2StatDecl() override = default;

    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A2StatDecl");
        if (decl) result += "\n" + decl->toString(indent + 1);
        return result;
    }
};

class A2StatAssign : public A2Stat { // assignment statement
    public:
    std::unique_ptr<A2Expr> left;
    std::unique_ptr<A2Expr> right;

    A2StatAssign(): A2Stat(A2StatType::ASSIGN), left(), right() {}
    ~A2StatAssign() override = default;

    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A2StatAssign");
        if (left) result += "\n" + left->toString(indent + 1);
        if (right) result += "\n" + right->toString(indent + 1);
        return result;
    }
};

// A2Stat loop statement forward
class A2StatLoop : public A2Stat { // while & for statement
    public:
    std::unique_ptr<A2Expr> cond;
    std::unique_ptr<A2Stat> step; // for loop step
    std::unique_ptr<A2Stat> body;

    A2StatLoop(): A2Stat(A2StatType::LOOP), cond(), step(), body() {}
    ~A2StatLoop() override = default;

    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A2StatLoop");
        if (cond) result += "\n" + cond->toString(indent + 1);
        if (step) result += "\n" + step->toString(indent + 1);
        if (body) result += "\n" + body->toString(indent + 1);
        return result;
    }
};
// A2Stat loop statement forward

class A2StatCtrl : public A2Stat { // control statement (return, break, continue)
    public:
    std::unique_ptr<A2Expr> body; // for return
    A2StatLoop* loop; // for break, continue

    A2StatCtrl(): A2Stat(A2StatType::NONE), body(), loop() {}
    A2StatCtrl(A2StatType tp): A2Stat(tp), body(), loop() {}
    ~A2StatCtrl() override = default;

    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A2StatCtrl {}", (int)objType);
        if (body) result += "\n" + body->toString(indent + 1);
        if (loop) result += "\n" + loop->toString(indent + 1);
        return result;
    }
};

class A2StatScope : public A2Stat { // scope statement
    public:
    A2StatScope* parent;
    std::vector<std::unique_ptr<A2Stat>> body;
    std::vector<std::unique_ptr<A2Expr>> defers;

    A2StatScope(): A2Stat(A2StatType::SCOPE), parent(nullptr), body(), defers() {}
    ~A2StatScope() override = default;

    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A2StatScope");
        for (auto& stat : body) {
            result += "\n" + stat->toString(indent + 1);
        }
        return result;
    }
};

class A2StatIf : public A2Stat { // if statement
    public:
    std::unique_ptr<A2Expr> cond;
    std::unique_ptr<A2Stat> thenBody;
    std::unique_ptr<A2Stat> elseBody;

    A2StatIf(): A2Stat(A2StatType::IF), cond(), thenBody(), elseBody() {}
    ~A2StatIf() override = default;

    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A2StatIf");
        if (cond) result += "\n" + cond->toString(indent + 1);
        if (thenBody) result += "\n" + thenBody->toString(indent + 1);
        if (elseBody) result += "\n" + elseBody->toString(indent + 1);
        return result;
    }
};

class A2StatSwitch : public A2Stat { // switch statement
    public:
    std::unique_ptr<A2Expr> cond;
    std::vector<int64_t> caseConds;
    std::vector<bool> caseFalls;
    std::vector<std::vector<std::unique_ptr<A2Stat>>> caseBodies;
    std::vector<std::unique_ptr<A2Stat>> defaultBody;

    A2StatSwitch(): A2Stat(A2StatType::SWITCH), cond(), caseConds(), caseFalls(), caseBodies(), defaultBody() {}
    ~A2StatSwitch() override = default;

    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A2StatSwitch");
        if (cond) result += "\n" + cond->toString(indent + 1);
        for (size_t i = 0; i < caseConds.size(); i++) {
            result += "\n" + std::string(indent * 2, ' ') + std::format("case {}:", caseConds[i]);
            for (auto& stat : caseBodies[i]) {
                result += "\n" + std::string(indent * 2, ' ') + stat->toString(indent + 1);
            }
        }
        if (!defaultBody.empty()) {
            result += "\ndefault:";
            for (auto& stat : defaultBody) {
                result += "\n" + std::string(indent * 2, ' ') + stat->toString(indent + 1);
            }
        }
        return result;
    }
};

// AST2 declaration node implementation
class A2DeclRaw : public A2Decl { // raw code
    public:
    std::string code;

    A2DeclRaw(): A2Decl(A2DeclType::NONE), code() {}
    A2DeclRaw(A2DeclType t): A2Decl(t), code() {}
    ~A2DeclRaw() override = default;

    std::string toString(int indent) override { return std::string(indent * 2, ' ') + std::format("A2DeclRaw {} {}", (int)objType, code); }
};

class A2DeclVar : public A2Decl { // variable declaration
    public:
    std::unique_ptr<A2Expr> init;
    bool isDefine;
    bool isConst;
    bool isVolatile;
    bool isExtern;
    bool isParam;

    A2DeclVar(): A2Decl(A2DeclType::VAR), init(), isDefine(false), isConst(false), isVolatile(false), isExtern(false), isParam(false) {}
    A2DeclVar(std::unique_ptr<A2Type> t, std::string nm): A2Decl(A2DeclType::VAR), init(), isDefine(false), isConst(false), isVolatile(false), isExtern(false), isParam(false) { name = nm; type = std::move(t); }
    ~A2DeclVar() override = default;

    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A2DeclVar {} {}", (int)objType, name);
        if (init) result += "\n" + init->toString(indent + 1);
        return result;
    }
};

class A2DeclFunc : public A2Decl { // function declaration
    public:
    std::string structNm; // for method
    std::string funcNm; // for method
    std::vector<std::unique_ptr<A2Type>> paramTypes;
    std::vector<std::string> paramNames;
    std::unique_ptr<A2Type> retType;
    std::unique_ptr<A2StatScope> body; // have param declarations
    bool isVaArg;

    A2DeclFunc(): A2Decl(A2DeclType::FUNC), structNm(), funcNm(), paramTypes(), paramNames(), retType(), body(), isVaArg(false) {}
    ~A2DeclFunc() override = default;

    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A2DeclFunc {} {}", (int)objType, name);
        for (size_t i = 0; i < paramTypes.size(); i++) {
            result += "\n" + std::string(indent * 2, ' ') + std::format("param {}:", i);
            result += "\n" + paramTypes[i]->toString(indent + 1);
        }
        if (retType) result += "\n" + retType->toString(indent + 1);
        if (body) result += "\n" + body->toString(indent + 1);
        return result;
    }
};

class A2DeclStruct : public A2Decl { // struct declaration
    public:
    std::vector<std::unique_ptr<A2Type>> memTypes;
    std::vector<std::string> memNames;
    std::vector<int> memOffsets;

    A2DeclStruct(): A2Decl(A2DeclType::STRUCT), memTypes(), memNames(), memOffsets() {}
    ~A2DeclStruct() override = default;

    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A2DeclStruct {} {}", (int)objType, name);
        for (size_t i = 0; i < memTypes.size(); i++) {
            result += "\n" + std::string(indent * 2, ' ') + std::format("member {}:", i);
            result += "\n" + memTypes[i]->toString(indent + 1);
        }
        return result;
    }
};

class A2DeclEnum : public A2Decl { // enum declaration
    public:
    std::vector<std::string> memNames;
    std::vector<int64_t> memValues;

    A2DeclEnum(): A2Decl(A2DeclType::ENUM), memNames(), memValues() {}
    ~A2DeclEnum() override = default;

    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A2DeclEnum {}", (int)objType, name);
        for (size_t i = 0; i < memNames.size(); i++) {
            result += "\n" + std::string(indent * 2, ' ') + std::format("member {}: {}", i, memNames[i]);
        }
        return result;
    }
};

// compile tree generator context info
class ScopeInfo {
    public:
    A2StatScope* scope;
    std::unordered_map<std::string, A2DeclVar*> nameMap; // global, local var name map

    ScopeInfo(): scope(nullptr), nameMap() {}
    ScopeInfo(A2StatScope* s): scope(s), nameMap() {}
};

// single source file module
class A2Module {
    public:
    std::string path;
    std::string uname;
    std::unique_ptr<A2StatScope> code;
    std::unordered_map<std::string, A2Decl*> nameMap; // name map for func, struct, enum

    A2Module(): path(""), uname(""), code(), nameMap() {}
    A2Module(const std::string& fpath): path(fpath), uname(""), code(), nameMap() {}
    A2Module(const std::string& fpath, const std::string& uname): path(fpath), uname(uname), code(), nameMap() {}

    std::string toString() {
        std::string result = std::format("A2Module {} {}", path, uname);
        if (code) result += "\n" + code->toString(0);
        return result;
    }
};

// AST2 generator
class A2Gen {
    public:
    CompileMessage prt;
    int arch;
    std::vector<std::unique_ptr<A2Module>> modules;

    A2Gen(): prt(3), arch(8), modules(), uidCount(0), ast1(), typePool(), genOrder(), scopes(), loops(), curModule(), curFunc() {}
    A2Gen(int p, int a): prt(p), arch(a), modules(), uidCount(0), ast1(), typePool(), genOrder(), scopes(), loops(), curModule(), curFunc() {}

    // global convertion context
    int64_t uidCount;
    A1Ext* ast1;
    std::vector<std::string> genOrder; // code generation order of fpath
    std::vector<std::unique_ptr<A2Type>> typePool; // type storage

    // local convertion context
    std::vector<ScopeInfo> scopes;
    std::vector<A2StatLoop*> loops;
    A2Module* curModule;
    A2DeclFunc* curFunc;

    std::string getLocString(Location loc) { return std::format("{}:{}", genOrder[loc.srcLoc], loc.line); } // get location string
    int findModule(const std::string& uname) {
        for (size_t i = 0; i < modules.size(); i++) {
            if (modules[i]->uname == uname) return i;
        }
        return -1;
    }

    std::string convert(A1Ext* ext);

    private:
    void initTypePool();

    int findType(A2Type* t) {
        for (size_t i = 0; i < typePool.size(); i++) {
            if (isTypeEqual(typePool[i].get(), t)) return i;
        }
        return -1;
    }

    A2DeclVar* findVar(const std::string& name) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            if (it->nameMap.count(name)) return it->nameMap[name];
        }
        return nullptr;
    }

    int nameCheck(const std::string& name, A1Module* mod, Location loc);

    std::unique_ptr<A2Type> convertType(A1Type* t, A1Module* mod);

    std::unique_ptr<A2Expr> convertExpr(A1Expr* e, A1Module* mod, A2Type* expectedType);
    std::unique_ptr<A2ExprLiteral> convertLiteralExpr(A1ExprLiteral* lit, A2Type* expectedType);
    std::unique_ptr<A2Expr> convertDotExpr(A1ExprOperation* op, A1Module* mod);
    std::unique_ptr<A2Expr> convertOpExpr(A1ExprOperation* op, A1Module* mod, A2Type* typeHint);
    std::unique_ptr<A2Expr> convertFuncCallExpr(A1ExprFuncCall* fcall, A1Module* mod);

    std::unique_ptr<A2Stat> convertStat(A1Stat* s, A1Module* mod, A2StatScope* parent);
    bool checkReturnable(A2Stat* stat);

    std::unique_ptr<A2Decl> convertDecl(A1Decl* d, A1Module* mod);
};

#endif // AST2_H