#ifndef AST3_H
#define AST3_H

#include <unordered_map>
#include "ast2.h"
#include "baseFunc.h"

// AST3 type node
enum class A3TypeType {
    PRIMITIVE,
    POINTER,
    ARRAY, // array of A3 has size-tags but will act like pointer
    SLICE,
    FUNCTION, // function returns array -> add ptr at end, change ret to void
    STRUCT
    // enum -> int
};

class A3Type {
    public:
    A3TypeType objType;
    Location location;
    std::string name;
    std::unique_ptr<A3Type> direct; // ptr, arr, slice target & func return
    std::vector<std::unique_ptr<A3Type>> indirect; // func args
    int64_t arrLen; // array length
    int typeSize; // total size in bytes
    int typeAlign; // align requirement in bytes

    std::unique_ptr<A3Type> clone() const {
        auto res = std::make_unique<A3Type>();
        res->objType = objType;
        res->location = location;
        res->name = name;
        if (direct) res->direct = direct->clone();
        for (const auto& item : indirect) {
            res->indirect.push_back(item->clone());
        }
        res->arrLen = arrLen;
        res->typeSize = typeSize;
        res->typeAlign = typeAlign;
        return res;
    }

    std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("A2Type {} {} {} {} {}", (int)objType, name, arrLen, typeSize, typeAlign);
        if (direct) result += "\n" + direct->toString(indent + 1);
        for (auto& ind : indirect) {
            result += "\n" + ind->toString(indent + 1);
        }
        return result;
    }
};

// AST3 expression node
enum class A3ExprType {
    LITERAL, // literal_data -> converted to preStats
    OPERATION,
    VAR_NAME,
    FUNC_NAME,
    FUNC_CALL,
    FPTR_CALL
};

class A3Expr {
    public:
    A3ExprType objType;
    Location location;
    A3Type* exprType;

    virtual ~A3Expr() = default;
    virtual std::string toString(int indent) { return std::string(indent * 2, ' ') + std::format("A3Expr {}", (int)objType); }
};

// AST3 statement node
enum class A3StatType {
    RAW_C, RAW_IR, // raw code
    LABEL, JUMP, BREAK, CONTINUE, RETURN, // controls
    MEMSET, MEMCPY, // array copy
    EXPR,
    DECL,
    ASSIGN,
    SCOPE,
    IF,
    WHILE,
    SWITCH
};

class A3Stat {
    public:
    A3StatType objType;
    Location location;
    int64_t uid;

    virtual ~A3Stat() = default;
    virtual std::string toString(int indent) { return std::string(indent * 2, ' ') + std::format("A3Stat {}", (int)objType); }
};

// AST3 declaration node
enum class A3DeclType {
    RAW_C,
    RAW_IR,
    VAR,
    FUNC,
    STRUCT,
    ENUM
};

class A3Decl {
    public:
    A3DeclType objType;
    Location location;
    std::string name; // declaration name
    int64_t uid;
    std::unique_ptr<A3Type> type; // declaration type
    bool isExported;

    virtual ~A3Decl() = default;
    virtual std::string toString(int indent) { 
        std::string result = std::string(indent * 2, ' ') + std::format("A3Decl {} {}", (int)objType, name);
        if (type) result += "\n" + type->toString(indent + 1);
        return result;
    }
};

// AST3 expression node implementation
class A3ExprLiteral : public A3Expr { // literal value
    public:
    Literal value;
    
    ~A3ExprLiteral() override = default;
    std::string toString(int indent) override { return std::string(indent * 2, ' ') + std::format("A3ExprLiteral {}", value.toString()); }
};

enum class A3ExprOpType {
    B_DOT, B_ARROW, B_INDEX, // slice arr[m:n] -> lowered to make(&arr[m], n-m)
    U_PLUS, U_MINUS, U_LOGIC_NOT, U_BIT_NOT, U_REF, U_DEREF,
    B_MUL, B_DIV, B_MOD,
    B_ADD, B_SUB, B_PTR_ADD, B_PTR_SUB, // lowered to ptr_add, ptr_sub
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

class A3ExprOperation : public A3Expr { // operation
    public:
    A3ExprOpType subType;
    std::unique_ptr<A3Type> typeOperand; // for sizeof(type), cast<type>
    std::unique_ptr<A3Expr> operand0;
    std::unique_ptr<A3Expr> operand1;
    std::unique_ptr<A3Expr> operand2;
    int accessPos; // struct member index

    ~A3ExprOperation() override = default;
    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A3ExprOperation {} {}", (int)subType, accessPos);
        if (typeOperand) result += "\n" + typeOperand->toString(indent + 1);
        if (operand0) result += "\n" + operand0->toString(indent + 1);
        if (operand1) result += "\n" + operand1->toString(indent + 1);
        return result;
    }
};

class A3ExprName: public A3Expr { // variable or function name
    public:
    A3Decl* decl;

    ~A3ExprName() override = default;
    std::string toString(int indent) override { return std::string(indent * 2, ' ') + std::format("A3ExprName {}", decl->name); }
};

class A3ExprFuncCall : public A3Expr { // static function call
    public:
    A3Decl* func;
    std::vector<std::unique_ptr<A3Expr>> args;

    ~A3ExprFuncCall() override = default;
    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A3ExprFuncCall");
        if (func) result += "\n" + func->toString(indent + 1);
        for (auto& arg : args) {
            result += "\n" + arg->toString(indent + 1);
        }
        return result;
    }
};

class A3ExprFptrCall : public A3Expr { // function pointer call
    public:
    std::unique_ptr<A3Expr> fptr;
    std::vector<std::unique_ptr<A3Expr>> args;

    ~A3ExprFptrCall() override = default;
    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A3ExprFptrCall");
        if (fptr) result += "\n" + fptr->toString(indent + 1);
        for (auto& arg : args) {
            result += "\n" + arg->toString(indent + 1);
        }
        return result;
    }
};

// AST3 statement node implementation
class A3StatRaw : public A3Stat { // raw code statement
    public:
    std::string code;

    ~A3StatRaw() override = default;
    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A3StatRaw {} {}", (int)objType, code);
        return result;
    }
};

class A3StatCtrl : public A3Stat { // label, jump, break, continue, return
    public:
    A3StatCtrl* label; // jump target
    std::unique_ptr<A3Expr> expr; // return value

    ~A3StatCtrl() override = default;
    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A3StatCtrl {}", (int)objType);
        return result;
    }
};

class A3StatMem : public A3Stat { // memset, memcpy
    public:
    std::unique_ptr<A3Expr> src;
    std::unique_ptr<A3Expr> dst; // memset tgt
    std::unique_ptr<A3Expr> size;
    int64_t sizeHint; // pre-calculated size for IR

    ~A3StatMem() override = default;
    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A3StatMem {}", sizeHint);
        if (src) result += "\n" + src->toString(indent + 1);
        if (dst) result += "\n" + dst->toString(indent + 1);
        if (size) result += "\n" + size->toString(indent + 1);
        return result;
    }
};

class A3StatExpr : public A3Stat { // expression statement
    public:
    std::unique_ptr<A3Expr> expr;

    ~A3StatExpr() override = default;
    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A3StatExpr");
        if (expr) result += "\n" + expr->toString(indent + 1);
        return result;
    }
};

class A3StatDecl : public A3Stat { // declaration statement
    public:
    std::unique_ptr<A3Decl> decl;

    ~A3StatDecl() override = default;
    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A2StatDecl");
        if (decl) result += "\n" + decl->toString(indent + 1);
        return result;
    }
};

class A3StatAssign : public A3Stat { // assignment statement
    public:
    std::unique_ptr<A3Expr> left;
    std::unique_ptr<A3Expr> right;

    ~A3StatAssign() override = default;
    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A3StatAssign");
        if (left) result += "\n" + left->toString(indent + 1);
        if (right) result += "\n" + right->toString(indent + 1);
        return result;
    }
};

class A3StatScope : public A3Stat { // scope statement
    public:
    std::vector<std::unique_ptr<A3Stat>> body;

    ~A3StatScope() override = default;
    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A3StatScope");
        for (auto& stat : body) {
            result += "\n" + stat->toString(indent + 1);
        }
        return result;
    }
};

class A3StatIf : public A3Stat { // if statement
    public:
    std::unique_ptr<A3Expr> cond;
    std::unique_ptr<A3Stat> thenBody;
    std::unique_ptr<A3Stat> elseBody;

    ~A3StatIf() override = default;
    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A3StatIf");
        if (cond) result += "\n" + cond->toString(indent + 1);
        if (thenBody) result += "\n" + thenBody->toString(indent + 1);
        if (elseBody) result += "\n" + elseBody->toString(indent + 1);
        return result;
    }
};

class A3StatWhile : public A3Stat { // while statement
    public:
    std::unique_ptr<A3Expr> cond;
    std::unique_ptr<A3Stat> body;

    ~A3StatWhile() override = default;
    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A3StatWhile");
        if (cond) result += "\n" + cond->toString(indent + 1);
        if (body) result += "\n" + body->toString(indent + 1);
        return result;
    }
};

class A3StatSwitch : public A3Stat { // switch statement
    public:
    std::unique_ptr<A3Expr> cond;
    std::vector<int64_t> caseConds;
    std::vector<bool> caseFalls;
    std::vector<std::vector<std::unique_ptr<A3Stat>>> caseBodies;
    std::vector<std::unique_ptr<A3Stat>> defaultBody;

    ~A3StatSwitch() override = default;
    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A3StatSwitch");
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

// AST3 declaration node implementation
class A3DeclRaw : public A3Decl { // raw code
    public:
    std::string code;

    ~A3DeclRaw() override = default;
    std::string toString(int indent) override { return std::string(indent * 2, ' ') + std::format("A3DeclRaw {} {}", (int)objType, code); }
};

class A3DeclVar : public A3Decl { // variable declaration
    public:
    std::unique_ptr<A3Expr> init;
    bool isConst; // define, param, extern is not real variable
    bool isVolatile;

    ~A3DeclVar() override = default;
    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A3DeclVar {} {}", (int)objType, name);
        if (init) result += "\n" + init->toString(indent + 1);
        return result;
    }
};

class A3DeclFunc : public A3Decl { // function declaration
    public:
    std::vector<std::unique_ptr<A3DeclVar>> params;
    std::unique_ptr<A3Type> retType;
    std::unique_ptr<A3StatScope> body; // have param init codes
    bool isVaArg; // for A3 only

    ~A3DeclFunc() override = default;
    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A3DeclFunc {} {}", (int)objType, name);
        for (size_t i = 0; i < params.size(); i++) {
            result += "\n" + std::string(indent * 2, ' ') + std::format("param {}:", i);
            result += "\n" + params[i]->toString(indent + 1);
        }
        if (retType) result += "\n" + retType->toString(indent + 1);
        if (body) result += "\n" + body->toString(indent + 1);
        return result;
    }
};

class A3DeclStruct : public A3Decl { // struct declaration
    public:
    std::vector<std::unique_ptr<A3Type>> memTypes;
    std::vector<std::string> memNames;
    std::vector<int> memOffsets;

    ~A3DeclStruct() override = default;
    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A3DeclStruct {} {}", (int)objType, name);
        for (size_t i = 0; i < memTypes.size(); i++) {
            result += "\n" + std::string(indent * 2, ' ') + std::format("member {}:", i);
            result += "\n" + memTypes[i]->toString(indent + 1);
        }
        return result;
    }
};

class A3DeclEnum : public A3Decl { // enum declaration
    public:
    std::vector<std::string> memNames;
    std::vector<int64_t> memValues;

    ~A3DeclEnum() override = default;
    std::string toString(int indent) override {
        std::string result = std::string(indent * 2, ' ') + std::format("A3DeclEnum {}", (int)objType, name);
        for (size_t i = 0; i < memNames.size(); i++) {
            result += "\n" + std::string(indent * 2, ' ') + std::format("member {}: {}", i, memNames[i]);
        }
        return result;
    }
};

// AST3 convert context
class A3ScopeInfo {
    public:
    A3StatScope* scope;
    A3StatCtrl* lbl;
    std::unordered_map<int64_t, A3Decl*> nameMap;
};

class A3Gen {
    public:
    CompileMessage prt;
    int arch;
    int64_t bigCopyAlert;
    std::unique_ptr<A3Stat> code;

    // convert context
    int64_t uidCount;
    A2Gen* ast2;
    std::vector<std::string> genOrder; // code generation order of fpath

    std::vector<std::unique_ptr<A3Type>> typePool; // type pool
    std::vector<std::unique_ptr<A3ScopeInfo>> scopes; // scope context
    std::vector<A3StatScope*> jmpScopes; // scope with jumps
    std::vector<A3StatWhile*> jmpWhiles; // while with jumps

    std::vector<std::unique_ptr<A3Stat>> statBuf; // preStat buffer

    A3Gen(int p, int a, int64_t b, int64_t c) : prt(p), arch(a), bigCopyAlert(b), uidCount(c) {}

    std::string lower(A2Gen* a);

    std::string getLocString(Location loc) { return std::format("{}:{}", genOrder[loc.srcLoc], loc.line); } // get location string

    private:
    void initTypePool();
    int findType(A3Type* t);

    A3Decl* findDecl(int64_t uid) { // find declaration by uid in global scope
        if (scopes[0]->nameMap.count(uid)) return scopes[0]->nameMap[uid];
        return nullptr;
    }

    A3DeclVar* findVar(int64_t uid) { // find variable by uid in all scopes
        for (auto& scope : scopes) {
            if (scope->nameMap.count(uid)) {
                A3Decl* decl = scope->nameMap[uid];
                if (decl->objType == A3DeclType::VAR) return (A3DeclVar*)decl;
            }
        }
        return nullptr;
    }

    A3DeclVar* findVar(std::string name) { // find variable by name in all scopes
        for (auto& scope : scopes) {
            for (auto& [uid, decl] : scope->nameMap) {
                if (decl->name == name && decl->objType == A3DeclType::VAR) return (A3DeclVar*)decl;
            }
        }
        return nullptr;
    }

    std::string genName() { // generate unique name for temp var
        int count = 0;
        while (true) {
            std::string name = std::format("_t{}_{}", uidCount, count++);
            if (findVar(name)) {
                if (count > 16) {uidCount++; count = 0;}
            } else {
                return name;
            }
        }
    }

    std::string genTempVar(A3Type* t, Location l);
    std::string setTempVar(A3Type* t, std::unique_ptr<A3Expr> v);
    std::unique_ptr<A3ExprName> getTempVar(std::string name, Location l);
    std::unique_ptr<A3ExprOperation> refVar(std::string name, Location l);
    std::unique_ptr<A3StatAssign> genAssignStat(std::unique_ptr<A3Expr> left, std::unique_ptr<A3Expr> right);

    std::unique_ptr<A3Type> lowerType(A2Type* t);

    std::unique_ptr<A3Expr> lowerExpr(A2Expr* e, std::string assignVarName);
    std::unique_ptr<A3Expr> lowerExprLitString(A2ExprLiteral* l);
    std::unique_ptr<A3Expr> lowerExprLitData(A2ExprLiteralData* e, std::string* setName);
    std::unique_ptr<A3Expr> lowerExprOp(A2ExprOperation* e);
    std::unique_ptr<A3Expr> lowerExprOpSlice(A2ExprOperation* e);
    std::unique_ptr<A3Expr> lowerExprOpCond(A2ExprOperation* e);
    std::vector<std::unique_ptr<A3Expr>> lowerExprCall(A3Type* ftype, std::vector<std::unique_ptr<A2Expr>>& a2Args, bool isVaArg, bool isRetArray, std::string* retName);

    std::unique_ptr<A3Stat> lowerStat(A2Stat* s);
    std::unique_ptr<A3StatScope> lowerStatScope(A2StatScope* s);
    std::unique_ptr<A3StatExpr> lowerStatExpr(A2StatExpr* s);
    std::unique_ptr<A3StatIf> lowerStatIf(A2StatIf* s);
    std::unique_ptr<A3StatWhile> lowerStatLoop(A2StatLoop* s);
};

#endif // AST3_H