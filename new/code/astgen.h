#ifndef ASTGEN_H
#define ASTGEN_H

#include "baseFunc.h"

// AST node types
enum class ASTNodeType {
    NONE,
    // compiler order
    INCLUDE,
    TEMPLATE_VAR,
    // expression
    LITERAL,
    LITERAL_ARRAY,
    BINARY_OP,
    UNARY_OP,
    // type node
    TYPE,
    // normal statement
    DECLARE_VAR,
    ASSIGN,
    RETURN,
    BREAK,
    CONTINUE,
    // control statement
    IF,
    WHILE,
    FOR,
    SWITCH,
    SCOPE,
};

// parent of AST nodes
class ASTNode {
    public:
    ASTNodeType type;
    Location location;
    std::string text;

    ASTNode(): type(ASTNodeType::NONE), location(), text("") {}
    ASTNode(ASTNodeType t): type(t), location(), text() {}

    std::string toString() const { return std::to_string((int)type) + " " + text; }
};

// include node
class IncludeNode: public ASTNode {
    public:
    std::string path; // normal or template source file path
    std::string name; // import name
    std::string unique_name; // compile target name
    std::vector<std::unique_ptr<ASTNode>> type_args; // template type arguments

    IncludeNode(): ASTNode(ASTNodeType::INCLUDE), path(""), name(""), unique_name("") {}
};

// template variable node
class TemplateVarNode: public ASTNode {
    public:
    std::vector<std::string> arg_names; // template type arguments names

    TemplateVarNode(): ASTNode(ASTNodeType::TEMPLATE_VAR), arg_names() {}
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

// operator node, ordered by priority
enum class OperatorType {
    NONE,
    B_DOT,
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

// type node
enum class TypeNodeType {
    NONE,
    PRIMITIVE,
    POINTER,
    ARRAY,
    SLICE,
    FUNCTION,
    NAME // for struct, enum, template
};

class TypeNode: public ASTNode {
    public:
    TypeNodeType subtype;
    std::string name; // type name
    std::unique_ptr<TypeNode> direct; // ptr, arr, slice target & func return
    std::vector<std::unique_ptr<TypeNode>> indirect; // func args
    int64_t length; // array length

    TypeNode(): ASTNode(ASTNodeType::TYPE), name(""), direct(nullptr), indirect(), length(-1) {}
    TypeNode(TypeNodeType tp, const std::string& nm): ASTNode(ASTNodeType::TYPE), subtype(tp), name(nm), direct(nullptr), indirect(), length(-1) {}
};

// statement node
class StatementNode: public ASTNode {
    public:
    std::unique_ptr<TypeNode> varType; // for var_declare
    std::unique_ptr<ASTNode> varExpr; // for var_declare init, var_assign, return
    std::string varName; // for var_declare, var_assign
    bool isDefer;
    bool isDefine;
    bool isConst;
    bool isVolatile;

    StatementNode(): ASTNode(ASTNodeType::NONE), varType(nullptr), varExpr(nullptr), varName(""), isDefer(false), isDefine(false), isConst(false), isVolatile(false) {}
    StatementNode(ASTNodeType tp): ASTNode(tp), varType(nullptr), varExpr(nullptr), varName(""), isDefer(false), isDefine(false), isConst(false), isVolatile(false) {}
};

// abstract source file
class SrcFile {
    public:
    std::string path;
    std::vector<ASTNode> nodes;
    bool isFinished;

    SrcFile(): path(""), nodes(), isFinished(false) {}
    SrcFile(std::string filepath): path(filepath), nodes(), isFinished(false) {}
};

#endif // ASTGEN_H