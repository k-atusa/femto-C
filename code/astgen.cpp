#include "astgen.h"

// get pratt operator precedence, -1 if not an operator
int getPrattPrecedence(TokenType tknType, bool isUnary) {
    if (isUnary) {
        switch (tknType) {
            case TokenType::OP_PLUS: case TokenType::OP_MINUS:
            case TokenType::OP_LOGIC_NOT: case TokenType::OP_BIT_NOT:
            case TokenType::OP_MUL: case TokenType::OP_BIT_AND:
                return 15;
            default:
                return -1;
        }
    } else {
        switch (tknType) {
            case TokenType::OP_DOT: case TokenType::OP_LPAREN: case TokenType::OP_LBRACKET:
                return 20;
            case TokenType::OP_MUL: case TokenType::OP_DIV: case TokenType::OP_REMAIN:
                return 10;
            case TokenType::OP_PLUS: case TokenType::OP_MINUS:
                return 9;
            case TokenType::OP_BIT_LSHIFT: case TokenType::OP_BIT_RSHIFT:
                return 8;
            case TokenType::OP_LITTER: case TokenType::OP_LITTER_EQ: case TokenType::OP_GREATER: case TokenType::OP_GREATER_EQ:
                return 7;
            case TokenType::OP_EQ: case TokenType::OP_NOT_EQ:
                return 6;
            case TokenType::OP_BIT_AND:
                return 5;
            case TokenType::OP_BIT_XOR:
                return 4;
            case TokenType::OP_BIT_OR:
                return 3;
            case TokenType::OP_LOGIC_AND:
                return 2;
            case TokenType::OP_LOGIC_OR:
                return 1;
            default:
                return -1;
        }
    }
}

// get pratt operator type
OperatorType getBinaryOpType(TokenType tknType) {
    switch (tknType) {
        case TokenType::OP_MUL:
            return OperatorType::B_MUL;
        case TokenType::OP_DIV:
            return OperatorType::B_DIV;
        case TokenType::OP_REMAIN:
            return OperatorType::B_MOD;
        case TokenType::OP_PLUS:
            return OperatorType::B_ADD;
        case TokenType::OP_MINUS:
            return OperatorType::B_SUB;
        case TokenType::OP_BIT_LSHIFT:
            return OperatorType::B_SHL;
        case TokenType::OP_BIT_RSHIFT:
            return OperatorType::B_SHR;
        case TokenType::OP_LITTER:
            return OperatorType::B_LT;
        case TokenType::OP_GREATER:
            return OperatorType::B_GT;
        case TokenType::OP_LITTER_EQ:
            return OperatorType::B_LE;
        case TokenType::OP_GREATER_EQ:
            return OperatorType::B_GE;
        case TokenType::OP_EQ:
            return OperatorType::B_EQ;
        case TokenType::OP_NOT_EQ:
            return OperatorType::B_NE;
        case TokenType::OP_BIT_AND:
            return OperatorType::B_BIT_AND;
        case TokenType::OP_BIT_XOR:
            return OperatorType::B_BIT_XOR;
        case TokenType::OP_BIT_OR:
            return OperatorType::B_BIT_OR;
        case TokenType::OP_LOGIC_AND:
            return OperatorType::B_LOGIC_AND;
        case TokenType::OP_LOGIC_OR:
            return OperatorType::B_LOGIC_OR;
        default:
            return OperatorType::NONE;
    }
}

// ASTNode functions
LongStatNode* ScopeNode::findVarByName(const std::string& name) {
    for (auto& node : body) {
        if (node->type == ASTNodeType::DECL_VAR) {
            LongStatNode* varNode = static_cast<LongStatNode*>(node.get());
            if (varNode->varName && varNode->varName->text == name) {
                return varNode;
            }
        }
    }
    if (parent != nullptr && parent->type == ASTNodeType::SCOPE) {
        ScopeNode* parentScope = static_cast<ScopeNode*>(parent);
        return parentScope->findVarByName(name);
    }
    return nullptr;
}

Literal ScopeNode::findDefinedLiteral(const std::string& name) {
    LongStatNode* varNode = findVarByName(name);
    if (varNode == nullptr || varNode->isDefine == false) {
        return Literal();
    }
    if (varNode->varExpr->type != ASTNodeType::LITERAL) {
        return Literal();
    }
    LiteralNode* litNode = static_cast<LiteralNode*>(varNode->varExpr.get());
    return litNode->literal;
}

// SrcFile functions
ASTNode* SrcFile::findNodeByName(ASTNodeType tp, const std::string& name, bool checkExported) {
    ASTNode* result = nullptr;
    for (auto& node : nodes->body) {
        if (node->type == tp && node->text == name) {
            result = node.get();
        }
    }
    if (result == nullptr || !checkExported) {
        return result;
    }
    switch (tp) {
        case ASTNodeType::INCLUDE: case ASTNodeType::DECL_TEMPLATE:
            return nullptr; // include and template are not exported
        case ASTNodeType::DECL_VAR: case ASTNodeType::DECL_STRUCT: case ASTNodeType::DECL_ENUM:
            if ('A' <= result->text[0] && result->text[0] <= 'Z') {
                return result;
            } else {
                return nullptr;
            }
        case ASTNodeType::DECL_FUNC:
            {
                DeclFuncNode* funcNode = static_cast<DeclFuncNode*>(result);
                if (funcNode->struct_name.empty()) { // global function
                    if ('A' <= result->text[0] && result->text[0] <= 'Z') {
                        return result;
                    } else {
                        return nullptr;
                    }
                } else { // method
                    if ('A' <= funcNode->struct_name[0] && funcNode->struct_name[0] <= 'Z' && 'A' <= funcNode->func_name[0] && funcNode->func_name[0] <= 'Z') {
                        return result;
                    } else {
                        return nullptr;
                    }
                }
            }
    }
    return result;
}

std::unique_ptr<TypeNode> SrcFile::parseType(TokenProvider& tp, ScopeNode& current, int arch) {
    // parse base type
    std::unique_ptr<TypeNode> result = std::make_unique<TypeNode>();
    if (tp.match({TokenType::IDENTIFIER, TokenType::OP_DOT, TokenType::IDENTIFIER})) { // foreign type
        Token& includeTkn = tp.pop();
        tp.pop();
        Token& nameTkn = tp.pop();
        if (findNodeByName(ASTNodeType::INCLUDE, includeTkn.text, false) == nullptr) {
            throw std::runtime_error(std::format("E02xx name {} not found while parsing type at {}:{}", includeTkn.text, path, includeTkn.location.line)); // E02xx
        }
        result->subtype = TypeNodeType::FOREIGN;
        result->include_tgt = includeTkn.text;
        result->name = nameTkn.text;

    } else if (tp.match({TokenType::IDENTIFIER})) { // tmp, struct, enum
        Token& nameTkn = tp.pop();
        result->subtype = TypeNodeType::NAME;
        result->name = nameTkn.text;

    } else if (tp.canPop(1)) { // primitive
        Token& baseTkn = tp.pop();
        result->subtype = TypeNodeType::PRIMITIVE;
        result->name = baseTkn.text;
        switch (baseTkn.type) {
            case TokenType::KEY_I8: case TokenType::KEY_U8:
                result->type_size = 1;
                result->type_align = 1;
                break;
            case TokenType::KEY_I16: case TokenType::KEY_U16:
                result->type_size = 2;
                result->type_align = 2;
                break;
            case TokenType::KEY_I32: case TokenType::KEY_U32: case TokenType::KEY_F32:
                result->type_size = 4;
                result->type_align = 4;
                break;
            case TokenType::KEY_I64: case TokenType::KEY_U64: case TokenType::KEY_F64:
                result->type_size = 8;
                result->type_align = 8;
                break;
            case TokenType::KEY_VOID:
                result->type_size = 0;
                result->type_align = 1;
                break;
            default:
                throw std::runtime_error(std::format("E02xx invalid primitive type {} at {}:{}", baseTkn.text, path, baseTkn.location.line)); // E02xx
        }

    } else {
        throw std::runtime_error("E02xx TokenProvider out of range"); // E02xx
    }

    // parse type modifiers
    while (tp.canPop(1)) {
        Token& tkn = tp.pop();
        switch (tkn.type) {
            case TokenType::OP_MUL: // pointer
                {
                    std::unique_ptr<TypeNode> ptrType = std::make_unique<TypeNode>();
                    ptrType->subtype = TypeNodeType::POINTER;
                    ptrType->name = "*";
                    ptrType->type_size = arch;
                    ptrType->type_align = arch;
                    ptrType->direct = std::move(result);
                    result = std::move(ptrType);
                }
                break;

            case TokenType::OP_LBRACKET:
                if (result->type_size == 0) {
                    throw std::runtime_error(std::format("E02xx cannot create array/slice of void type at {}:{}", path, tkn.location.line)); // E02xx
                }
                if (tp.match({TokenType::OP_RBRACKET})) { // slice
                    tp.pop();
                    std::unique_ptr<TypeNode> sliceType = std::make_unique<TypeNode>();
                    sliceType->subtype = TypeNodeType::SLICE;
                    sliceType->name = "[]";
                    sliceType->type_size = arch * 2; // ptr + length
                    sliceType->type_align = arch;
                    sliceType->direct = std::move(result);
                    result = std::move(sliceType);
                } else if (tp.match({TokenType::LIT_INT, TokenType::OP_RBRACKET})) { // array
                    Token& lenTkn = tp.pop();
                    int64_t len = lenTkn.value.int_value;
                    if (len <= 0) {
                        throw std::runtime_error(std::format("E02xx invalid array length {} at {}:{}", len, path, lenTkn.location.line)); // E02xx
                    }
                    tp.pop();
                    std::unique_ptr<TypeNode> arrType = std::make_unique<TypeNode>();
                    arrType->subtype = TypeNodeType::ARRAY;
                    arrType->name = std::format("[{}]", len);
                    arrType->length = len;
                    if (result->type_size > 0) {
                        arrType->type_size = result->type_size * len;
                        arrType->type_align = result->type_align;
                    }
                    arrType->direct = std::move(result);
                    result = std::move(arrType);
                } else if (tp.match({TokenType::IDENTIFIER, TokenType::OP_RBRACKET})) { // array with defined size
                    Token& lenTkn = tp.pop();
                    Literal lenLit = current.findDefinedLiteral(lenTkn.text);
                    if (lenLit.node_type == LiteralType::NONE) {
                        throw std::runtime_error(std::format("E02xx undefined compile time literal {} at {}:{}", lenTkn.text, path, lenTkn.location.line)); // E02xx
                    }
                    if (lenLit.node_type != LiteralType::INT || lenLit.int_value <= 0) {
                        throw std::runtime_error(std::format("E02xx invalid array length literal {} at {}:{}", lenTkn.text, path, lenTkn.location.line)); // E02xx
                    }
                    int64_t len = lenLit.int_value;
                    tp.pop();
                    std::unique_ptr<TypeNode> arrType = std::make_unique<TypeNode>();
                    arrType->subtype = TypeNodeType::ARRAY;
                    arrType->name = std::format("[{}]", len);
                    arrType->length = len;
                    if (result->type_size > 0) {
                        arrType->type_size = result->type_size * len;
                        arrType->type_align = result->type_align;
                    }
                    arrType->direct = std::move(result);
                    result = std::move(arrType);
                } else {
                    throw std::runtime_error(std::format("E02xx invalid type modifier at {}:{}", path, tkn.location.line)); // E02xx
                }
                break;

            case TokenType::OP_LPAREN: // function
                {
                    std::unique_ptr<TypeNode> funcType = std::make_unique<TypeNode>();
                    funcType->subtype = TypeNodeType::FUNCTION;
                    funcType->name = "(...)";
                    funcType->type_size = arch;
                    funcType->type_align = arch;
                    funcType->direct = std::move(result);
                    result = std::move(funcType);
                    if (tp.seek().type != TokenType::OP_RPAREN) {
                        while (tp.canPop(1)) {
                            std::unique_ptr<TypeNode> argType = parseType(tp, current, arch);
                            result->indirect.push_back(std::move(argType));
                            if (tp.seek().type == TokenType::OP_COMMA) {
                                tp.pop();
                            } else if (tp.seek().type == TokenType::OP_RPAREN) {
                                break;
                            } else {
                                throw std::runtime_error(std::format("E02xx expected ',' at {}:{}", path, tkn.location.line)); // E02xx
                            }
                        }
                    }
                    if (tp.pop().type != TokenType::OP_RPAREN) {
                        throw std::runtime_error(std::format("E02xx expected ')' at {}:{}", path, tkn.location.line)); // E02xx
                    }
                }
                break;

            default:
                tp.rewind();
                return result;
        }
    }
    return result;
}

// ASTGen basic helper functions
int ASTGen::findSource(const std::string& path) {
    for (size_t i = 0; i < srcFiles.size(); i++) {
        if (srcFiles[i]->path == path) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool ASTGen::isTypeStart(TokenProvider& tp, ScopeNode& current, SrcFile& src) {
    if (tp.match({TokenType::KEY_I8}) || tp.match({TokenType::KEY_I16}) || tp.match({TokenType::KEY_I32}) || tp.match({TokenType::KEY_I64}) ||
        tp.match({TokenType::KEY_U8}) || tp.match({TokenType::KEY_U16}) || tp.match({TokenType::KEY_U32}) || tp.match({TokenType::KEY_U64}) ||
        tp.match({TokenType::KEY_F32}) || tp.match({TokenType::KEY_F64}) || tp.match({TokenType::KEY_VOID})) { // primitive type
        return true;
    }
    if (tp.match({TokenType::IDENTIFIER, TokenType::OP_DOT, TokenType::IDENTIFIER})) { // foreign type
        Token& includeTkn = tp.pop();
        tp.pop();
        Token& nameTkn = tp.pop();
        tp.rewind();
        tp.rewind();
        tp.rewind();
        IncludeNode* includeNode = static_cast<IncludeNode*>(src.findNodeByName(ASTNodeType::INCLUDE, includeTkn.text, false));
        if (includeNode == nullptr) {
            return false;
        }
        int index = findSource(includeNode->path);
        if (index == -1) {
            throw std::runtime_error(std::format("E03xx included source file {} not found at {}", includeNode->path, getLocString(includeTkn.location))); // E03xx
        }
        if (srcFiles[index]->findNodeByName(ASTNodeType::DECL_STRUCT, nameTkn.text, true) != nullptr
                || srcFiles[index]->findNodeByName(ASTNodeType::DECL_ENUM, nameTkn.text, true) != nullptr) {
            return true;
        }
    } else if (tp.match({TokenType::IDENTIFIER})) { // tmp, struct, enum
        Token& nameTkn = tp.pop();
        tp.rewind();
        if (src.findNodeByName(ASTNodeType::DECL_TEMPLATE, nameTkn.text, false) != nullptr
                || src.findNodeByName(ASTNodeType::DECL_STRUCT, nameTkn.text, false) != nullptr
                || src.findNodeByName(ASTNodeType::DECL_ENUM, nameTkn.text, false) != nullptr) {
            return true;
        }
    }
    return false;
}

// parse Struct declaration, after struct keyword
std::unique_ptr<DeclStructNode> ASTGen::parseStruct(TokenProvider& tp, ScopeNode& current, SrcFile& src, int64_t tag) {
    std::unique_ptr<DeclStructNode> structNode = std::make_unique<DeclStructNode>();
    Token& idTkn = tp.pop();
    if (idTkn.type != TokenType::IDENTIFIER) {
        throw std::runtime_error(std::format("E03xx expected struct name at {}", getLocString(idTkn.location))); // E03xx
    }
    structNode->struct_name = idTkn.text;
    structNode->location = idTkn.location;
    if (tp.pop().type != TokenType::OP_LBRACE) {
        throw std::runtime_error(std::format("E03xx expected '{{' at {}", getLocString(idTkn.location))); // E03xx
    }
    while (tp.canPop(1)) {
        std::unique_ptr<TypeNode> fieldType = src.parseType(tp, current, arch);
        if (fieldType == nullptr) {
            throw std::runtime_error(std::format("E03xx expected field type at {}", getLocString(tp.seek().location))); // E03xx
        } else if (fieldType->type_size == 0) {
            throw std::runtime_error(std::format("E03xx field type cannot be void at {}", getLocString(fieldType->location))); // E03xx
        }
        Token& fieldIdTkn = tp.pop();
        if (fieldIdTkn.type != TokenType::IDENTIFIER) {
            throw std::runtime_error(std::format("E03xx expected field name at {}", getLocString(fieldIdTkn.location))); // E03xx
        }
        structNode->member_types.push_back(std::move(fieldType));
        structNode->member_names.push_back(fieldIdTkn.text);
        structNode->member_offsets.push_back(-1);
        Token& sepTkn = tp.seek();
        if (sepTkn.type == TokenType::OP_RBRACE) {
            break;
        } else if (sepTkn.type == TokenType::OP_COMMA || sepTkn.type == TokenType::OP_SEMICOLON) {
            tp.pop();
            if (tp.seek().type == TokenType::OP_RBRACE) {
                break;
            }
        } else {
            throw std::runtime_error(std::format("E03xx expected ',' at {}", getLocString(sepTkn.location))); // E03xx
        }
    }
    if (tp.pop().type != TokenType::OP_RBRACE) {
        throw std::runtime_error(std::format("E03xx expected '}}' at {}", getLocString(tp.seek().location))); // E03xx
    }
    structNode->isExported = ((tag & 0x010000) != 0);
    return structNode;
}

// parse Enum declaration, after enum keyword
std::unique_ptr<DeclEnumNode> ASTGen::parseEnum(TokenProvider& tp, ScopeNode& current, SrcFile& src, int64_t tag) {
    std::unique_ptr<DeclEnumNode> enumNode = std::make_unique<DeclEnumNode>();
    Token& idTkn = tp.pop();
    if (idTkn.type != TokenType::IDENTIFIER) {
        throw std::runtime_error(std::format("E03xx expected enum name at {}", getLocString(idTkn.location))); // E03xx
    }
    enumNode->enum_name = idTkn.text;
    enumNode->location = idTkn.location;
    if (tp.pop().type != TokenType::OP_LBRACE) {
        throw std::runtime_error(std::format("E03xx expected '{{' at {}", getLocString(idTkn.location))); // E03xx
    }
    int64_t prevValue = -1;
    int64_t maxValue = 0;
    int64_t minValue = 0;
    while (tp.canPop(1)) {
        Token& nameTkn = tp.pop();
        if (nameTkn.type != TokenType::IDENTIFIER) {
            throw std::runtime_error(std::format("E03xx expected enumerator name at {}", getLocString(nameTkn.location))); // E03xx
        }
        enumNode->member_names.push_back(nameTkn.text);
        if (tp.seek().type == TokenType::OP_EQ) { // init with value
            tp.pop();
            int64_t negMult = 1;
            if (tp.seek().type == TokenType::OP_MINUS) { // negative value
                negMult = -1;
                tp.pop();
            } else if (tp.seek().type == TokenType::OP_PLUS) {
                tp.pop();
            }
            if (tp.seek().type == TokenType::LIT_INT || tp.seek().type == TokenType::LIT_CHAR) { // literal value
                Token& valueTkn = tp.pop();
                prevValue = negMult * valueTkn.value.int_value - 1;
            } else if (tp.seek().type == TokenType::IDENTIFIER) { // defined literal
                Token& valueTkn = tp.pop();
                prevValue = negMult * current.findDefinedLiteral(valueTkn.text).int_value - 1;
            } else {
                throw std::runtime_error(std::format("E03xx expected enumerator value at {}", getLocString(nameTkn.location))); // E03xx
            }
        }
        prevValue++;
        for (size_t i = 0; i < enumNode->member_values.size(); i++) {
            if (enumNode->member_values[i] == prevValue) {
                throw std::runtime_error(std::format("E03xx duplicate enumerator value {} at {}", prevValue, getLocString(nameTkn.location))); // E03xx
            }
        }
        enumNode->member_values.push_back(prevValue);
        maxValue = std::max(maxValue, prevValue);
        minValue = std::min(minValue, prevValue);
        Token& sepTkn = tp.seek();
        if (sepTkn.type == TokenType::OP_RBRACE) {
            break;
        } else if (sepTkn.type == TokenType::OP_COMMA || sepTkn.type == TokenType::OP_SEMICOLON) {
            tp.pop();
            if (tp.seek().type == TokenType::OP_RBRACE) {
                break;
            }
        } else {
            throw std::runtime_error(std::format("E03xx expected ',' at {}", getLocString(sepTkn.location))); // E03xx
        }
    }
    if (tp.pop().type != TokenType::OP_RBRACE) {
        throw std::runtime_error(std::format("E03xx expected '}}' at {}", getLocString(tp.seek().location))); // E03xx
    }
    enumNode->isExported = ((tag & 0x010000) != 0);
    return enumNode;
}

// parse atomic expression
std::unique_ptr<ASTNode> ASTGen::parseAtomicExpr(TokenProvider& tp, ScopeNode& current, SrcFile& src) {
    Token& tkn = tp.pop();
    std::unique_ptr<ASTNode> result = std::make_unique<ASTNode>();
    switch (tkn.type) {
        case TokenType::LIT_INT: case TokenType::LIT_FLOAT: case TokenType::LIT_CHAR: case TokenType::LIT_STRING: // literal
            {
                std::unique_ptr<LiteralNode> litNode = std::make_unique<LiteralNode>();
                litNode->literal = tkn.value;
                litNode->location = tkn.location;
                result = std::move(litNode);
            }
            break;

        case TokenType::KEY_NULL: // null literal
            {
                std::unique_ptr<LiteralNode> litNode = std::make_unique<LiteralNode>();
                litNode->literal = Literal((int64_t)0);
                litNode->location = tkn.location;
                result = std::move(litNode);
            }
            break;

        case TokenType::KEY_TRUE: // true literal
            {
                std::unique_ptr<LiteralNode> litNode = std::make_unique<LiteralNode>();
                litNode->literal = Literal((int64_t)1);
                litNode->location = tkn.location;
                result = std::move(litNode);
            }
            break;

        case TokenType::KEY_FALSE: // false literal
            {
                std::unique_ptr<LiteralNode> litNode = std::make_unique<LiteralNode>();
                litNode->literal = Literal((int64_t)0);
                litNode->location = tkn.location;
                result = std::move(litNode);
            }
            break;

        case TokenType::IDENTIFIER: // variable name
            {
                if (current.findVarByName(tkn.text) == nullptr) {
                    throw std::runtime_error(std::format("E03xx undefined variable {} at {}", tkn.text, getLocString(tkn.location))); // E03xx
                }
                std::unique_ptr<ASTNode> nameNode = std::make_unique<ASTNode>(ASTNodeType::NAME, tkn.text);
                nameNode->location = tkn.location;
                result = std::move(nameNode);
            }
            break;

        case TokenType::OP_LPAREN: // parenthesis expression
            {
                result = parsePrattExpr(tp, current, src, 0);
                if (tp.pop().type != TokenType::OP_RPAREN) {
                    throw std::runtime_error(std::format("E03xx expected ')' at {}", getLocString(tkn.location))); // E03xx
                }
            }
            break;

        case TokenType::OP_LBRACE: // literal array
            {
                std::unique_ptr<LiteralArrayNode> arrNode = std::make_unique<LiteralArrayNode>();
                while (tp.canPop(1)) {
                    std::unique_ptr<ASTNode> element = parsePrattExpr(tp, current, src, 0);
                    arrNode->elements.push_back(std::move(element));
                    if (tp.seek().type == TokenType::OP_COMMA) {
                        tp.pop();
                    } else if (tp.seek().type == TokenType::OP_RBRACE) {
                        break;
                    } else {
                        throw std::runtime_error(std::format("E03xx expected ',' at {}", getLocString(tkn.location))); // E03xx
                    }
                }
                if (tp.pop().type != TokenType::OP_RBRACE) {
                    throw std::runtime_error(std::format("E03xx expected '}}' at {}", getLocString(tkn.location))); // E03xx
                }
                result = std::move(arrNode);
            }
            break;

        case TokenType::OP_PLUS: case TokenType::OP_MINUS: case TokenType::OP_LOGIC_NOT: case TokenType::OP_BIT_NOT: case TokenType::OP_MUL: case TokenType::OP_BIT_AND: // unary operators
            {
                std::unique_ptr<UnaryOpNode> unaryNode = std::make_unique<UnaryOpNode>();
                if (tkn.type == TokenType::OP_PLUS) {
                    unaryNode->subtype = OperatorType::U_PLUS;
                } else if (tkn.type == TokenType::OP_MINUS) {
                    unaryNode->subtype = OperatorType::U_MINUS;
                } else if (tkn.type == TokenType::OP_LOGIC_NOT) {
                    unaryNode->subtype = OperatorType::U_LOGIC_NOT;
                } else if (tkn.type == TokenType::OP_BIT_NOT) {
                    unaryNode->subtype = OperatorType::U_BIT_NOT;
                } else if (tkn.type == TokenType::OP_MUL) {
                    unaryNode->subtype = OperatorType::U_DEREF;
                } else if (tkn.type == TokenType::OP_BIT_AND) {
                    unaryNode->subtype = OperatorType::U_REF;
                }
                unaryNode->location = tkn.location;
                unaryNode->operand = parsePrattExpr(tp, current, src, getPrattPrecedence(TokenType::OP_PLUS, true));
                result = std::move(unaryNode);
            }
            break;

        case TokenType::IFUNC_MAKE:
            {
                if (tp.pop().type != TokenType::OP_LPAREN) {
                    throw std::runtime_error(std::format("E03xx expected '(' at {}", getLocString(tkn.location))); // E03xx
                }
                std::unique_ptr<BinaryOpNode> makeNode = std::make_unique<BinaryOpNode>(OperatorType::B_MAKE);
                makeNode->location = tkn.location;
                makeNode->left = parsePrattExpr(tp, current, src, 0);
                if (tp.pop().type != TokenType::OP_COMMA) {
                    throw std::runtime_error(std::format("E03xx expected ',' at {}", getLocString(tkn.location))); // E03xx
                }
                makeNode->right = parsePrattExpr(tp, current, src, 0);
                if (tp.pop().type != TokenType::OP_RPAREN) {
                    throw std::runtime_error(std::format("E03xx expected ')' at {}", getLocString(tkn.location))); // E03xx
                }
                result = std::move(makeNode);
            }
            break;

        case TokenType::IFUNC_LEN:
            {
                if (tp.pop().type != TokenType::OP_LPAREN) {
                    throw std::runtime_error(std::format("E03xx expected '(' at {}", getLocString(tkn.location))); // E03xx
                }
                std::unique_ptr<UnaryOpNode> lenNode = std::make_unique<UnaryOpNode>(OperatorType::U_LEN);
                lenNode->location = tkn.location;
                lenNode->operand = parsePrattExpr(tp, current, src, 0);
                if (tp.pop().type != TokenType::OP_RPAREN) {
                    throw std::runtime_error(std::format("E03xx expected ')' at {}", getLocString(tkn.location))); // E03xx
                }
                result = std::move(lenNode);
            }
            break;

        case TokenType::IFUNC_CAST:
            {
                if (tp.pop().type != TokenType::OP_LITTER) {
                    throw std::runtime_error(std::format("E03xx expected '<' at {}", getLocString(tkn.location))); // E03xx
                }
                std::unique_ptr<BinaryOpNode> castNode = std::make_unique<BinaryOpNode>(OperatorType::B_CAST);
                castNode->location = tkn.location;
                castNode->left = src.parseType(tp, current, arch);
                if (tp.pop().type != TokenType::OP_GREATER) {
                    throw std::runtime_error(std::format("E03xx expected '>' at {}", getLocString(tkn.location))); // E03xx
                }
                if (tp.pop().type != TokenType::OP_LPAREN) {
                    throw std::runtime_error(std::format("E03xx expected '(' at {}", getLocString(tkn.location))); // E03xx
                }
                castNode->right = parsePrattExpr(tp, current, src, 0);
                if (tp.pop().type != TokenType::OP_RPAREN) {
                    throw std::runtime_error(std::format("E03xx expected ')' at {}", getLocString(tkn.location))); // E03xx
                }
                result = std::move(castNode);
            }
            break;

        case TokenType::IFUNC_SIZEOF:
            {
                if (tp.pop().type != TokenType::OP_LPAREN) {
                    throw std::runtime_error(std::format("E03xx expected '(' at {}", getLocString(tkn.location))); // E03xx
                }
                std::unique_ptr<UnaryOpNode> sizeofNode = std::make_unique<UnaryOpNode>(OperatorType::U_SIZEOF);
                sizeofNode->location = tkn.location;
                if (isTypeStart(tp, current, src)) {
                    sizeofNode->operand = src.parseType(tp, current, arch);
                } else {
                    sizeofNode->operand = parsePrattExpr(tp, current, src, 0);
                }
                if (tp.pop().type != TokenType::OP_RPAREN) {
                    throw std::runtime_error(std::format("E03xx expected ')' at {}", getLocString(tkn.location))); // E03xx
                }
                result = std::move(sizeofNode);
            }
            break;

        default:
            throw std::runtime_error(std::format("E03xx invalid atomic expression {} at {}", tkn.text, getLocString(tkn.location))); // E03xx
    }
    return result;
}

// parse pratt expression
std::unique_ptr<ASTNode> ASTGen::parsePrattExpr(TokenProvider& tp, ScopeNode& current, SrcFile& src, int level) {
    std::unique_ptr<ASTNode> lhs = parseAtomicExpr(tp, current, src); // LHS is start of expression
    while (tp.canPop(1)) {
        int mylvl = getPrattPrecedence(tp.seek().type, false);
        if (mylvl < level) {
            break; // end of expression
        }

        Token& opTkn = tp.pop(); // operator can be binary or postfix unary
        switch (opTkn.type) {
            case TokenType::OP_DOT: // member access
                {
                    Token& memberTkn = tp.pop();
                    if (memberTkn.type != TokenType::IDENTIFIER) {
                        throw std::runtime_error(std::format("E03xx expected identifier after '.' at {}", getLocString(opTkn.location))); // E03xx
                    }
                    std::unique_ptr<BinaryOpNode> memberNode = std::make_unique<BinaryOpNode>();
                    memberNode->subtype = OperatorType::B_DOT;
                    memberNode->location = opTkn.location;
                    memberNode->left = std::move(lhs);
                    std::unique_ptr<ASTNode> memberNameNode = std::make_unique<ASTNode>(ASTNodeType::NAME, memberTkn.text);
                    memberNameNode->location = memberTkn.location;
                    memberNode->right = std::move(memberNameNode);
                    lhs = std::move(memberNode);
                }
                break;

            case TokenType::OP_LPAREN: // function call
                {
                    std::unique_ptr<FuncCallNode> callNode = std::make_unique<FuncCallNode>();
                    callNode->location = opTkn.location;
                    callNode->func_expr = std::move(lhs);
                    if (tp.seek().type != TokenType::OP_RPAREN) {
                        while (tp.canPop(1)) {
                            std::unique_ptr<ASTNode> argExpr = parsePrattExpr(tp, current, src, 0);
                            callNode->args.push_back(std::move(argExpr));
                            if (tp.seek().type == TokenType::OP_COMMA) {
                                tp.pop();
                            } else if (tp.seek().type == TokenType::OP_RPAREN) {
                                break;
                            } else {
                                throw std::runtime_error(std::format("E03xx expected ',' at {}", getLocString(opTkn.location))); // E03xx
                            }
                        }
                    }
                    if (tp.pop().type != TokenType::OP_RPAREN) {
                        throw std::runtime_error(std::format("E03xx expected ')' at {}", getLocString(opTkn.location))); // E03xx
                    }
                    lhs = std::move(callNode);
                }
                break;

            case TokenType::OP_LBRACKET: // index, slice
                {
                    std::unique_ptr<ASTNode> left = nullptr;
                    std::unique_ptr<ASTNode> right = nullptr;
                    left = parsePrattExpr(tp, current, src, 0);
                    if (tp.seek().type == TokenType::OP_COLON) { // slice
                        tp.pop();
                        right = parsePrattExpr(tp, current, src, 0);
                    }
                    if (tp.pop().type != TokenType::OP_RBRACKET) {
                        throw std::runtime_error(std::format("E03xx expected ']' at {}", getLocString(opTkn.location))); // E03xx
                    }
                    if (right == nullptr) { // index
                        std::unique_ptr<BinaryOpNode> indexNode = std::make_unique<BinaryOpNode>();
                        indexNode->subtype = OperatorType::B_INDEX;
                        indexNode->location = opTkn.location;
                        indexNode->left = std::move(lhs);
                        indexNode->right = std::move(left);
                        lhs = std::move(indexNode);
                    } else { // slice
                        std::unique_ptr<TripleOpNode> sliceNode = std::make_unique<TripleOpNode>();
                        sliceNode->subtype = OperatorType::T_SLICE;
                        sliceNode->location = opTkn.location;
                        sliceNode->base = std::move(lhs);
                        sliceNode->left = std::move(left);
                        sliceNode->right = std::move(right);
                        lhs = std::move(sliceNode);
                    }
                }
                break;

            default: // binary operator
                {
                    std::unique_ptr<BinaryOpNode> binOpNode = std::make_unique<BinaryOpNode>();
                    binOpNode->subtype = getBinaryOpType(opTkn.type);
                    if (binOpNode->subtype == OperatorType::NONE) {
                        throw std::runtime_error(std::format("E03xx invalid binary operator {} at {}", opTkn.text, getLocString(opTkn.location))); // E03xx
                    }
                    binOpNode->location = opTkn.location;
                    binOpNode->left = std::move(lhs);
                    binOpNode->right = parsePrattExpr(tp, current, src, mylvl + 1);
                    lhs = std::move(binOpNode);
                }
        }
    }
    return lhs;
}