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
    // expression
    LITERAL,
    LITERAL_ARRAY,
    NAME,
    TRIPLE_OP,
    BINARY_OP,
    UNARY_OP,
    FUNC_CALL,
    // type node
    TYPE,
    // long statement
    DECL_VAR,
    ASSIGN,
    // short statement
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
    ASTNodeType type;
    Location location;
    std::string text; // used for name or raw code

    ASTNode(): type(ASTNodeType::NONE), location(), text("") {}
    ASTNode(ASTNodeType t): type(t), location(), text("") {}
    ASTNode(ASTNodeType tp, const std::string& tx): type(tp), location(), text(tx) {}
    virtual ~ASTNode() = default;

    virtual std::string toString(int indent) { return std::string(indent, '  ') + std::format("AST {} {}", (int)type, text); }
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

// literal node
class LiteralNode: public ASTNode {
    public:
    Literal literal; // atom value

    LiteralNode(): ASTNode(ASTNodeType::LITERAL), literal() {}

    std::string toString(int indent) { return std::string(indent, '  ') + std::format("LITERAL {}", literal.toString()); }
};

// literal array node
class LiteralArrayNode: public ASTNode {
    public:
    std::vector<std::unique_ptr<ASTNode>> elements; // array element expressions

    LiteralArrayNode(): ASTNode(ASTNodeType::LITERAL_ARRAY), elements() {}

    std::string toString(int indent) {
        std::string result = std::string(indent, '  ') + "LITERAL_ARRAY";
        for (auto& element : elements) {
            result += "\n" + element->toString(indent + 1);
        }
        return result;
    }
};

// name node
class NameNode: public ASTNode {
    public:
    std::string& name; // name of variable, function, member, enum

    NameNode(): ASTNode(ASTNodeType::NAME), name(text) {}

    std::string toString(int indent) { return std::string(indent, '  ') + std::format("NAME {}", name); }
};

// operator node, ordered by priority
enum class OperatorType {
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

class TripleOpNode: public ASTNode {
    public:
    OperatorType subtype;
    std::unique_ptr<ASTNode> base;
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;

    TripleOpNode(): ASTNode(ASTNodeType::TRIPLE_OP), subtype(OperatorType::NONE), base(nullptr), left(nullptr), right(nullptr) {}
    TripleOpNode(OperatorType tp): ASTNode(ASTNodeType::TRIPLE_OP), subtype(tp), base(nullptr), left(nullptr), right(nullptr) {}

    std::string toString(int indent) {
        std::string result = std::string(indent, '  ') + std::format("TRIPLE_OP {}", subtype);
        result += "\n" + base->toString(indent + 1);
        result += "\n" + left->toString(indent + 1);
        result += "\n" + right->toString(indent + 1);
        return result;
    }
};

class BinaryOpNode: public ASTNode {
    public:
    OperatorType subtype;
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;

    BinaryOpNode(): ASTNode(ASTNodeType::BINARY_OP), subtype(OperatorType::NONE), left(nullptr), right(nullptr) {}
    BinaryOpNode(OperatorType tp): ASTNode(ASTNodeType::BINARY_OP), subtype(tp), left(nullptr), right(nullptr) {}

    std::string toString(int indent) {
        std::string result = std::string(indent, '  ') + std::format("BINARY_OP {}", subtype);
        result += "\n" + left->toString(indent + 1);
        result += "\n" + right->toString(indent + 1);
        return result;
    }
};

class UnaryOpNode: public ASTNode {
    public:
    OperatorType subtype;
    std::unique_ptr<ASTNode> operand;

    UnaryOpNode(): ASTNode(ASTNodeType::UNARY_OP), subtype(OperatorType::NONE), operand(nullptr) {}
    UnaryOpNode(OperatorType tp): ASTNode(ASTNodeType::UNARY_OP), subtype(tp), operand(nullptr) {}

    std::string toString(int indent) {
        std::string result = std::string(indent, '  ') + std::format("UNARY_OP {}", subtype);
        result += "\n" + operand->toString(indent + 1);
        return result;
    }
};

// function call node
class FuncCallNode: public ASTNode {
    public:
    std::unique_ptr<ASTNode> func_expr; // function expression
    std::vector<std::unique_ptr<ASTNode>> args; // argument expressions

    FuncCallNode(): ASTNode(ASTNodeType::FUNC_CALL), func_expr(nullptr), args() {}

    std::string toString(int indent) {
        std::string result = std::string(indent, '  ') + "FUNC_CALL";
        result += "\n" + func_expr->toString(indent + 1);
        for (auto& arg : args) {
            result += "\n" + arg->toString(indent + 1);
        }
        return result;
    }
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
    TypeNodeType subtype;
    std::string& name; // type name
    std::string include_tgt; // include name for FOREIGN type
    std::unique_ptr<TypeNode> direct; // ptr, arr, slice target & func return
    std::vector<std::unique_ptr<TypeNode>> indirect; // func args
    int64_t length; // array length
    int type_size; // total size in bytes
    int type_align; // align requirement in bytes

    TypeNode(): ASTNode(ASTNodeType::TYPE), subtype(TypeNodeType::NONE), name(text), include_tgt(""), direct(nullptr), indirect(), length(-1) {}
    TypeNode(TypeNodeType tp, const std::string& nm): ASTNode(ASTNodeType::TYPE, nm), subtype(tp), name(text), include_tgt(""), direct(nullptr), indirect(), length(-1) {}

    std::string toString(int indent) {
        std::string result = std::string(indent, '  ') + std::format("TYPE {} {} {} {} {} {}", name, include_tgt, subtype, length, type_size, type_align);
        if (direct) result += "\n" + direct->toString(indent + 1);
        for (auto& ind : indirect) {
            result += "\n" + ind->toString(indent + 1);
        }
        return result;
    }

    std::string toString() {
        std::string result;
        switch(subtype) {
            case TypeNodeType::PRIMITIVE: case TypeNodeType::NAME:
                result = name;
                break;
            case TypeNodeType::FOREIGN:
                result = include_tgt + "." + name;
                break;
            case TypeNodeType::POINTER: case TypeNodeType::ARRAY: case TypeNodeType::SLICE:
                result = direct->toString() + name;
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

// statement node
class LongStatNode: public ASTNode {
    public:
    std::unique_ptr<TypeNode> varType; // declare type
    std::unique_ptr<ASTNode> varName; // declare name, assign lvalue
    std::unique_ptr<ASTNode> varExpr; // initialize, assign rvalue
    bool isDefine;
    bool isConst;
    bool isVolatile;
    bool isExtern;
    bool isExported;

    LongStatNode(): ASTNode(ASTNodeType::NONE), varType(nullptr), varName(nullptr), varExpr(nullptr), isDefine(false), isConst(false), isVolatile(false), isExtern(false), isExported(false) {}
    LongStatNode(ASTNodeType tp): ASTNode(tp), varType(nullptr), varName(nullptr), varExpr(nullptr), isDefine(false), isConst(false), isVolatile(false), isExtern(false), isExported(false) {}

    std::string toString(int indent) {
        std::string result = std::string(indent, '  ') + std::format("LONGSTAT {} {} {} {} {} {}", type, isDefine, isConst, isVolatile, isExtern, isExported);
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
        std::string result = std::string(indent, '  ') + std::format("SHORTSTAT {}", type);
        if (statExpr) result += "\n" + statExpr->toString(indent + 1);
        return result;
    }
};

// control statement node
class ScopeNode: public ASTNode {
    public:
    std::vector<std::unique_ptr<ASTNode>> body;
    ASTNode* parent;

    ScopeNode(): ASTNode(ASTNodeType::SCOPE), body(), parent(nullptr) {}

    std::string toString(int indent) {
        std::string result = std::string(indent, '  ') + "SCOPE";
        for (auto& node : body) {
            result += "\n" + node->toString(indent + 1);
        }
        return result;
    }

    LongStatNode* findVarByName(const std::string& name); // find variable declaration, nullptr if not found
    Literal findDefinedLiteral(const std::string& name); // find defined literal variable, type NONE if not found
};

class IfNode: public ASTNode {
    public:
    std::unique_ptr<ASTNode> cond;
    std::unique_ptr<ASTNode> flowBody;
    std::unique_ptr<ASTNode> elseBody;

    IfNode(): ASTNode(ASTNodeType::IF), cond(nullptr), flowBody(nullptr), elseBody(nullptr) {}

    std::string toString(int indent) {
        std::string result = std::string(indent, '  ') + "IF";
        if (cond) result += "\n" + cond->toString(indent + 1);
        if (flowBody) result += "\n" + flowBody->toString(indent + 1);
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
    std::vector<std::unique_ptr<ASTNode>> case_exprs;
    std::vector<std::unique_ptr<ASTNode>> case_bodies;
    std::unique_ptr<ASTNode> defaultBody;

    SwitchNode(): ASTNode(ASTNodeType::SWITCH), cond(nullptr), case_exprs(), case_bodies(), defaultBody(nullptr) {}

    std::string toString(int indent) {
        std::string result = std::string(indent, '  ') + "SWITCH";
        if (cond) result += "\n" + cond->toString(indent + 1);
        for (auto& expr : case_exprs) result += "\n" + expr->toString(indent + 1);
        for (auto& body : case_bodies) result += "\n" + body->toString(indent + 1);
        if (defaultBody) result += "\n" + defaultBody->toString(indent + 1);
        return result;
    }
};

// declaration nodes
class DeclFuncNode: public ASTNode {
    public:
    std::string& full_name; // mangled name (struct.func)
    std::string struct_name; // for method
    std::string func_name; // for method
    std::vector<std::unique_ptr<TypeNode>> param_types;
    std::vector<std::string> param_names;
    std::unique_ptr<TypeNode> return_type;
    std::unique_ptr<ScopeNode> body;
    bool isVaArg;
    bool isExported;

    DeclFuncNode(): ASTNode(ASTNodeType::DECL_FUNC), full_name(text), struct_name(""), func_name(""), param_types(), param_names(), return_type(nullptr), body(nullptr), isVaArg(false), isExported(false) {}

    std::string toString(int indent) {
        std::string result = std::string(indent, '  ') + std::format("DECLFUNC {} {} {}", full_name, isVaArg, isExported);
        for (auto& type : param_types) result += "\n" + type->toString(indent + 1);
        if (return_type) result += "\n" + return_type->toString(indent + 1);
        if (body) result += "\n" + body->toString(indent + 1);
        return result;
    }
};

class DeclStructNode: public ASTNode {
    public:
    std::string& struct_name;
    int struct_size; // total size in bytes
    int struct_align; // align requirement in bytes
    std::vector<std::unique_ptr<TypeNode>> member_types;
    std::vector<std::string> member_names;
    std::vector<int> member_offsets;
    bool isExported;

    DeclStructNode(): ASTNode(ASTNodeType::DECL_STRUCT), struct_name(text), struct_size(-1), struct_align(-1), member_types(), member_names(), member_offsets(), isExported(false) {}

    std::string toString(int indent) {
        std::string result = std::string(indent, '  ') + std::format("DECLSTRUCT {} {} {} {}", struct_name, struct_size, struct_align, isExported);
        for (auto& type : member_types) result += "\n" + type->toString(indent + 1);
        for (auto& name : member_names) result += "\n" + name;
        for (auto& offset : member_offsets) result += "\n" + std::to_string(offset);
        return result;
    }
};

class DeclEnumNode: public ASTNode {
    public:
    std::string& enum_name;
    int enum_size; // size in bytes
    std::vector<std::string> member_names;
    std::vector<int64_t> member_values;
    bool isExported;

    DeclEnumNode(): ASTNode(ASTNodeType::DECL_ENUM), enum_name(text), enum_size(-1), member_names(), member_values(), isExported(false) {}

    std::string toString(int indent) {
        std::string result = std::string(indent, '  ') + std::format("DECLENUM {} {} {}", enum_name, enum_size, isExported);
        for (auto& name : member_names) result += "\n" + name;
        for (auto& value : member_values) result += "\n" + std::to_string(value);
        return result;
    }
};

// abstract source file
class SrcFile {
    public:
    std::string path;
    std::string unique_name; // for non-duplicate compile
    std::unique_ptr<ScopeNode> nodes;
    bool isFinished;

    SrcFile(): path(""), unique_name(""), nodes(), isFinished(false) {}
    SrcFile(const std::string& fpath): path(fpath), unique_name(""), nodes(), isFinished(false) {}
    SrcFile(const std::string& fpath, const std::string& uname): path(fpath), unique_name(uname), nodes(), isFinished(false) {}

    std::string toString() {
        std::string result = std::format("SrcFile {} {}", path, unique_name);
        if (nodes) result += "\n" + nodes->toString(0);
        return result;
    }

    ASTNode* findNodeByName(ASTNodeType tp, const std::string& name, bool checkExported); // find include, tmp, var, func, struct, enum
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
    std::string getLocString(const Location& loc) { return std::format("{}:{}", srcFiles[loc.source_id]->path, loc.line); }
    int findSource(const std::string& path); // find source file index, -1 if not found
    bool isTypeStart(TokenProvider& tp, ScopeNode& current, SrcFile& src);
    bool isAssignStart(TokenProvider& tp);

    std::unique_ptr<DeclStructNode> parseStruct(TokenProvider& tp, ScopeNode& current, SrcFile& src, int64_t tag); // parse struct declaration
    std::unique_ptr<DeclEnumNode> parseEnum(TokenProvider& tp, ScopeNode& current, SrcFile& src, int64_t tag); // parse enum declaration
    std::unique_ptr<DeclFuncNode> parseFunc(TokenProvider& tp, ScopeNode& current, SrcFile& src, int64_t tag); // parse function declaration

    std::unique_ptr<ASTNode> parseAtomicExpr(TokenProvider& tp, ScopeNode& current, SrcFile& src); // parse atomic expression
    std::unique_ptr<ASTNode> parsePrattExpr(TokenProvider& tp, ScopeNode& current, SrcFile& src, int level); // parse pratt expression
    std::unique_ptr<LongStatNode> parseVarStat(TokenProvider& tp, ScopeNode& current, SrcFile& src, int64_t tag); // parse variable declaration, assignment

    std::unique_ptr<ASTNode> parseStatement(TokenProvider& tp, ScopeNode& current, SrcFile& src); // parse general statement
    std::unique_ptr<ASTNode> parseTopLevel(TokenProvider& tp, ScopeNode& current, SrcFile& src); // parse toplevel declaration
    std::unique_ptr<ScopeNode> parseScope(TokenProvider& tp, ScopeNode& current, SrcFile& src); // parse scope
};

/*
tag
0x000001 define
0x000010 const
0x000100 volatile
0x001000 extern
0x010000 exported
*/

#endif // ASTGEN_H
