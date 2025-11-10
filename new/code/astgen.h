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
    FUNC_C,
    FUNC_IR,
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
    DEFINE,
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

    std::string toString() const { return std::to_string((int)type) + " " + text; }
};

// include node
class IncludeNode: public ASTNode {
    public:
    std::string path; // normal or template source file path
    std::string& name; // import name
    std::vector<std::unique_ptr<TypeNode>> args; // template type arguments

    IncludeNode(): ASTNode(ASTNodeType::INCLUDE), path(""), name(text), args() {}
};

// template variable node
class DeclTemplateNode: public ASTNode {
    public:
    std::vector<std::string> arg_names; // template type arguments names

    DeclTemplateNode(): ASTNode(ASTNodeType::DECL_TEMPLATE), arg_names() {}
};

// raw code node
class RawCodeNode: public ASTNode {
    public:
    std::string& code; // raw code, raw function name
    std::vector<std::unique_ptr<ASTNode>> args; // raw function arguments

    RawCodeNode(): ASTNode(ASTNodeType::NONE), code(text), args() {}
    RawCodeNode(ASTNodeType tp): ASTNode(tp), code(text), args() {}
};

// literal node
class LiteralNode: public ASTNode {
    public:
    Literal literal; // atom value

    LiteralNode(): ASTNode(ASTNodeType::LITERAL), literal() {}
};

// literal array node
class LiteralArrayNode: public ASTNode {
    public:
    std::vector<std::unique_ptr<ASTNode>> elements; // array element expressions

    LiteralArrayNode(): ASTNode(ASTNodeType::LITERAL_ARRAY), elements() {}
};

// name node
class NameNode: public ASTNode {
    public:
    std::string& name; // variable or function name

    NameNode(): ASTNode(ASTNodeType::NAME), name(text) {}
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
    std::unique_ptr<ASTNode> expr0;
    std::unique_ptr<ASTNode> expr1;
    std::unique_ptr<ASTNode> expr2;

    TripleOpNode(): ASTNode(ASTNodeType::TRIPLE_OP), subtype(OperatorType::NONE), expr0(nullptr), expr1(nullptr), expr2(nullptr) {}
    TripleOpNode(OperatorType tp): ASTNode(ASTNodeType::TRIPLE_OP), subtype(tp), expr0(nullptr), expr1(nullptr), expr2(nullptr) {}
};

class BinaryOpNode: public ASTNode {
    public:
    OperatorType subtype;
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;

    BinaryOpNode(): ASTNode(ASTNodeType::BINARY_OP), subtype(OperatorType::NONE), left(nullptr), right(nullptr) {}
    BinaryOpNode(OperatorType tp): ASTNode(ASTNodeType::BINARY_OP), subtype(tp), left(nullptr), right(nullptr) {}
};

class UnaryOpNode: public ASTNode {
    public:
    OperatorType subtype;
    std::unique_ptr<ASTNode> operand;

    UnaryOpNode(): ASTNode(ASTNodeType::UNARY_OP), subtype(OperatorType::NONE), operand(nullptr) {}
    UnaryOpNode(OperatorType tp): ASTNode(ASTNodeType::UNARY_OP), subtype(tp), operand(nullptr) {}
};

// function call node
class FuncCallNode: public ASTNode {
    public:
    std::unique_ptr<ASTNode> func_expr; // function expression
    std::vector<std::unique_ptr<ASTNode>> args; // argument expressions

    FuncCallNode(): ASTNode(ASTNodeType::FUNC_CALL), func_expr(nullptr), args() {}
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

    LongStatNode(): ASTNode(ASTNodeType::NONE), varType(nullptr), varName(nullptr), varExpr(nullptr), isDefine(false), isConst(false), isVolatile(false) {}
    LongStatNode(ASTNodeType tp): ASTNode(tp), varType(nullptr), varName(nullptr), varExpr(nullptr), isDefine(false), isConst(false), isVolatile(false) {}
};

class ShortStatNode: public ASTNode {
    public:
    std::unique_ptr<ASTNode> statExpr; // for return, defer, define

    ShortStatNode(): ASTNode(ASTNodeType::NONE), statExpr(nullptr) {}
    ShortStatNode(ASTNodeType tp): ASTNode(tp), statExpr(nullptr) {}
};

// control statement node
class ScopeNode: public ASTNode {
    public:
    std::vector<std::unique_ptr<ASTNode>> body;
    ASTNode* parent;

    ScopeNode(): ASTNode(ASTNodeType::SCOPE), body(), parent(nullptr) {}

    LongStatNode* findVarByName(const std::string& name); // find variable declaration, nullptr if not found
    LongStatNode* findDefineByName(const std::string& name); // find compile-time constant declaration, nullptr if not found
};

class IfNode: public ASTNode {
    public:
    std::unique_ptr<ASTNode> cond;
    std::unique_ptr<ScopeNode> flowBody;
    std::unique_ptr<ScopeNode> elseBody;

    IfNode(): ASTNode(ASTNodeType::IF), cond(nullptr), flowBody(nullptr), elseBody(nullptr) {}
};

class WhileNode: public ASTNode {
    public:
    std::unique_ptr<ASTNode> cond;
    std::unique_ptr<ScopeNode> body;

    WhileNode(): ASTNode(ASTNodeType::WHILE), cond(nullptr), body(nullptr) {}
};

class ForNode: public ASTNode {
    public:
    std::unique_ptr<ASTNode> init;
    std::unique_ptr<ASTNode> cond;
    std::unique_ptr<ASTNode> step;
    std::unique_ptr<ScopeNode> body;

    ForNode(): ASTNode(ASTNodeType::FOR), init(nullptr), cond(nullptr), step(nullptr), body(nullptr) {}
};

class SwitchNode: public ASTNode {
    public:
    std::unique_ptr<ASTNode> cond;
    std::vector<std::unique_ptr<ASTNode>> case_exprs;
    std::vector<std::unique_ptr<ASTNode>> case_bodies;
    std::unique_ptr<ScopeNode> defaultBody;

    SwitchNode(): ASTNode(ASTNodeType::SWITCH), cond(nullptr), case_exprs(), case_bodies(), defaultBody(nullptr) {}
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

    DeclFuncNode(): ASTNode(ASTNodeType::DECL_FUNC), full_name(text), struct_name(""), func_name(""), param_types(), param_names(), return_type(nullptr), body(nullptr), isVaArg(false) {}
};

class DeclStructNode: public ASTNode {
    public:
    std::string& struct_name;
    int struct_size; // total size in bytes
    int struct_align; // align requirement in bytes
    std::vector<std::unique_ptr<TypeNode>> member_types;
    std::vector<std::string> member_names;
    std::vector<int> member_sizes;
    std::vector<int> member_offsets;

    DeclStructNode(): ASTNode(ASTNodeType::DECL_STRUCT), struct_name(text), struct_size(-1), struct_align(-1), member_types(), member_names(), member_sizes(), member_offsets() {}
};

class DeclEnumNode: public ASTNode {
    public:
    std::string& enum_name;
    int enum_size; // size in bytes
    std::vector<std::string> member_names;
    std::vector<int64_t> member_values;

    DeclEnumNode(): ASTNode(ASTNodeType::DECL_ENUM), enum_name(text), enum_size(-1), member_names(), member_values() {}
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

    ASTNode* findNodeByName(ASTNodeType tp, const std::string& name, bool checkExported); // find include, tmp, var, func, struct, enum
    std::unique_ptr<TypeNode> parseType(TokenProvider& tp); // parse type from tokens
};

// parser class
class ASTGen {
    public:
    CompileMessage prt;
    int arch; // target architecture in bytes
    std::vector<std::vector<std::string>> nameStack; // for scope name mangling
    std::vector<std::unique_ptr<SrcFile>> srcFiles;

    ASTGen(): prt(3), arch(8), nameStack(), srcFiles() {}
    ASTGen(int p, int a): prt(p), arch(a), nameStack(), srcFiles() {}

    std::string parse(const std::string& path); // returns error message or empty if ok

    private:
    std::string getLocString(const Location& loc) { return std::format("{}:{}", srcFiles[loc.source_id]->path, loc.line); }
    int findSource(const std::string& path); // find source file index, -1 if not found


};

#endif // ASTGEN_H