#include <stdexcept>
#include <format>
#include "tokenizer.h"

bool isDoubleOpStart(char c) {
    return c == '<' || c == '>' || c == '=' || c == '!' || c == '&' || c == '|';
}

TokenType isDoubleOp(char c1, char c2) {
    if (c1 == '<' && c2 == '=') return TokenType::OP_LITTER_EQ;
    if (c1 == '>' && c2 == '=') return TokenType::OP_GREATER_EQ;
    if (c1 == '=' && c2 == '=') return TokenType::OP_EQ;
    if (c1 == '!' && c2 == '=') return TokenType::OP_NOT_EQ;
    if (c1 == '&' && c2 == '&') return TokenType::OP_LOGIC_AND;
    if (c1 == '|' && c2 == '|') return TokenType::OP_LOGIC_OR;
    if (c1 == '<' && c2 == '<') return TokenType::OP_BIT_LSHIFT;
    if (c1 == '>' && c2 == '>') return TokenType::OP_BIT_RSHIFT;
    return TokenType::NONE;
}

TokenType isSingleOp(char c) {
    switch (c) {
        case '+': return TokenType::OP_PLUS;
        case '-': return TokenType::OP_MINUS;
        case '*': return TokenType::OP_MUL;
        case '/': return TokenType::OP_DIV;
        case '%': return TokenType::OP_REMAIN;
        case '<': return TokenType::OP_LITTER;
        case '>': return TokenType::OP_GREATER;
        case '!': return TokenType::OP_LOGIC_NOT;
        case '&': return TokenType::OP_BIT_AND;
        case '|': return TokenType::OP_BIT_OR;
        case '~': return TokenType::OP_BIT_NOT;
        case '^': return TokenType::OP_BIT_XOR;
        case '=': return TokenType::OP_ASSIGN;
        case '.': return TokenType::OP_DOT;
        case ',': return TokenType::OP_COMMA;
        case ':': return TokenType::OP_COLON;
        case ';': return TokenType::OP_SEMICOLON;
        case '(': return TokenType::OP_LPAREN;
        case ')': return TokenType::OP_RPAREN;
        case '{': return TokenType::OP_LBRACE;
        case '}': return TokenType::OP_RBRACE;
        case '[': return TokenType::OP_LBRACKET;
        case ']': return TokenType::OP_RBRACKET;
        default: return TokenType::NONE;
    }
}

TokenType isKeyword(const std::string& word) {
    if (word == "i8") return TokenType::KEY_I8;
    if (word == "i16") return TokenType::KEY_I16;
    if (word == "i32") return TokenType::KEY_I32;
    if (word == "i64") return TokenType::KEY_I64;
    if (word == "u8") return TokenType::KEY_U8;
    if (word == "u16") return TokenType::KEY_U16;
    if (word == "u32") return TokenType::KEY_U32;
    if (word == "u64") return TokenType::KEY_U64;
    if (word == "f32") return TokenType::KEY_F32;
    if (word == "f64") return TokenType::KEY_F64;
    if (word == "void") return TokenType::KEY_VOID;
    if (word == "null") return TokenType::KEY_NULL;
    if (word == "true") return TokenType::KEY_TRUE;
    if (word == "false") return TokenType::KEY_FALSE;
    if (word == "if") return TokenType::KEY_IF;
    if (word == "else") return TokenType::KEY_ELSE;
    if (word == "while") return TokenType::KEY_WHILE;
    if (word == "for") return TokenType::KEY_FOR;
    if (word == "switch") return TokenType::KEY_SWITCH;
    if (word == "case") return TokenType::KEY_CASE;
    if (word == "default") return TokenType::KEY_DEFAULT;
    if (word == "break") return TokenType::KEY_BREAK;
    if (word == "continue") return TokenType::KEY_CONTINUE;
    if (word == "return") return TokenType::KEY_RETURN;
    if (word == "struct") return TokenType::KEY_STRUCT;
    if (word == "enum") return TokenType::KEY_ENUM;
    if (word == "sizeof") return TokenType::IFUNC_SIZEOF;
    if (word == "cast") return TokenType::IFUNC_CAST;
    if (word == "make") return TokenType::IFUNC_MAKE;
    if (word == "len") return TokenType::IFUNC_LEN;
    return TokenType::NONE;
}

TokenType isCplrOrd(const std::string& word) {
    if (word == "#include") return TokenType::ORDER_INCLUDE;
    if (word == "#template") return TokenType::ORDER_TEMPLATE;
    if (word == "#defer") return TokenType::ORDER_DEFER;
    if (word == "#define") return TokenType::ORDER_DEFINE;
    if (word == "#const") return TokenType::ORDER_CONST;
    if (word == "#volatile") return TokenType::ORDER_VOLATILE;
    if (word == "#va_arg") return TokenType::ORDER_VA_ARG;
    if (word == "#raw_c") return TokenType::ORDER_RAW_C;
    if (word == "#func_c") return TokenType::ORDER_FUNC_C;
    if (word == "#raw_ir") return TokenType::ORDER_RAW_IR;
    if (word == "#func_ir") return TokenType::ORDER_FUNC_IR;
    return TokenType::NONE;
}

TokenType isNumber(const std::string& text) {
    bool isHex = false;
    bool isFloat = false;
    for (size_t i = 0; i < text.size(); i++) {
        char c = text[i];
        if ((c == 'x' || c == 'X') && i == 1 && text[0] == '0') {
            isHex = true;
        } else if (c == '.' && !isHex && !isFloat) {
            isFloat = true;
        } else if (!(('0' <= c && c <= '9') || (isHex && (('a' <= c && c <= 'f') || ('A' <= c && c <= 'F'))))) {
            return TokenType::NONE;
        }
    }
    if (isFloat) return TokenType::LIT_FLOAT;
    if (isHex) return TokenType::LIT_INT16;
    return TokenType::LIT_INT10;
}

// Main function to tokenize the input source code, [can raise error]
std::vector<Token> tokenize(const std::string& source, const std::string filename, const int source_id) {
    std::vector<Token> result;
    std::vector<char> buffer;
    TokenizeStatus status = TokenizeStatus::DEFAULT;
    int line = 1;
    int readPos = 0;

    bool loopCtrl = true;
    while (loopCtrl) {
        char c;
        if (readPos >= source.size()) { // add newline at EOF
            c = '\n';
            loopCtrl = false;
        } else { // getchar
            c = source[readPos++];
        }

        switch (status) {
            case TokenizeStatus::DEFAULT:
                if (('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') || c == '_' || c > 127) { // id start
                    buffer.clear();
                    buffer.push_back(c);
                    status = TokenizeStatus::IDENTIFIER;
                } else if ('0' <= c && c <= '9') { // number start
                    buffer.clear();
                    buffer.push_back(c);
                    status = TokenizeStatus::NUMBER;
                } else if (isDoubleOpStart(c)) { // op start
                    buffer.clear();
                    buffer.push_back(c);
                    status = TokenizeStatus::DOUBLE_OP;
                } else {
                    switch (c) {
                        case ' ': case '\t': case 0: break; // skip whitespace
                        case '\r': // mac newline
                            line++;
                            if (readPos < source.size() && source[readPos] == '\n') readPos++; // windows newline
                            break;
                        case '\n': // unix newline
                            line++;
                            break;
                        case '/': // comment or divide
                            if (readPos < source.size()) {
                                if (source[readPos] == '/') { // short comment
                                    readPos++;
                                    status = TokenizeStatus::SHORT_COMMENT;
                                } else if (source[readPos] == '*') { // long comment
                                    readPos++;
                                    status = TokenizeStatus::LONG_COMMENT;
                                } else { // divide
                                    Token tkn;
                                    tkn.type = TokenType::OP_DIV;
                                    tkn.location = Location(source_id, line);
                                    tkn.text = "/";
                                    result.push_back(tkn);
                                }
                            } else { // divide
                                Token tkn;
                                tkn.type = TokenType::OP_DIV;
                                tkn.location = Location(source_id, line);
                                tkn.text = "/";
                                result.push_back(tkn);
                            }
                            break;
                        case '\'': // char start
                            buffer.clear();
                            status = TokenizeStatus::CHAR;
                            break;
                        case '\"': // string start
                            buffer.clear();
                            status = TokenizeStatus::STRING;
                            break;
                        case '#': // compiler order start
                            buffer.clear();
                            buffer.push_back(c);
                            status = TokenizeStatus::COMPILER_ORD;
                            break;
                        default: // operator or unknown
                            {
                                TokenType tkn_type = isSingleOp(c);
                                if (tkn_type != TokenType::NONE) { // known single char operator
                                    Token tkn;
                                    tkn.type = tkn_type;
                                    tkn.location = Location(source_id, line);
                                    tkn.text = std::string(1, c);
                                    result.push_back(tkn);
                                } else { // unknown char
                                    throw std::runtime_error(std::format("E0101 invalid char {} at {}:{}", c, filename, line)); // E0101
                                }
                            }
                            break;
                    }
                }
                break;

            case TokenizeStatus::SHORT_COMMENT:
                if (c == '\r') { // mac newline
                    line++;
                    if (readPos < source.size() && source[readPos] == '\n') readPos++; // windows newline
                    status = TokenizeStatus::DEFAULT;
                } else if (c == '\n') { // unix newline
                    line++;
                    status = TokenizeStatus::DEFAULT;
                }
                break;

            case TokenizeStatus::LONG_COMMENT:
                if (c == '\r') { // mac newline
                    line++;
                    if (readPos < source.size() && source[readPos] == '\n') readPos++; // windows newline
                } else if (c == '\n') { // unix newline
                    line++;
                } else if (c == '*') {
                    if (readPos < source.size() && source[readPos] == '/') { // end of long comment
                        readPos++;
                        status = TokenizeStatus::DEFAULT;
                    }
                }
                break;

            case TokenizeStatus::IDENTIFIER:
                if (('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') || c == '_' || c > 127 || ('0' <= c && c <= '9')) { // id continue
                    buffer.push_back(c);
                } else { // id end
                    std::string id_str(buffer.begin(), buffer.end());
                    Token tkn;
                    tkn.location = Location(source_id, line);
                    tkn.text = id_str;
                    TokenType kw_type = isKeyword(id_str);
                    if (kw_type != TokenType::NONE) { // keyword
                        tkn.type = kw_type;
                    } else { // identifier
                        tkn.type = TokenType::IDENTIFIER;
                        tkn.value = Literal(id_str);
                    }
                    result.push_back(tkn);
                    status = TokenizeStatus::DEFAULT;
                    readPos--; // rewind 1 char
                }
                break;

            case TokenizeStatus::DOUBLE_OP:
            {
                TokenType tkn_type = isDoubleOp(buffer[0], c);
                if (tkn_type != TokenType::NONE) { // known double char operator
                    Token tkn;
                    tkn.type = tkn_type;
                    tkn.location = Location(source_id, line);
                    tkn.text = std::string(1, buffer[0]) + std::string(1, c);
                    result.push_back(tkn);
                    status = TokenizeStatus::DEFAULT;
                } else { // single char operator + char c
                    Token tkn;
                    tkn.type = isSingleOp(buffer[0]);
                    tkn.location = Location(source_id, line);
                    tkn.text = std::string(1, buffer[0]);
                    result.push_back(tkn);
                    status = TokenizeStatus::DEFAULT;
                    readPos--; // rewind 1 char
                }
                break;
            }

            case TokenizeStatus::NUMBER:
                if (('0' <= c && c <= '9') || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F') || c == 'x' || c == 'X' || c == '.') { // number continue
                    buffer.push_back(c);
                } else { // number end
                    std::string num_str(buffer.begin(), buffer.end());
                    TokenType num_type = isNumber(num_str);
                    if (num_type == TokenType::NONE) { // invalid number
                        throw std::runtime_error(std::format("E0102 invalid number {} at {}:{}", num_str, filename, line)); // E0102
                    }
                    Token tkn;
                    tkn.type = num_type;
                    tkn.location = Location(source_id, line);
                    tkn.text = num_str;
                    if (num_type == TokenType::LIT_INT10) {
                        tkn.value = Literal(std::stoll(num_str));
                    } else if (num_type == TokenType::LIT_INT16) {
                        tkn.value = Literal(std::stoll(num_str, nullptr, 16));
                    } else if (num_type == TokenType::LIT_FLOAT) {
                        tkn.value = Literal(std::stod(num_str));
                    }
                    result.push_back(tkn);
                    status = TokenizeStatus::DEFAULT;
                    readPos--; // rewind 1 char
                }
                break;

            case TokenizeStatus::CHAR:
                if (c == '\\') { // escape start
                    status = TokenizeStatus::CHAR_ESCAPE;
                } else if (c == '\r' || c == '\n') { // newline in char error
                    throw std::runtime_error(std::format("E0103 newline in char literal at {}:{}", filename, line)); // E0103
                } else if (c == '\'') { // char end
                    if (buffer.empty()) { // empty char error
                        throw std::runtime_error(std::format("E0104 empty char literal at {}:{}", filename, line)); // E0104
                    }
                    if (buffer.size() > 1) { // multi char error
                        throw std::runtime_error(std::format("E0105 char literal too long at {}:{}", filename, line)); // E0105
                    }
                    Token tkn;
                    tkn.type = TokenType::LIT_CHAR;
                    tkn.location = Location(source_id, line);
                    tkn.text = std::string(buffer.begin(), buffer.end());
                    tkn.value = Literal(buffer[0]);
                    result.push_back(tkn);
                    status = TokenizeStatus::DEFAULT;
                } else { // normal char
                    buffer.push_back(c);
                }
                break;

            case TokenizeStatus::CHAR_ESCAPE:
                switch (c) {
                    case '0': buffer.push_back('\0'); break;
                    case 'n': buffer.push_back('\n'); break;
                    case 'r': buffer.push_back('\r'); break;
                    case 't': buffer.push_back('\t'); break;
                    case '\\': buffer.push_back('\\'); break;
                    case '\'': buffer.push_back('\''); break;
                    case '\"': buffer.push_back('\"'); break;
                    default: // unknown escape
                        throw std::runtime_error(std::format("E0106 invalid char escape \\{} at {}:{}", c, filename, line)); // E0106
                }
                status = TokenizeStatus::CHAR;
                break;

            case TokenizeStatus::STRING:
                if (c == '\\') { // escape start
                    status = TokenizeStatus::STRING_ESCAPE;
                } else if (c == '\r' || c == '\n') { // newline in string error
                    throw std::runtime_error(std::format("E0107 newline in string literal at {}:{}", filename, line)); // E0107
                } else if (c == '\"') { // string end
                    Token tkn;
                    tkn.type = TokenType::LIT_STRING;
                    tkn.location = Location(source_id, line);
                    tkn.text = std::string(buffer.begin(), buffer.end());
                    tkn.value = Literal(tkn.text);
                    result.push_back(tkn);
                    status = TokenizeStatus::DEFAULT;
                } else { // normal char
                    buffer.push_back(c);
                }
                break;

            case TokenizeStatus::STRING_ESCAPE:
                switch (c) {
                    case '0': buffer.push_back('\0'); break;
                    case 'n': buffer.push_back('\n'); break;
                    case 'r': buffer.push_back('\r'); break;
                    case 't': buffer.push_back('\t'); break;
                    case '\\': buffer.push_back('\\'); break;
                    case '\'': buffer.push_back('\''); break;
                    case '\"': buffer.push_back('\"'); break;
                    default: // unknown escape
                        throw std::runtime_error(std::format("E0108 invalid string escape \\{} at {}:{}", c, filename, line)); // E0108
                }
                status = TokenizeStatus::STRING;
                break;

            case TokenizeStatus::COMPILER_ORD:
                if (('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') || c == '_' || c > 127 || ('0' <= c && c <= '9')) { // order continue
                    buffer.push_back(c);
                } else { // order end
                    std::string order_str(buffer.begin(), buffer.end());
                    Token tkn;
                    tkn.location = Location(source_id, line);
                    tkn.text = order_str;
                    tkn.type = isCplrOrd(order_str);
                    if (tkn.type == TokenType::NONE) { // invalid compiler order
                        throw std::runtime_error(std::format("E0109 unsupported compiler order {} at {}:{}", order_str, filename, line)); // E0109
                    }
                    result.push_back(tkn);
                    status = TokenizeStatus::DEFAULT;
                    readPos--; // rewind 1 char
                }
                break;
        }
    }
    return result;
}

// token provider functions
bool TokenProvider::canPop(int num) {
    return pos + num - 1 < tokens.size();
}

Token& TokenProvider::pop() {
    if (pos >= tokens.size()) {
        return nulltkn;
    }
    return tokens[pos++];
}

Token& TokenProvider::seek() {
    if (pos >= tokens.size()) {
        return nulltkn;
    }
    return tokens[pos];
}

void TokenProvider::rewind() {
    if (pos > 0) {
        pos--;
    }
}

bool TokenProvider::match(const std::vector<TokenType>& types) {
    if (!canPop(types.size())) {
        return false;
    }
    for (int i = 0; i < types.size(); i++) {
        if (types[i] == TokenType::PRECOMPILE) {
            continue;
        }
        if (tokens[pos + i].type != types[i]) {
            return false;
        }
    }
    return true;
}