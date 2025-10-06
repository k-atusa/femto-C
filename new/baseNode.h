#ifndef BASE_NODE_H
#define BASE_NODE_H

#include <string>
#include <vector>
#include <memory>

struct LocNode {
    int source_id;
    int line;

    LocNode() : source_id(-1), line(-1) {}
    LocNode(int src_id, int ln) : source_id(src_id), line(ln) {}
};

class SourceTable {
    public:
    std::vector<std::string> sources;

    int addSource(const std::string& source) {
        sources.push_back(source);
        return sources.size() - 1;
    }

    const std::string& getSource(int id) const {
        if (id >= 0 && id < static_cast<int>(sources.size())) {
            return sources[id];
        }
        static const std::string empty = "";
        return empty;
    }

    int findSource(const std::string& source) const {
        for (size_t i = 0; i < sources.size(); ++i) {
            if (sources[i] == source) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    std::string toString() {
        std::string result;
        for (size_t i = 0; i < sources.size(); ++i) {
            result += "SrcID " + std::to_string(i) + ": " + sources[i] + "\n";
        }
        return result;
    }
};

enum class ValueType {
    NONE,
    INT10,
    INT16,
    FLOAT,
    CHAR,
    STRING
};

struct ValueNode {
    ValueType type;
    int int_value;
    double float_value;
    char char_value;
    std::string string_value;

    ValueNode() : type(ValueType::NONE) {}
    ValueNode(int val) : type(ValueType::INT10), int_value(val) {}
    ValueNode(double val) : type(ValueType::FLOAT), float_value(val) {}
    ValueNode(char val) : type(ValueType::CHAR), char_value(val) {}
    ValueNode(const std::string& val) : type(ValueType::STRING), string_value(val) {}

    std::string toString() const {
        switch (type) {
            case ValueType::INT10:
                return std::to_string(int_value);
            case ValueType::FLOAT:
                return std::to_string(float_value);
            case ValueType::CHAR:
                return "'" + std::string(1, char_value) + "'";
            case ValueType::STRING:
                return "\"" + string_value + "\"";
            default:
                return "";
        }
    }
};

enum class TokenizeStatus {
    DEFAULT,
    SHORT_COMMENT,
    LONG_COMMENT,
    IDENTIFIER,
    DOUBLE_OP,
    NUMBER,
    CHAR,
    CHAR_ESCAPE,
    STRING,
    STRING_ESCAPE
};

enum class TokenType {
    NONE,
    // Literals and identifiers
    LIT_INT10,
    LIT_INT16,
    LIT_FLOAT,
    LIT_CHAR,
    LIT_STRING,
    IDENTIFIER,
    // + - * / %
    OP_PLUS,
    OP_MINUS,
    OP_MUL,
    OP_DIV,
    OP_REMAIN,
    // < <= > >= == !=
    OP_LITTER,
    OP_LITTER_EQ,
    OP_GREATER,
    OP_GREATER_EQ,
    OP_EQ,
    OP_NOT_EQ,
    // && || ! & | ~ ^ << >>
    OP_LOGIC_AND,
    OP_LOGIC_OR,
    OP_LOGIC_NOT,
    OP_BIT_AND,
    OP_BIT_OR,
    OP_BIT_NOT,
    OP_BIT_XOR,
    OP_BIT_LSHIFT,
    OP_BIT_RSHIFT,
    // = . , : ; # ( ) { } [ ]
    OP_ASSIGN,
    OP_DOT,
    OP_COMMA,
    OP_COLON,
    OP_SEMICOLON,
    OP_HASH,
    OP_LPAREN,
    OP_RPAREN,
    OP_LBRACE,
    OP_RBRACE,
    OP_LBRACKET,
    OP_RBRACKET,
    // Keywords
    KEY_I8,
    KEY_I16,
    KEY_I32,
    KEY_I64,
    KEY_U8,
    KEY_U16,
    KEY_U32,
    KEY_U64,
    KEY_F32,
    KEY_F64,
    KEY_VOID,
    KEY_NULL,
    KEY_TRUE,
    KEY_FALSE,
    KEY_SIZEOF,
    KEY_IF,
    KEY_ELSE,
    KEY_WHILE,
    KEY_FOR,
    KEY_SWITCH,
    KEY_CASE,
    KEY_DEFAULT,
    KEY_BREAK,
    KEY_CONTINUE,
    KEY_RETURN,
    KEY_STRUCT,
    KEY_ENUM
};

class Token {
    public:
    TokenType type;
    LocNode location;
    ValueNode value;
    std::string text;

    Token() : type(TokenType::NONE), location(), value(), text("") {}

    std::string toString() {
        std::string result;
        result += "Tkn type: " + std::to_string(static_cast<int>(type));
        result += ", location: " + std::to_string(location.source_id) + "." + std::to_string(location.line);
        result += ", value: " + value.toString();
        result += ", text: " + text;
        return result;
    }
};

enum class PNodeType {
    NONE,
    A
};

class PNode {
    public:
    PNodeType type;
    LocNode location;
    ValueNode value;
    std::unique_ptr<PNode> type_node;
    std::vector<std::unique_ptr<PNode>> children;

    PNode() : type(PNodeType::NONE), location(), value(), type_node(nullptr), children() {}

    std::string toString(int depth = 0) {
        std::string indent(depth * 2, ' ');
        std::string result;
        result += indent + "PNode type: " + std::to_string(static_cast<int>(type)) + "\n";
        result += indent + "location: " + std::to_string(location.source_id) + "." + std::to_string(location.line) + "\n";
        result += indent + "value: " + value.toString() + "\n";
        if (type_node) {
            result += indent + "type_node:\n" + type_node->toString(depth + 1);
        }
        for (const auto& child : children) {
            result += indent + "child:\n" + child->toString(depth + 1);
        }
        return result + "\n";
    }
};

#endif // BASE_NODE_H