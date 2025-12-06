#ifndef AST1_H
#define AST1_H

#include <format>
#include "baseFunc.h"
#include "tokenizer.h"

// forward declarations
class A1StatScope;
class A1DeclVar;

// AST1 type node
enum class A1TypeType {
    NONE,
    AUTO,
    PRIMITIVE,
    POINTER,
    ARRAY,
    SLICE,
    FUNCTION,
    NAME, // for struct, enum, template
    FOREIGN, // from other source file
    TEMPLATE // for A1Ext only, incName is [uname of caller]/[incName] or [uname of caller]
};

class A1Type {
    public:
    A1TypeType objType;
    Location location;
    std::string name;
    std::string incName; // include namespace for FOREIGN type
    std::unique_ptr<A1Type> direct; // ptr, arr, slice target & func return
    std::vector<std::unique_ptr<A1Type>> indirect; // func args
    int64_t arrLen; // array length
    int typeSize; // total size in bytes
    int typeAlign; // align requirement in bytes
    
    A1Type(): objType(A1TypeType::NONE), location(), name(""), incName(""), direct(nullptr), indirect(), arrLen(-1), typeSize(-1), typeAlign(-1) {}
    A1Type(A1TypeType tp, const std::string& nm): objType(tp), location(), name(nm), incName(""), direct(nullptr), indirect(), arrLen(-1), typeSize(-1), typeAlign(-1) {}
    A1Type(const std::string& incNm, const std::string& tpNm): objType(A1TypeType::FOREIGN), location(), name(tpNm), incName(incNm), direct(nullptr), indirect(), arrLen(-1), typeSize(-1), typeAlign(-1) {}

    std::unique_ptr<A1Type> Clone() {
        std::unique_ptr<A1Type> newType = std::make_unique<A1Type>();
        newType->objType = objType;
        newType->location = location;
        newType->name = name;
        newType->incName = incName;
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

    std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("A1Type {} {} {} {} {} {}", (int)objType, name, incName, arrLen, typeSize, typeAlign);
        if (direct) result += "\n" + direct->toString(indent + 1);
        for (auto& ind : indirect) {
            result += "\n" + ind->toString(indent + 1);
        }
        return result;
    }
};

// AST1 expression node
enum class A1ExprType {
    NONE,
    LITERAL,
    LITERAL_DATA,
    NAME,
    OPERATION,
    FUNC_CALL
};

class A1Expr {
    public:
    A1ExprType objType;
    Location location;

    A1Expr(): objType(A1ExprType::NONE), location() {}
    A1Expr(A1ExprType t): objType(t), location() {}
    virtual ~A1Expr() = default;

    virtual std::unique_ptr<A1Expr> Clone() {
        std::unique_ptr<A1Expr> newNode = std::make_unique<A1Expr>(objType);
        newNode->location = location;
        return newNode;
    }

    virtual std::string toString(int indent) { return std::string(indent * 2, ' ') + std::format("A1Expr {}", (int)objType); }
};

// AST1 statement node
enum class A1StatType {
    NONE,
    RAW_C,
    RAW_IR,
    EXPR,
    DECL,
    ASSIGN,
    RETURN,
    DEFER,
    BREAK,
    CONTINUE,
    FALL,
    SCOPE,
    IF,
    WHILE,
    FOR,
    SWITCH
};

class A1Stat {
    public:
    A1StatType objType;
    Location location;

    A1Stat(): objType(A1StatType::NONE), location() {}
    A1Stat(A1StatType t): objType(t), location() {}
    virtual ~A1Stat() = default;

    virtual std::unique_ptr<A1Stat> Clone(A1StatScope* parent) {
        std::unique_ptr<A1Stat> newNode = std::make_unique<A1Stat>(objType);
        newNode->location = location;
        return newNode;
    }

    virtual std::string toString(int indent) { return std::string(indent * 2, ' ') + std::format("A1Stat {}", (int)objType); }
};

// AST1 declaration node
enum class A1DeclType {
    NONE,
    RAW_C,
    RAW_IR,
    INCLUDE,
    TEMPLATE,
    VAR,
    FUNC,
    STRUCT,
    ENUM
};

class A1Decl {
    public:
    A1DeclType objType;
    Location location;
    std::string name; // declaration name
    std::unique_ptr<A1Type> type; // declaration type
    bool isExported;

    A1Decl(): objType(A1DeclType::NONE), location() {}
    A1Decl(A1DeclType t): objType(t), location() {}
    virtual ~A1Decl() = default;

    virtual std::unique_ptr<A1Decl> Clone(A1StatScope* parent) {
        std::unique_ptr<A1Decl> newNode = std::make_unique<A1Decl>(objType);
        newNode->location = location;
        newNode->name = name;
        newNode->type = type->Clone();
        newNode->isExported = isExported;
        return newNode;
    }

    virtual std::string toString(int indent) { 
        std::string result = std::string(indent * 2, ' ') + std::format("A1Decl {} {}", (int)objType, name);
        if (type) result += "\n" + type->toString(indent + 1);
        return result;
    }
};

// AST1 expression node implementation
class A1ExprLiteral : public A1Expr { // literal value
    public:
    Literal value;

    A1ExprLiteral(): A1Expr(A1ExprType::LITERAL) {}
    A1ExprLiteral(Literal v): A1Expr(A1ExprType::LITERAL), value(v) {}
    virtual ~A1ExprLiteral() = default;

    virtual std::unique_ptr<A1Expr> Clone() {
        std::unique_ptr<A1ExprLiteral> newNode = std::make_unique<A1ExprLiteral>();
        newNode->location = location;
        newNode->value = value;
        return newNode;
    }

    virtual std::string toString(int indent) { return std::string(indent * 2, ' ') + std::format("A1ExprLiteral {}", value.toString()); }
};

class A1ExprLiteralData : public A1Expr { // literal data chunk
    public:
    std::vector<std::unique_ptr<A1Expr>> elements;

    A1ExprLiteralData(): A1Expr(A1ExprType::LITERAL_DATA) {}
    virtual ~A1ExprLiteralData() = default;

    virtual std::unique_ptr<A1Expr> Clone() {
        std::unique_ptr<A1ExprLiteralData> newNode = std::make_unique<A1ExprLiteralData>();
        newNode->location = location;
        for (auto& element : elements) {
            newNode->elements.push_back(element->Clone());
        }
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("A1ExprLiteralData");
        for (auto& element : elements) {
            result += "\n" + element->toString(indent + 1);
        }
        return result;
    }
};

class A1ExprName : public A1Expr { // name
    public:
    std::string name;

    A1ExprName(): A1Expr(A1ExprType::NAME), name("") {}
    A1ExprName(const std::string& n): A1Expr(A1ExprType::NAME), name(n) {}
    virtual ~A1ExprName() = default;

    virtual std::unique_ptr<A1Expr> Clone() {
        std::unique_ptr<A1ExprName> newNode = std::make_unique<A1ExprName>();
        newNode->location = location;
        newNode->name = name;
        return newNode;
    }

    virtual std::string toString(int indent) { return std::string(indent * 2, ' ') + std::format("A1ExprName {}", name); }
};

enum class A1ExprOpType {
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
    T_COND,
    // integrated func
    U_SIZEOF, B_CAST, B_MAKE, U_LEN
};

class A1ExprOperation : public A1Expr { // operation
    public:
    A1ExprOpType subType;
    std::unique_ptr<A1Type> typeOperand; // for sizeof(type), cast<type>
    std::unique_ptr<A1Expr> operand0;
    std::unique_ptr<A1Expr> operand1;
    std::unique_ptr<A1Expr> operand2;

    A1ExprOperation(): A1Expr(A1ExprType::OPERATION), subType(A1ExprOpType::NONE), typeOperand(), operand0(), operand1(), operand2() {}
    A1ExprOperation(A1ExprOpType t): A1Expr(A1ExprType::OPERATION), subType(t), typeOperand(), operand0(), operand1(), operand2() {}
    virtual ~A1ExprOperation() = default;

    virtual std::unique_ptr<A1Expr> Clone() {
        std::unique_ptr<A1ExprOperation> newNode = std::make_unique<A1ExprOperation>();
        newNode->location = location;
        newNode->subType = subType;
        if (typeOperand) newNode->typeOperand = typeOperand->Clone();
        if (operand0) newNode->operand0 = operand0->Clone();
        if (operand1) newNode->operand1 = operand1->Clone();
        if (operand2) newNode->operand2 = operand2->Clone();
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("A1ExprOperation {}", (int)subType);
        if (typeOperand) result += "\n" + typeOperand->toString(indent + 1);
        if (operand0) result += "\n" + operand0->toString(indent + 1);
        if (operand1) result += "\n" + operand1->toString(indent + 1);
        if (operand2) result += "\n" + operand2->toString(indent + 1);
        return result;
    }
};

class A1ExprFuncCall : public A1Expr { // function call
    public:
    std::unique_ptr<A1Expr> func;
    std::vector<std::unique_ptr<A1Expr>> args;

    A1ExprFuncCall(): A1Expr(A1ExprType::FUNC_CALL), func(), args() {}
    virtual ~A1ExprFuncCall() = default;

    virtual std::unique_ptr<A1Expr> Clone() {
        std::unique_ptr<A1ExprFuncCall> newNode = std::make_unique<A1ExprFuncCall>();
        newNode->location = location;
        newNode->func = func->Clone();
        for (auto& arg : args) {
            newNode->args.push_back(arg->Clone());
        }
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("A1ExprFuncCall");
        if (func) result += "\n" + func->toString(indent + 1);
        for (auto& arg : args) {
            result += "\n" + arg->toString(indent + 1);
        }
        return result;
    }
};

// AST1 statement node implementation
class A1StatRaw : public A1Stat { // raw code statement
    public:
    std::string code;

    A1StatRaw(): A1Stat(A1StatType::NONE), code() {}
    A1StatRaw(A1StatType tp): A1Stat(tp), code() {}
    virtual ~A1StatRaw() = default;

    virtual std::unique_ptr<A1Stat> Clone(A1StatScope* parent) {
        std::unique_ptr<A1StatRaw> newNode = std::make_unique<A1StatRaw>(objType);
        newNode->location = location;
        newNode->code = code;
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("A1StatRaw {} {}", (int)objType, code);
        return result;
    }
};

class A1StatExpr : public A1Stat { // expression statement
    public:
    std::unique_ptr<A1Expr> expr;

    A1StatExpr(): A1Stat(A1StatType::EXPR), expr() {}
    virtual ~A1StatExpr() = default;

    virtual std::unique_ptr<A1Stat> Clone(A1StatScope* parent) {
        std::unique_ptr<A1StatExpr> newNode = std::make_unique<A1StatExpr>();
        newNode->location = location;
        newNode->expr = expr->Clone();
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("A1StatExpr");
        if (expr) result += "\n" + expr->toString(indent + 1);
        return result;
    }
};

class A1StatDecl : public A1Stat { // declaration statement
    public:
    std::unique_ptr<A1Decl> decl;

    A1StatDecl(): A1Stat(A1StatType::DECL), decl() {}
    virtual ~A1StatDecl() = default;

    virtual std::unique_ptr<A1Stat> Clone(A1StatScope* parent) {
        std::unique_ptr<A1StatDecl> newNode = std::make_unique<A1StatDecl>();
        newNode->location = location;
        newNode->decl = decl->Clone(parent);
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("A1StatDecl");
        if (decl) result += "\n" + decl->toString(indent + 1);
        return result;
    }
};

enum class A1StatAssignType {
    NONE,
    ASSIGN,
    ASSIGN_ADD,
    ASSIGN_SUB,
    ASSIGN_MUL,
    ASSIGN_DIV,
    ASSIGN_REMAIN
};

class A1StatAssign : public A1Stat { // assignment statement
    public:
    A1StatAssignType subType;
    std::unique_ptr<A1Expr> left;
    std::unique_ptr<A1Expr> right;

    A1StatAssign(): A1Stat(A1StatType::ASSIGN), subType(A1StatAssignType::NONE), left(), right() {}
    virtual ~A1StatAssign() = default;

    virtual std::unique_ptr<A1Stat> Clone(A1StatScope* parent) {
        std::unique_ptr<A1StatAssign> newNode = std::make_unique<A1StatAssign>();
        newNode->location = location;
        newNode->subType = subType;
        newNode->left = left->Clone();
        newNode->right = right->Clone();
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("A1StatAssign {}", (int)subType);
        if (left) result += "\n" + left->toString(indent + 1);
        if (right) result += "\n" + right->toString(indent + 1);
        return result;
    }
};

class A1StatCtrl : public A1Stat { // control statement (return, defer, break, continue, fall)
    public:
    std::unique_ptr<A1Expr> body;

    A1StatCtrl(): A1Stat(A1StatType::NONE), body() {}
    A1StatCtrl(A1StatType tp): A1Stat(tp), body() {}
    virtual ~A1StatCtrl() = default;

    virtual std::unique_ptr<A1Stat> Clone(A1StatScope* parent) {
        std::unique_ptr<A1StatCtrl> newNode = std::make_unique<A1StatCtrl>(objType);
        newNode->location = location;
        newNode->body = body->Clone();
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("A1StatCtrl {}", (int)objType);
        if (body) result += "\n" + body->toString(indent + 1);
        return result;
    }
};

class A1StatScope : public A1StatCtrl { // scope statement
    public:
    A1StatScope* parent;
    std::vector<std::unique_ptr<A1Stat>> body;

    A1StatScope(): A1StatCtrl(A1StatType::SCOPE), parent(nullptr), body() {}
    A1StatScope(A1StatScope* p): A1StatCtrl(A1StatType::SCOPE), parent(p), body() {}
    virtual ~A1StatScope() = default;

    virtual std::unique_ptr<A1Stat> Clone(A1StatScope* p) {
        std::unique_ptr<A1StatScope> newNode = std::make_unique<A1StatScope>();
        newNode->location = location;
        newNode->parent = p;
        for (auto& stat : body) {
            newNode->body.push_back(stat->Clone(newNode.get()));
        }
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("A1StatScope");
        for (auto& stat : body) {
            result += "\n" + stat->toString(indent + 1);
        }
        return result;
    }

    A1Decl* findDeclaration(const std::string& name); // find var, func, etc declarations by name
    Literal findLiteral(const std::string& name); // find defined literal by name
};

class A1StatIf : public A1StatCtrl { // if statement
    public:
    std::unique_ptr<A1Expr> cond;
    std::unique_ptr<A1Stat> thenBody;
    std::unique_ptr<A1Stat> elseBody;

    A1StatIf(): A1StatCtrl(A1StatType::IF), cond(), thenBody(), elseBody() {}
    virtual ~A1StatIf() = default;

    virtual std::unique_ptr<A1Stat> Clone(A1StatScope* parent) {
        std::unique_ptr<A1StatIf> newNode = std::make_unique<A1StatIf>();
        newNode->location = location;
        newNode->cond = cond->Clone();
        newNode->thenBody = thenBody->Clone(parent);
        newNode->elseBody = elseBody->Clone(parent);
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("A1StatIf");
        if (cond) result += "\n" + cond->toString(indent + 1);
        if (thenBody) result += "\n" + thenBody->toString(indent + 1);
        if (elseBody) result += "\n" + elseBody->toString(indent + 1);
        return result;
    }
};

class A1StatWhile : public A1StatCtrl { // while statement
    public:
    std::unique_ptr<A1Expr> cond;
    std::unique_ptr<A1Stat> body;

    A1StatWhile(): A1StatCtrl(A1StatType::WHILE), cond(), body() {}
    virtual ~A1StatWhile() = default;

    virtual std::unique_ptr<A1Stat> Clone(A1StatScope* parent) {
        std::unique_ptr<A1StatWhile> newNode = std::make_unique<A1StatWhile>();
        newNode->location = location;
        newNode->cond = cond->Clone();
        newNode->body = body->Clone(parent);
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("A1StatWhile");
        if (cond) result += "\n" + cond->toString(indent + 1);
        if (body) result += "\n" + body->toString(indent + 1);
        return result;
    }
};

class A1StatFor : public A1StatCtrl { // for statement
    public:
    // init statement will be at another outside scope
    std::unique_ptr<A1Expr> cond;
    std::unique_ptr<A1Stat> step;
    std::unique_ptr<A1Stat> body;

    A1StatFor(): A1StatCtrl(A1StatType::FOR), cond(), step(), body() {}
    virtual ~A1StatFor() = default;

    virtual std::unique_ptr<A1Stat> Clone(A1StatScope* parent) {
        std::unique_ptr<A1StatFor> newNode = std::make_unique<A1StatFor>();
        newNode->location = location;
        newNode->cond = cond->Clone();
        newNode->step = step->Clone(parent);
        newNode->body = body->Clone(parent);
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("A1StatFor");
        if (cond) result += "\n" + cond->toString(indent + 1);
        if (step) result += "\n" + step->toString(indent + 1);
        if (body) result += "\n" + body->toString(indent + 1);
        return result;
    }
};

class A1StatSwitch : public A1StatCtrl { // switch statement
    public:
    std::unique_ptr<A1Expr> cond;
    std::vector<int64_t> caseConds;
    std::vector<std::vector<std::unique_ptr<A1Stat>>> caseBodies;
    std::vector<std::unique_ptr<A1Stat>> defaultBody;

    A1StatSwitch(): A1StatCtrl(A1StatType::SWITCH), cond(), caseConds(), caseBodies(), defaultBody() {}
    virtual ~A1StatSwitch() = default;

    virtual std::unique_ptr<A1Stat> Clone(A1StatScope* parent) {
        std::unique_ptr<A1StatSwitch> newNode = std::make_unique<A1StatSwitch>();
        newNode->location = location;
        newNode->cond = cond->Clone();
        newNode->caseConds = caseConds;
        for (auto& stats : caseBodies) {
            std::vector<std::unique_ptr<A1Stat>> newStats;
            for (auto& stat : stats) {
                newStats.push_back(stat->Clone(parent));
            }
            newNode->caseBodies.push_back(std::move(newStats));
        }
        for (auto& stat : defaultBody) {
            newNode->defaultBody.push_back(stat->Clone(parent));
        }
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("A1StatSwitch");
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

// AST1 declaration node implementation
class A1DeclRaw : public A1Decl { // raw code
    public:
    std::string code;

    A1DeclRaw(): A1Decl(A1DeclType::NONE), code() {}
    A1DeclRaw(A1DeclType t): A1Decl(t), code() {}
    virtual ~A1DeclRaw() = default;

    virtual std::unique_ptr<A1Decl> Clone(A1StatScope* parent) {
        std::unique_ptr<A1DeclRaw> newNode = std::make_unique<A1DeclRaw>(objType);
        newNode->location = location;
        newNode->name = name;
        newNode->type = type ? type->Clone() : nullptr;
        newNode->isExported = isExported;
        newNode->code = code;
        return newNode;
    }

    virtual std::string toString(int indent) { return std::string(indent * 2, ' ') + std::format("A1DeclRaw {} {}", (int)objType, code); }
};

class A1DeclInclude : public A1Decl { // include
    public:
    std::string tgtPath;
    std::string tgtUname;
    std::vector<std::unique_ptr<A1Type>> argTypes;

    A1DeclInclude(): A1Decl(A1DeclType::INCLUDE), tgtPath(), tgtUname(), argTypes() {}
    virtual ~A1DeclInclude() = default;

    virtual std::unique_ptr<A1Decl> Clone(A1StatScope* parent) {
        std::unique_ptr<A1DeclInclude> newNode = std::make_unique<A1DeclInclude>();
        newNode->location = location;
        newNode->name = name;
        newNode->type = type ? type->Clone() : nullptr;
        newNode->isExported = isExported;
        newNode->tgtPath = tgtPath;
        newNode->tgtUname = tgtUname;
        for (auto& argType : argTypes) {
            newNode->argTypes.push_back(argType->Clone());
        }
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("A1DeclInclude {} {}", tgtPath, tgtUname);
        for (size_t i = 0; i < argTypes.size(); i++) {
            result += "\n" + std::string(indent * 2, ' ') + std::format("arg{}: {}", i, argTypes[i]->toString(indent + 1));
        }
        return result;
    }
};

class A1DeclTemplate : public A1Decl { // template
    public:
    std::unique_ptr<A1Type> body;

    A1DeclTemplate(): A1Decl(A1DeclType::TEMPLATE), body() {}
    virtual ~A1DeclTemplate() = default;

    virtual std::unique_ptr<A1Decl> Clone(A1StatScope* parent) {
        std::unique_ptr<A1DeclTemplate> newNode = std::make_unique<A1DeclTemplate>();
        newNode->location = location;
        newNode->name = name;
        newNode->body = body->Clone();
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("A1DeclTemplate {} {}", (int)objType, name);
        return result;
    }
};

class A1DeclVar : public A1Decl { // variable declaration
    public:
    std::unique_ptr<A1Expr> init;
    bool isDefine;
    bool isConst;
    bool isVolatile;
    bool isExtern;
    bool isParam;

    A1DeclVar(): A1Decl(A1DeclType::VAR), init(), isDefine(false), isConst(false), isVolatile(false), isExtern(false), isParam(false) {}
    A1DeclVar(std::unique_ptr<A1Type> t, std::string nm): A1Decl(A1DeclType::VAR), init(), isDefine(false), isConst(false), isVolatile(false), isExtern(false), isParam(false) { name = nm; type = std::move(t); }
    virtual ~A1DeclVar() = default;

    virtual std::unique_ptr<A1Decl> Clone(A1StatScope* parent) {
        std::unique_ptr<A1DeclVar> newNode = std::make_unique<A1DeclVar>();
        newNode->location = location;
        newNode->name = name;
        newNode->type = type ? type->Clone() : nullptr;
        newNode->isExported = isExported;
        if (init) newNode->init = init->Clone();
        newNode->isDefine = isDefine;
        newNode->isConst = isConst;
        newNode->isVolatile = isVolatile;
        newNode->isExtern = isExtern;
        newNode->isParam = isParam;
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("A1DeclVar {} {}", (int)objType, name);
        if (init) result += "\n" + init->toString(indent + 1);
        return result;
    }
};

class A1DeclFunc : public A1Decl { // function declaration
    public:
    std::string structNm; // for method
    std::string funcNm; // for method
    std::vector<std::unique_ptr<A1Type>> paramTypes;
    std::vector<std::string> paramNames;
    std::unique_ptr<A1Type> retType;
    std::unique_ptr<A1StatScope> body; // have param declarations
    bool isVaArg;
    // bool isExported; // shadowed from A1Decl

    A1DeclFunc(): A1Decl(A1DeclType::FUNC), structNm(), funcNm(), paramTypes(), paramNames(), retType(), body(), isVaArg(false) {}
    virtual ~A1DeclFunc() = default;

    virtual std::unique_ptr<A1Decl> Clone(A1StatScope* parent) {
        std::unique_ptr<A1DeclFunc> newNode = std::make_unique<A1DeclFunc>();
        newNode->location = location;
        newNode->name = name;
        newNode->type = type ? type->Clone() : nullptr;
        newNode->isExported = isExported;
        newNode->structNm = structNm;
        newNode->funcNm = funcNm;
        for (auto& paramType : paramTypes) {
            newNode->paramTypes.push_back(paramType->Clone());
        }
        newNode->paramNames = paramNames;
        newNode->retType = retType->Clone();
        if (body) newNode->body = std::unique_ptr<A1StatScope>(static_cast<A1StatScope*>(body->Clone(parent).release()));
        newNode->isVaArg = isVaArg;
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("A1DeclFunc {} {}", (int)objType, name);
        for (size_t i = 0; i < paramTypes.size(); i++) {
            result += "\n" + std::string(indent * 2, ' ') + std::format("param {}:", i);
            result += "\n" + paramTypes[i]->toString(indent + 1);
        }
        if (retType) result += "\n" + retType->toString(indent + 1);
        if (body) result += "\n" + body->toString(indent + 1);
        return result;
    }
};

class A1DeclStruct : public A1Decl { // struct declaration
    public:
    int structSize; // total size in bytes
    int structAlign; // align requirement in bytes
    std::vector<std::unique_ptr<A1Type>> memTypes;
    std::vector<std::string> memNames;
    std::vector<int> memOffsets;

    A1DeclStruct(): A1Decl(A1DeclType::STRUCT), structSize(-1), structAlign(-1), memTypes(), memNames(), memOffsets() {}
    virtual ~A1DeclStruct() = default;

    virtual std::unique_ptr<A1Decl> Clone(A1StatScope* parent) {
        std::unique_ptr<A1DeclStruct> newNode = std::make_unique<A1DeclStruct>();
        newNode->location = location;
        newNode->name = name;
        newNode->type = type ? type->Clone() : nullptr;
        newNode->isExported = isExported;
        newNode->structSize = structSize;
        newNode->structAlign = structAlign;
        for (auto& member : memTypes) {
            newNode->memTypes.push_back(member->Clone());
        }
        newNode->memNames = memNames;
        newNode->memOffsets = memOffsets;
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("A1DeclStruct {} {}", (int)objType, name);
        for (size_t i = 0; i < memTypes.size(); i++) {
            result += "\n" + std::string(indent * 2, ' ') + std::format("member {}:", i);
            result += "\n" + memTypes[i]->toString(indent + 1);
        }
        return result;
    }
};

class A1DeclEnum : public A1Decl { // enum declaration
    public:
    int enumSize; // size in bytes
    std::vector<std::string> memNames;
    std::vector<int64_t> memValues;

    A1DeclEnum(): A1Decl(A1DeclType::ENUM), enumSize(-1), memNames(), memValues() {}
    virtual ~A1DeclEnum() = default;

    virtual std::unique_ptr<A1Decl> Clone(A1StatScope* parent) {
        std::unique_ptr<A1DeclEnum> newNode = std::make_unique<A1DeclEnum>();
        newNode->location = location;
        newNode->name = name;
        newNode->type = type ? type->Clone() : nullptr;
        newNode->isExported = isExported;
        newNode->enumSize = enumSize;
        newNode->memNames = memNames;
        newNode->memValues = memValues;
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("A1DeclEnum {}", (int)objType, name);
        for (size_t i = 0; i < memNames.size(); i++) {
            result += "\n" + std::string(indent * 2, ' ') + std::format("member {}: {}", i, memNames[i]);
        }
        return result;
    }
};

// single source file module
class A1Module {
    public:
    std::string path;
    std::string uname; // for non-duplicate compile
    std::unique_ptr<A1StatScope> code;
    std::vector<std::unique_ptr<A1Type>> tmpArgs; // template arguments with uname.name format
    int tmpArgsCount; // number of template argument required
    bool isFinished;

    A1Module(): path(""), uname(""), code(), tmpArgsCount(0), isFinished(false) {}
    A1Module(const std::string& fpath): path(fpath), uname(""), code(), tmpArgsCount(0), isFinished(false) {}
    A1Module(const std::string& fpath, const std::string& uname): path(fpath), uname(uname), code(), tmpArgsCount(0), isFinished(false) {}

    std::unique_ptr<A1Module> Clone() {
        std::unique_ptr<A1Module> result = std::make_unique<A1Module>(path);
        result->uname = uname;
        if (code) result->code = std::unique_ptr<A1StatScope>(static_cast<A1StatScope*>(code->Clone(nullptr).release()));
        for (auto& arg : tmpArgs) result->tmpArgs.push_back(arg->Clone());
        result->tmpArgsCount = tmpArgsCount;
        result->isFinished = isFinished;
        return result;
    }

    std::string toString() {
        std::string result = std::format("A1Module {} {}", path, uname);
        if (code) result += "\n" + code->toString(0);
        return result;
    }

    A1Decl* findDeclaration(const std::string& name, bool checkExported); // find global declarations by name
    A1Decl* findDeclaration(const std::string& name, A1DeclType type, bool checkExported); // find global declarations by name and type
    Literal findLiteral(const std::string& name, bool checkExported); // find global enum, defined literal by name
    std::string isNameUsable(const std::string& name, Location loc); // check if name is usable for global declarations
    std::unique_ptr<A1Type> parseType(TokenProvider& tp, A1StatScope& current, int arch); // parse type
};

// AST1 generator
class A1Gen {
    public:
    CompileMessage prt;
    int arch; // target architecture in bytes
    std::vector<std::unique_ptr<A1Module>> modules; // parse result

    A1Gen(): prt(3), arch(8), modules() {}
    A1Gen(int p, int a): prt(p), arch(a), modules() {}

    std::string toString() {
        std::string result = "A1Gen";
        for (auto& mod : modules) result += "\n\n\n" + mod->toString();
        return result;
    }

    std::string getLocString(Location loc) { return std::format("{}:{}", modules[loc.srcLoc]->path, loc.line); } // get location string
    int findModule(const std::string& path); // find module by path

    std::string parse(const std::string& path, int nameCut); // parse file

    private:
    bool isTypeStart(TokenProvider& tp, A1Module& mod); // check if token is type start
    Literal foldNode(A1Expr& tgt, A1StatScope& current, A1Module& mod); // fold node to literal, NONE if not foldable

    std::unique_ptr<A1StatRaw> parseRawCode(TokenProvider& tp); // raw_c, raw_ir
    std::unique_ptr<A1DeclStruct> parseStruct(TokenProvider& tp, A1StatScope& current, A1Module& mod, bool isExported); // struct declaration
    std::unique_ptr<A1DeclEnum> parseEnum(TokenProvider& tp, A1StatScope& current, A1Module& mod, bool isExported); // enum declaration
    std::unique_ptr<A1DeclFunc> parseFunc(TokenProvider& tp, A1StatScope& current, A1Module& mod, std::unique_ptr<A1Type> retType, bool isVaArg, bool isExported); // function declaration
    
    std::unique_ptr<A1Expr> parseAtomicExpr(TokenProvider& tp, A1StatScope& current, A1Module& mod); // atomic expression (primary expression)
    std::unique_ptr<A1Expr> parsePrattExpr(TokenProvider& tp, A1StatScope& current, A1Module& mod, int level); // pratt expression (binary, tertiary)
    std::unique_ptr<A1Expr> parseExpr(TokenProvider& tp, A1StatScope& current, A1Module& mod); // parse expression, fold if possible

    std::unique_ptr<A1DeclVar> parseVarDecl(TokenProvider& tp, A1StatScope& current, A1Module& mod, std::unique_ptr<A1Type> varType, bool isDefine, bool isConst, bool isVolatile, bool isExtern, bool isExported); // variable declaration, after type declaration
    std::unique_ptr<A1StatAssign> parseVarAssign(TokenProvider& tp, A1StatScope& current, A1Module& mod, std::unique_ptr<A1Expr> lvalue, A1StatAssignType assignType, TokenType endExpect); // variable assignment, after lvalue =
    std::unique_ptr<A1Stat> parseStatement(TokenProvider& tp, A1StatScope& current, A1Module& mod); // parse general statement
    std::unique_ptr<A1StatScope> parseScope(TokenProvider& tp, A1StatScope& current, A1Module& mod); // parse scope
    std::unique_ptr<A1StatDecl> parseTopLevel(TokenProvider& tp, A1StatScope& current, A1Module& mod); // parse toplevel declaration

    bool completeType(A1Module& mod, A1Type& tgt); // calculate type size, return true if modified
    bool completeStruct(A1Module& mod, A1DeclStruct& tgt); // calculate struct size, return true if modified
};

#endif // AST1_H
