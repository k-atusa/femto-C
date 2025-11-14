#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "baseFunc.h"

enum class TokenizeStatus {
    DEFAULT,
    SHORT_COMMENT,
    LONG_COMMENT,
    IDENTIFIER,
    COMPILER_ORD,
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
    // = . , : ; ( ) { } [ ]
    OP_ASSIGN,
    OP_DOT,
    OP_COMMA,
    OP_COLON,
    OP_SEMICOLON,
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
    KEY_ENUM,
    // integrated functions
    IFUNC_SIZEOF,
    IFUNC_CAST,
    IFUNC_MAKE,
    IFUNC_LEN,
    // compiler order
    ORDER_INCLUDE,
    ORDER_TEMPLATE,
    ORDER_DEFER,
    ORDER_DEFINE,
    ORDER_CONST,
    ORDER_VOLATILE,
    ORDER_VA_ARG,
    ORDER_RAW_C,
    ORDER_FUNC_C,
    ORDER_RAW_IR,
    ORDER_FUNC_IR,
    // for token match
    PRECOMPILE
};

class Token {
    public:
    TokenType type;
    Location location;
    Literal value;
    std::string text;

    Token() : type(TokenType::NONE), location(), value(), text("") {}

    std::string toString() {
        std::string result = "Tkn type: " + std::to_string(static_cast<int>(type)) + ", location: " + std::to_string(location.source_id) + "." + std::to_string(location.line);
        result += ", value: " + value.toString() + ", text: " + text;
        return result;
    }
};

// tokenize source code into tokens, [can raise exception]
std::vector<Token> tokenize(const std::string& source, const std::string filename, const int source_id);

class TokenProvider {
    public:
    std::vector<Token> tokens;
    Token nulltkn;
    int pos;

    TokenProvider(std::vector<Token> data) : tokens(std::move(data)), nulltkn(), pos(0) {}

    bool canPop(int num);
    Token& pop();
    Token& seek();
    void rewind();
    bool match(const std::vector<TokenType>& types);
};

#endif // TOKENIZER_H
