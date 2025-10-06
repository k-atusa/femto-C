#ifndef TOKENIZE_H
#define TOKENIZE_H

#include "baseNode.h"

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

std::vector<Token> tokenize(const std::string& source, const std::string filename, const int source_id);

#endif // TOKENIZE_H