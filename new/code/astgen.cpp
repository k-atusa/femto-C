#include "astgen.h"

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
                } else if (tp.match({TokenType::LIT_INT10, TokenType::OP_RBRACKET}) || tp.match({TokenType::LIT_INT16, TokenType::OP_RBRACKET})) { // array
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