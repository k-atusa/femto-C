#ifndef ASTGEN_H
#define ASTGEN_H

#include "baseFunc.h"

// AST node types
enum class ASTNodeType {
    NONE,
    // compiler order
    INCLUDE,
    // expression
    LITERAL,
    LITERAL_ARRAY,
    // type node
    TYPE,
    TYPE_PTR,
    TYPE_ARRAY,
    TYPE_SLICE,
    TYPE_FUNCTION
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