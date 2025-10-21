#ifndef PARSER_H
#define PARSER_H

#include "tokenize.h"

// parse node types
enum class ParseNodeType {
    NONE,

    DECL_VAR, // variable declaration
    DECL_FUNC, // function declaration

    STAT_BLOCK, // series of statements
    STAT_VAR, // set variable
    STAT_EXPR, // function call
    STAT_IF, // if statement
    STAT_WHILE, // while statement
    STAT_FOR, // for statement
    STAT_SWITCH, // switch statement
    STAT_RETURN, // return statement
    STAT_CTRL, // compiler control, break, continue

    EXPR_LITERAL, // literal
    EXPR_VAR, // variable
    EXPR_FUNC, // function call
    EXPR_UNARY, // sizeof, cast, *, &, -, !, ~
    EXPR_BINARY // ., *, /, %, +, -, <<, >>, <, <=, >, >=, ==, !=, &, ^, |, &&, ||
};

// parent class of parsing node
class ParseNode {
    public:
    ParseNodeType type;
    LocNode location;
    std::string text;
    ValueNode value_node; // for literal
    std::unique_ptr<TypeNode> type_node; // for expression node
    std::unique_ptr<NameTable> name_node; // for block node
    ParseNode* parent;
    std::vector<std::unique_ptr<ParseNode>> children;

    ParseNode() : type(ParseNodeType::NONE), location(), text(""), type_node(nullptr), name_table(nullptr), parent(nullptr) {}
    ParseNode(ParseNodeType tp, LocNode loc, std::string txt, ParseNode* p) : type(tp), location(loc), text(txt), type_node(nullptr), name_table(nullptr), parent(p) {}
    std::string toString(int indent = 0) const {
        std::string indent_str(indent, ' ');
        std::string result = indent_str + "ParseNode type: " + std::to_string(static_cast<int>(type)) + ", text: " + text + "\n";
        for (const auto& child : children) {
            result += child->toString(indent + 1) + "\n";
        }
        return result.substr(0, result.length() - 1);
    }
};

// represents a source module
class SrcModule {
    public:
    int source_id;
    TypeTable export_types; // struct & enum exports
    NameTable export_names; // function & global exports
    TypeTable table_types; // local types
    NameTable table_names; // local names
    // add ast nodes /////

    SrcModule(int id) : source_id(id), export_types(id), export_names(id), table_types(id), table_names(id) {}
    
    std::string toString(const std::string& path);
};

// parser class, only one per program
class Parser {
    public:
    CompileMessage printer;
    int ArchSize;
    SourceTable srcTable;
    std::vector<std::unique_ptr<SrcModule>> modTables; // one per source

    Parser() : printer(), ArchSize(8), srcTable(), modTables() {}
    Parser(int asz) : printer(), ArchSize(asz), srcTable(), modTables() {}

    std::string toString();
    std::string parseSrc(const std::string& src_path); // returns error message or empty if ok

    private:
    std::string findLocation(LocNode loc); // get location string from loc node
    int findModule(int id); // find module index in modTables, -1 if not found

    std::string pass1(TokenProvider& prov, SrcModule& curSrc, const std::string& curPath); // manage import, create type table
    std::unique_ptr<TypeNode> parseGenericType(TokenProvider& prov, SrcModule& curSrc); // parse any type, [can raise exception]
    std::unique_ptr<TypeNode> parseStructDef(const std::string name, TokenProvider& prov, SrcModule& curSrc); // parse struct type, [can raise exception]
    std::unique_ptr<TypeNode> parseEnumDef(const std::string name, TokenProvider& prov, SrcModule& curSrc); // parse enum type, [can raise exception]
    
    std::string pass2(TokenProvider& prov, SrcModule& curSrc); // create name table
    std::string parseGlobalDef(TokenProvider& prov, SrcModule& curSrc, std::unique_ptr<TypeNode> front_type, int tag); // parse global variable definition
    std::string parseFunctionDef(TokenProvider& prov, SrcModule& curSrc, std::unique_ptr<TypeNode> front_type, int tag); // parse function definition

    std::string pass3(TokenProvider& prov, SrcModule& curSrc); // create ast nodes
};

#endif // PARSER_H
