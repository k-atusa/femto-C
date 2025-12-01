#ifndef ASTGEN_H
#define ASTGEN_H

#include <format>
#include "baseFunc.h"
#include "tokenizer.h"

// forward declarations
class ScopeNode;
class TypeNode;

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
    FALL,
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

    virtual std::unique_ptr<ASTNode> Clone(ScopeNode* parent) {
        std::unique_ptr<ASTNode> newNode = std::make_unique<ASTNode>(objType);
        newNode->location = location;
        newNode->text = text;
        return newNode;
    }

    virtual std::string toString(int indent) { return std::string(indent * 2, ' ') + std::format("AST {} {}", (int)objType, text); }
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

    virtual std::unique_ptr<ASTNode> Clone(ScopeNode* parent) {
        return clone();
    }

    std::unique_ptr<TypeNode> clone() {
        std::unique_ptr<TypeNode> newType = std::make_unique<TypeNode>();
        newType->objType = objType;
        newType->location = location;
        newType->text = text;
        newType->subType = subType;
        newType->name = name;
        newType->includeName = includeName;
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

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("TYPE {} {} {} {} {} {}", name, includeName, (int)subType, length, typeSize, typeAlign);
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

// include node
class IncludeNode: public ASTNode {
    public:
    std::string path; // normal or template source file path
    std::string tgtNm; // include target unique name
    std::string& name; // import name
    std::vector<std::unique_ptr<TypeNode>> args; // template type arguments

    IncludeNode(): ASTNode(ASTNodeType::INCLUDE), path(""), tgtNm(text), name(text), args() {}

    virtual std::unique_ptr<ASTNode> Clone(ScopeNode* parent) {
        std::unique_ptr<IncludeNode> newNode = std::make_unique<IncludeNode>();
        newNode->location = location;
        newNode->text = text;
        newNode->path = path;
        newNode->tgtNm = tgtNm;
        for (auto& arg : args) {
            newNode->args.push_back(arg->clone());
        }
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("INCLUDE {} {}", path, name);
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
    int tmpSize;
    int tmpAlign;

    DeclTemplateNode(): ASTNode(ASTNodeType::DECL_TEMPLATE), name(text), tmpSize(-1), tmpAlign(-1) {}

    virtual std::unique_ptr<ASTNode> Clone(ScopeNode* parent) {
        std::unique_ptr<DeclTemplateNode> newNode = std::make_unique<DeclTemplateNode>();
        newNode->location = location;
        newNode->text = text;
        newNode->tmpSize = tmpSize;
        newNode->tmpAlign = tmpAlign;
        return newNode;
    }

    virtual std::string toString(int indent) { return std::string(indent * 2, ' ') + std::format("DECLTMP {}", name); }
};

// raw code node
class RawCodeNode: public ASTNode {
    public:
    std::string& code; // raw code of C or IR

    RawCodeNode(): ASTNode(ASTNodeType::NONE), code(text) {}
    RawCodeNode(ASTNodeType tp): ASTNode(tp), code(text) {}

    virtual std::unique_ptr<ASTNode> Clone(ScopeNode* parent) {
        std::unique_ptr<RawCodeNode> newNode = std::make_unique<RawCodeNode>(objType);
        newNode->location = location;
        newNode->text = text;
        return newNode;
    }

    virtual std::string toString(int indent) { return std::string(indent * 2, ' ') + std::format("RAW {}", code); }
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

    virtual std::unique_ptr<ASTNode> Clone(ScopeNode* parent) {
        std::unique_ptr<AtomicExprNode> newNode = std::make_unique<AtomicExprNode>();
        newNode->objType = objType;
        newNode->location = location;
        newNode->text = text;
        newNode->literal = literal;
        for (auto& element : elements) {
            newNode->elements.push_back(element->Clone(parent));
        }
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ');
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

    virtual std::unique_ptr<ASTNode> Clone(ScopeNode* parent) {
        std::unique_ptr<OperationNode> newNode = std::make_unique<OperationNode>(subType);
        newNode->location = location;
        newNode->text = text;
        if (operand0) newNode->operand0 = operand0->Clone(parent);
        if (operand1) newNode->operand1 = operand1->Clone(parent);
        if (operand2) newNode->operand2 = operand2->Clone(parent);
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("OPERATION {}", (int)subType);
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

    virtual std::unique_ptr<ASTNode> Clone(ScopeNode* parent) {
        std::unique_ptr<FuncCallNode> newNode = std::make_unique<FuncCallNode>();
        newNode->location = location;
        newNode->text = text;
        if (funcExpr) newNode->funcExpr = funcExpr->Clone(parent);
        for (auto& arg : args) {
            newNode->args.push_back(arg->Clone(parent));
        }
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + "FUNC_CALL";
        result += "\n" + funcExpr->toString(indent + 1);
        for (auto& arg : args) {
            result += "\n" + arg->toString(indent + 1);
        }
        return result;
    }
};

// variable declaration
class DeclVarNode: public ASTNode {
    public:
    std::unique_ptr<TypeNode> varType; // declare type
    std::string& name; // declare name
    std::unique_ptr<ASTNode> varExpr; // initialize
    bool isDefine;
    bool isExtern;
    bool isExported;
    bool isParam;

    DeclVarNode(): ASTNode(ASTNodeType::DECL_VAR), varType(nullptr), name(text), varExpr(nullptr), isDefine(false), isExtern(false), isExported(false), isParam(false) {}
    DeclVarNode(std::unique_ptr<TypeNode> vt, const std::string& nm): ASTNode(ASTNodeType::DECL_VAR, nm), varType(std::move(vt)), name(text), varExpr(nullptr), isDefine(false), isExtern(false), isExported(false), isParam(false) {}

    virtual std::unique_ptr<ASTNode> Clone(ScopeNode* parent) {
        std::unique_ptr<DeclVarNode> newNode = std::make_unique<DeclVarNode>(varType->clone(), name);
        newNode->location = location;
        newNode->text = text;
        if (varExpr) newNode->varExpr = varExpr->Clone(parent);
        newNode->isDefine = isDefine;
        newNode->isExtern = isExtern;
        newNode->isExported = isExported;
        newNode->isParam = isParam;
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("VAR_DECL {} {} {} {} {}", name, isDefine, isExtern, isExported, isParam);
        if (varType) result += "\n" + varType->toString(indent + 1);
        if (varExpr) result += "\n" + varExpr->toString(indent + 1);
        return result;
    }
};

// variable assignment
class AssignNode: public ASTNode {
    public:
    std::unique_ptr<ASTNode> lvalue; // assign lvalue
    std::unique_ptr<ASTNode> rvalue; // assign rvalue

    AssignNode(): ASTNode(ASTNodeType::ASSIGN), lvalue(nullptr), rvalue(nullptr) {}

    virtual std::unique_ptr<ASTNode> Clone(ScopeNode* parent) {
        std::unique_ptr<AssignNode> newNode = std::make_unique<AssignNode>();
        newNode->location = location;
        newNode->text = text;
        if (lvalue) newNode->lvalue = lvalue->Clone(parent);
        if (rvalue) newNode->rvalue = rvalue->Clone(parent);
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + "ASSIGN";
        result += "\n" + lvalue->toString(indent + 1);
        result += "\n" + rvalue->toString(indent + 1);
        return result;
    }
};

class ShortStatNode: public ASTNode {
    public:
    std::unique_ptr<ASTNode> statExpr; // for return, defer

    ShortStatNode(): ASTNode(ASTNodeType::NONE), statExpr(nullptr) {}
    ShortStatNode(ASTNodeType tp): ASTNode(tp), statExpr(nullptr) {}

    virtual std::unique_ptr<ASTNode> Clone(ScopeNode* parent) {
        std::unique_ptr<ShortStatNode> newNode = std::make_unique<ShortStatNode>(objType);
        newNode->location = location;
        newNode->text = text;
        if (statExpr) newNode->statExpr = statExpr->Clone(parent);
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("SHORTSTAT {}", (int)objType);
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

    virtual std::unique_ptr<ASTNode> Clone(ScopeNode* parent_g) {
        return clone(parent_g);
    }

    std::unique_ptr<ScopeNode> clone(ScopeNode* parent_g) {
        std::unique_ptr<ScopeNode> newNode = std::make_unique<ScopeNode>(parent_g);
        newNode->location = location;
        newNode->text = text;
        for (auto& node : body) newNode->body.push_back(node->Clone(newNode.get()));
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + "SCOPE";
        for (auto& node : body) result += "\n" + node->toString(indent + 1);
        return result;
    }

    DeclVarNode* findVarByName(const std::string& name); // find variable declaration, nullptr if not found
    Literal findDefinedLiteral(const std::string& name); // find defined literal variable, type NONE if not found
};

class IfNode: public ASTNode {
    public:
    std::unique_ptr<ASTNode> cond;
    std::unique_ptr<ASTNode> ifBody;
    std::unique_ptr<ASTNode> elseBody;

    IfNode(): ASTNode(ASTNodeType::IF), cond(nullptr), ifBody(nullptr), elseBody(nullptr) {}

    virtual std::unique_ptr<ASTNode> Clone(ScopeNode* parent) {
        std::unique_ptr<IfNode> newNode = std::make_unique<IfNode>();
        newNode->location = location;
        newNode->text = text;
        if (cond) newNode->cond = cond->Clone(parent);
        if (ifBody) newNode->ifBody = ifBody->Clone(parent);
        if (elseBody) newNode->elseBody = elseBody->Clone(parent);
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + "IF";
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

    virtual std::unique_ptr<ASTNode> Clone(ScopeNode* parent) {
        std::unique_ptr<WhileNode> newNode = std::make_unique<WhileNode>();
        newNode->location = location;
        newNode->text = text;
        if (cond) newNode->cond = cond->Clone(parent);
        if (body) newNode->body = body->Clone(parent);
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + "WHILE";
        if (cond) result += "\n" + cond->toString(indent + 1);
        if (body) result += "\n" + body->toString(indent + 1);
        return result;
    }
};

class ForNode: public ASTNode {
    public:
    // init statement will be at another outside scope
    std::unique_ptr<ASTNode> cond;
    std::unique_ptr<ASTNode> body;
    std::unique_ptr<ASTNode> step;

    ForNode(): ASTNode(ASTNodeType::FOR), cond(nullptr), body(nullptr), step(nullptr) {}

    virtual std::unique_ptr<ASTNode> Clone(ScopeNode* parent) {
        std::unique_ptr<ForNode> newNode = std::make_unique<ForNode>();
        newNode->location = location;
        newNode->text = text;
        if (cond) newNode->cond = cond->Clone(parent);
        if (body) newNode->body = body->Clone(parent);
        if (step) newNode->step = step->Clone(parent);
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + "FOR_BODY";
        if (cond) result += "\n" + cond->toString(indent + 1);
        if (body) result += "\n" + body->toString(indent + 1);
        if (step) result += "\n" + step->toString(indent + 1);
        return result;
    }
};

class SwitchNode: public ASTNode {
    public:
    std::unique_ptr<ASTNode> cond;
    std::vector<int64_t> caseConds;
    std::vector<std::vector<std::unique_ptr<ASTNode>>> caseBodies;
    std::vector<std::unique_ptr<ASTNode>> defaultBody;

    SwitchNode(): ASTNode(ASTNodeType::SWITCH), cond(nullptr), caseConds(), caseBodies(), defaultBody() {}

    virtual std::unique_ptr<ASTNode> Clone(ScopeNode* parent) {
        std::unique_ptr<SwitchNode> newNode = std::make_unique<SwitchNode>();
        newNode->location = location;
        newNode->text = text;
        if (cond) newNode->cond = cond->Clone(parent);
        for (auto& cond : caseConds) {
            newNode->caseConds.push_back(cond);
        }
        for (auto& body : caseBodies) {
            std::vector<std::unique_ptr<ASTNode>> newBody;
            for (auto& stmt : body) {
                newBody.push_back(stmt->Clone(parent));
            }
            newNode->caseBodies.push_back(std::move(newBody));
        }
        for (auto& stmt : defaultBody) {
            newNode->defaultBody.push_back(stmt->Clone(parent));
        }
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + "SWITCH";
        if (cond) result += "\n" + cond->toString(indent + 1);
        for (size_t i = 0; i < caseConds.size(); i++) {
            result += "\n" + std::string((indent + 1) * 2, ' ') + std::to_string(caseConds[i]);
            for (auto& stmt : caseBodies[i]) {
                result += "\n" + stmt->toString(indent + 1);
            }
        }
        result += "\n" + std::string((indent + 1) * 2, ' ') + "_";
        for (auto& stmt : defaultBody) {
            result += "\n" + stmt->toString(indent + 1);
        }
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
    std::unique_ptr<ScopeNode> body; // have param declarations
    bool isVaArg;
    bool isExported;

    DeclFuncNode(): ASTNode(ASTNodeType::DECL_FUNC), name(text), structNm(""), funcNm(""), paramTypes(), paramNames(), retType(nullptr), body(), isVaArg(false), isExported(false) { body = nullptr; }

    virtual std::unique_ptr<ASTNode> Clone(ScopeNode* parent) {
        std::unique_ptr<DeclFuncNode> newNode = std::make_unique<DeclFuncNode>();
        newNode->location = location;
        newNode->text = text;
        newNode->structNm = structNm;
        newNode->funcNm = funcNm;
        for (auto& type : paramTypes) newNode->paramTypes.push_back(type->clone());
        for (auto& nm : paramNames) newNode->paramNames.push_back(nm);
        if (retType) newNode->retType = retType->clone();
        if (body) newNode->body = body->clone(parent);
        newNode->isVaArg = isVaArg;
        newNode->isExported = isExported;
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("DECLFUNC {} {} {}", name, isVaArg, isExported);
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

    virtual std::unique_ptr<ASTNode> Clone(ScopeNode* parent) {
        std::unique_ptr<DeclStructNode> newNode = std::make_unique<DeclStructNode>();
        newNode->location = location;
        newNode->text = text;
        newNode->structSize = structSize;
        newNode->structAlign = structAlign;
        for (auto& type : memTypes) newNode->memTypes.push_back(type->clone());
        for (auto& name : memNames) newNode->memNames.push_back(name);
        for (auto& offset : memOffsets) newNode->memOffsets.push_back(offset);
        newNode->isExported = isExported;
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("DECLSTRUCT {} {} {} {}", name, structSize, structAlign, isExported);
        for (size_t i = 0; i < memNames.size(); i++) {
            result += "\n" + std::string((indent + 1) * 2, ' ') + std::to_string(memOffsets[i]) + "\n" + memTypes[i]->toString(indent + 1);
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

    virtual std::unique_ptr<ASTNode> Clone(ScopeNode* parent) {
        std::unique_ptr<DeclEnumNode> newNode = std::make_unique<DeclEnumNode>();
        newNode->location = location;
        newNode->text = text;
        newNode->enumSize = enumSize;
        for (auto& name : memNames) newNode->memNames.push_back(name);
        for (auto& value : memValues) newNode->memValues.push_back(value);
        newNode->isExported = isExported;
        return newNode;
    }

    virtual std::string toString(int indent) {
        std::string result = std::string(indent * 2, ' ') + std::format("DECLENUM {} {} {}", name, enumSize, isExported);
        for (size_t i = 0; i < memNames.size(); i++) {
            result += "\n" + std::string((indent + 1) * 2, ' ') + memNames[i] + " " + std::to_string(memValues[i]);
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
    bool isTemplate;
    bool isFinished;

    SrcFile(): path(""), uniqueName(""), isTemplate(false), isFinished(false), code() { code = std::make_unique<ScopeNode>(nullptr); }
    SrcFile(const std::string& fpath): path(fpath), uniqueName(""), isTemplate(false), isFinished(false), code() { code = std::make_unique<ScopeNode>(nullptr); }
    SrcFile(const std::string& fpath, const std::string& uname): path(fpath), uniqueName(uname), isTemplate(false), isFinished(false), code() { code = std::make_unique<ScopeNode>(nullptr); }

    std::unique_ptr<SrcFile> Clone() {
        std::unique_ptr<SrcFile> result = std::make_unique<SrcFile>(path, uniqueName);
        result->code = code->clone(nullptr);
        result->isTemplate = isTemplate;
        result->isFinished = isFinished;
        return result;
    }

    std::string toString() {
        std::string result = std::format("SrcFile {} {}", path, uniqueName);
        if (code) result += "\n" + code->toString(0);
        return result;
    }

    ASTNode* findNodeByName(ASTNodeType tp, const std::string& name, bool checkExported); // find toplevel node by name (include, tmp, var, func, struct, enum)
    Literal findConstByName(const std::string& name, bool checkExported); // find defined literal or enum member, type NONE if not found
    std::string isNameUsable(const std::string& name, Location loc); // check if name is usable at toplevel, return error message or empty if ok
    std::unique_ptr<TypeNode> parseType(TokenProvider& tp, ScopeNode& current, int arch); // parse type from tokens
};

// parser class
class ASTGen {
    public:
    CompileMessage prt;
    int arch; // target architecture in bytes
    std::vector<std::unique_ptr<SrcFile>> srcFiles; // parse result

    ASTGen(): prt(3), arch(8), srcFiles() {}
    ASTGen(int p, int a): prt(p), arch(a), srcFiles() {}

    std::string toString() {
        std::string result = "ASTGen";
        for (auto& src : srcFiles) result += "\n\n\n" + src->toString();
        return result;
    }

    int findSource(const std::string& path); // find source file index, -1 if not found
    std::string parse(const std::string& path, int nameCut = -1); // returns error message or empty if ok

    private:
    std::string getLocString(const Location& loc) { return std::format("{}:{}", srcFiles[loc.srcLoc]->path, loc.line); }
    bool isTypeStart(TokenProvider& tp, SrcFile& src);
    Literal foldNode(ASTNode& tgt, ScopeNode& current, SrcFile& src); // constant folding for name & oper, type NONE if not folded

    std::unique_ptr<RawCodeNode> parseRawCode(TokenProvider& tp); // parse raw code
    std::unique_ptr<DeclStructNode> parseStruct(TokenProvider& tp, ScopeNode& current, SrcFile& src, bool isExported); // parse struct declaration
    std::unique_ptr<DeclEnumNode> parseEnum(TokenProvider& tp, ScopeNode& current, SrcFile& src, bool isExported); // parse enum declaration
    std::unique_ptr<DeclFuncNode> parseFunc(TokenProvider& tp, ScopeNode& current, SrcFile& src, std::unique_ptr<TypeNode> retType, bool isVaArg, bool isExported); // parse function declaration

    std::unique_ptr<ASTNode> parseAtomicExpr(TokenProvider& tp, ScopeNode& current, SrcFile& src); // parse atomic expression
    std::unique_ptr<ASTNode> parsePrattExpr(TokenProvider& tp, ScopeNode& current, SrcFile& src, int level); // parse pratt expression
    std::unique_ptr<ASTNode> parseExpr(TokenProvider& tp, ScopeNode& current, SrcFile& src); // parse expression, fold if possible

    std::unique_ptr<DeclVarNode> parseVarDecl(TokenProvider& tp, ScopeNode& current, SrcFile& src, std::unique_ptr<TypeNode> varType, bool isDefine, bool isExtern, bool isExported); // parse variable declaration
    std::unique_ptr<AssignNode> parseVarAssign(TokenProvider& tp, ScopeNode& current, SrcFile& src, std::unique_ptr<ASTNode> lvalue, TokenType endExpect); // parse variable assignment
    std::unique_ptr<ASTNode> parseStatement(TokenProvider& tp, ScopeNode& current, SrcFile& src); // parse general statement
    std::unique_ptr<ScopeNode> parseScope(TokenProvider& tp, ScopeNode& current, SrcFile& src); // parse scope
    std::unique_ptr<ASTNode> parseTopLevel(TokenProvider& tp, ScopeNode& current, SrcFile& src); // parse toplevel declaration

    bool completeType(SrcFile& src, TypeNode& tgt); // calculate type size, return true if modified
    bool completeStruct(SrcFile& src, DeclStructNode& tgt); // complete struct size, return true if modified
};

#endif // ASTGEN_H
