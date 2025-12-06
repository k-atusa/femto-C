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
    STRING_ESCAPE,
    RAW_STRING
};

enum class TokenType {
    NONE,
    // Literals
    LIT_INT_BIN, // for tokenizer only
    LIT_INT_OCT, // for tokenizer only
    LIT_INT_HEX, // for tokenizer only
    LIT_INT_CHAR, // for tokenizer only
    LIT_INT,
    LIT_FLOAT,
    LIT_STRING,
    // identifier
    IDENTIFIER,
    // + - * / %
    OP_PLUS,
    OP_MINUS,
    OP_MUL,
    OP_DIV,
    OP_REMAIN,
    // < <= > >= == !=
    OP_LT,
    OP_LT_EQ,
    OP_GT,
    OP_GT_EQ,
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
    // ? . , : ; ( ) { } [ ]
    OP_QMARK,
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
    // = += -= *= /= %=
    OP_ASSIGN,
    OP_ASSIGN_ADD,
    OP_ASSIGN_SUB,
    OP_ASSIGN_MUL,
    OP_ASSIGN_DIV,
    OP_ASSIGN_REMAIN,
    // Keywords
    KEY_AUTO,
    KEY_INT,
    KEY_I8,
    KEY_I16,
    KEY_I32,
    KEY_I64,
    KEY_UINT,
    KEY_U8,
    KEY_U16,
    KEY_U32,
    KEY_U64,
    KEY_F32,
    KEY_F64,
    KEY_BOOL,
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
    KEY_FALL,
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
    ORDER_VA_ARG,
    ORDER_RAW_C,
    ORDER_RAW_IR,
    ORDER_CONST,
    ORDER_VOLATILE,
    ORDER_EXTERN,
    ORDER_EXPORT,
    // for token match
    PRECOMPILE
};

class Token {
    public:
    TokenType objType;
    Location location;
    Literal value;
    std::string text;

    Token() : objType(TokenType::NONE), location(), value(), text("") {}

    std::string toString() {
        std::string result = "Tkn type: " + std::to_string(static_cast<int>(objType)) + ", location: " + std::to_string(location.srcLoc) + "." + std::to_string(location.line);
        result += ", value: " + value.toString() + ", text: " + text;
        return result;
    }
};

// tokenize source code into tokens, [can raise exception]
std::unique_ptr<std::vector<Token>> tokenize(const std::string& source, const std::string filename, const int source_id);

class TokenProvider {
    public:
    std::vector<Token>& tokens;
    Token nulltkn;
    int pos;

    TokenProvider(std::vector<Token>& data) : tokens(data), nulltkn(), pos(0) {
        if (tokens.size() > 0) {
            nulltkn.location.srcLoc = tokens[0].location.srcLoc;
            nulltkn.location.line = -1;
        }
    }

    bool canPop(int num);
    Token& pop();
    Token& seek();
    void rewind();
    bool match(const std::vector<TokenType>& types);
};

// token type checker
bool isSInt(TokenType type) { return type == TokenType::KEY_INT || type == TokenType::KEY_I8 || type == TokenType::KEY_I16 || type == TokenType::KEY_I32 || type == TokenType::KEY_I64; }
bool isUInt(TokenType type) { return type == TokenType::KEY_UINT || type == TokenType::KEY_U8 || type == TokenType::KEY_U16 || type == TokenType::KEY_U32 || type == TokenType::KEY_U64; }
bool isInt(TokenType type) { return isSInt(type) || isUInt(type); }
bool isFloat(TokenType type) { return type == TokenType::KEY_F32 || type == TokenType::KEY_F64; }
bool isPrimitive(TokenType type) { return isInt(type) || isFloat(type) || type == TokenType::KEY_VOID || type == TokenType::KEY_BOOL; }

#endif // TOKENIZER_H
