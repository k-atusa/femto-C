#include <algorithm>
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
            case TokenType::OP_LT: case TokenType::OP_LT_EQ: case TokenType::OP_GT: case TokenType::OP_GT_EQ:
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
OperationType getBinaryOpType(TokenType tknType) {
    switch (tknType) {
        case TokenType::OP_MUL:
            return OperationType::B_MUL;
        case TokenType::OP_DIV:
            return OperationType::B_DIV;
        case TokenType::OP_REMAIN:
            return OperationType::B_MOD;
        case TokenType::OP_PLUS:
            return OperationType::B_ADD;
        case TokenType::OP_MINUS:
            return OperationType::B_SUB;
        case TokenType::OP_BIT_LSHIFT:
            return OperationType::B_SHL;
        case TokenType::OP_BIT_RSHIFT:
            return OperationType::B_SHR;
        case TokenType::OP_LT:
            return OperationType::B_LT;
        case TokenType::OP_GT:
            return OperationType::B_GT;
        case TokenType::OP_LT_EQ:
            return OperationType::B_LE;
        case TokenType::OP_GT_EQ:
            return OperationType::B_GE;
        case TokenType::OP_EQ:
            return OperationType::B_EQ;
        case TokenType::OP_NOT_EQ:
            return OperationType::B_NE;
        case TokenType::OP_BIT_AND:
            return OperationType::B_BIT_AND;
        case TokenType::OP_BIT_XOR:
            return OperationType::B_BIT_XOR;
        case TokenType::OP_BIT_OR:
            return OperationType::B_BIT_OR;
        case TokenType::OP_LOGIC_AND:
            return OperationType::B_LOGIC_AND;
        case TokenType::OP_LOGIC_OR:
            return OperationType::B_LOGIC_OR;
        default:
            return OperationType::NONE;
    }
}

DeclVarNode* ScopeNode::findVarByName(const std::string& name) {
    for (auto& node : body) { // all variable declarations should be decl_var
        if (node->objType == ASTNodeType::DECL_VAR && node->text == name) {
            return static_cast<DeclVarNode*>(node.get());
        }
    }
    if (parent) { // find in parent
        return parent->findVarByName(name);
    }
    return nullptr;
}

Literal ScopeNode::findDefinedLiteral(const std::string& name) {
    DeclVarNode* varNode = findVarByName(name);
    if (varNode == nullptr || !varNode->isDefine) {
        return Literal();
    }
    AtomicExprNode* litNode = static_cast<AtomicExprNode*>(varNode->varExpr.get());
    if (litNode->objType == ASTNodeType::LITERAL) {
        return litNode->literal;
    } else if (litNode->objType == ASTNodeType::LITERAL_KEY) {
        return Literal(litNode->word == "true" ? (int64_t)1 : (int64_t)0);
    } else {
        return Literal();
    }
}

// SrcFile functions
ASTNode* SrcFile::findNodeByName(ASTNodeType tp, const std::string& name, bool checkExported) {
    ASTNode* result = nullptr;
    for (auto& node : code->body) {
        if (node->objType == tp && node->text == name) {
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
                if (funcNode->structNm.empty()) { // global function
                    if ('A' <= result->text[0] && result->text[0] <= 'Z') {
                        return result;
                    } else {
                        return nullptr;
                    }
                } else { // method
                    if ('A' <= funcNode->structNm[0] && funcNode->structNm[0] <= 'Z' && 'A' <= funcNode->funcNm[0] && funcNode->funcNm[0] <= 'Z') {
                        return result;
                    } else {
                        return nullptr;
                    }
                }
            }
    }
    return result;
}

Literal SrcFile::findConstByName(const std::string& name, bool checkExported) {
    if (name.contains('.')) { // enum member
        size_t pos = name.find('.');
        std::string enumName = name.substr(0, pos);
        std::string memberName = name.substr(pos + 1);
        if (checkExported && (enumName[0] < 'A' || 'Z' < enumName[0] || memberName[0] < 'A' || 'Z' < memberName[0])) {
            return Literal();
        }
        DeclEnumNode* enumNode = static_cast<DeclEnumNode*>(findNodeByName(ASTNodeType::DECL_ENUM, enumName, false));
        if (enumNode == nullptr) {
            return Literal();
        }
        for (size_t i = 0; i < enumNode->memNames.size(); i++) {
            if (enumNode->memNames[i] == memberName) {
                return Literal(enumNode->memValues[i]);
            }
        }
        return Literal();
    } else { // defined literal
        if (checkExported && (name[0] < 'A' || 'Z' < name[0])) {
            return Literal();
        }
        return code->findDefinedLiteral(name);
    }
}

std::string SrcFile::isNameUsable(const std::string& name, Location loc) {
    // toplevel names used: include, template, var, func, struct, enum
    if (findNodeByName(ASTNodeType::INCLUDE, name, false) != nullptr) {
        return std::format("E0201 name {} already used by include at {}:{}", name, path, loc.line); // E0201
    } else if (findNodeByName(ASTNodeType::DECL_TEMPLATE, name, false) != nullptr) {
        return std::format("E0202 name {} already used by template at {}:{}", name, path, loc.line); // E0202
    } else if (findNodeByName(ASTNodeType::DECL_VAR, name, false) != nullptr) {
        return std::format("E0203 name {} already used by variable at {}:{}", name, path, loc.line); // E0203
    } else if (findNodeByName(ASTNodeType::DECL_FUNC, name, false) != nullptr) {
        return std::format("E0204 name {} already used by function at {}:{}", name, path, loc.line); // E0204
    } else if (findNodeByName(ASTNodeType::DECL_STRUCT, name, false) != nullptr) {
        return std::format("E0205 name {} already used by struct at {}:{}", name, path, loc.line); // E0205
    } else if (findNodeByName(ASTNodeType::DECL_ENUM, name, false) != nullptr) {
        return std::format("E0206 name {} already used by enum at {}:{}", name, path, loc.line); // E0206
    } else {
        return "";
    }
}

std::unique_ptr<TypeNode> SrcFile::parseType(TokenProvider& tp, ScopeNode& current, int arch) {
    // parse base type
    std::unique_ptr<TypeNode> result;
    if (tp.match({TokenType::IDENTIFIER, TokenType::OP_DOT, TokenType::IDENTIFIER})) { // foreign type
        Token& includeTkn = tp.pop();
        tp.pop();
        Token& nameTkn = tp.pop();
        if (findNodeByName(ASTNodeType::INCLUDE, includeTkn.text, false) == nullptr) {
            throw std::runtime_error(std::format("E0207 include name {} not found at {}:{}", includeTkn.text, path, includeTkn.location.line)); // E0207
        }
        result = std::make_unique<TypeNode>(includeTkn.text, nameTkn.text);
        result->location = includeTkn.location;

    } else if (tp.match({TokenType::IDENTIFIER})) { // tmp, struct, enum
        Token& nameTkn = tp.pop();
        result = std::make_unique<TypeNode>(nameTkn.text);
        result->location = nameTkn.location;

    } else if (tp.canPop(1)) { // primitive
        Token& baseTkn = tp.pop();
        result = std::make_unique<TypeNode>(TypeNodeType::PRIMITIVE, baseTkn.text);
        result->location = baseTkn.location;
        switch (baseTkn.objType) {
            case TokenType::KEY_I8: case TokenType::KEY_U8:
                result->typeSize = 1;
                result->typeAlign = 1;
                break;
            case TokenType::KEY_I16: case TokenType::KEY_U16:
                result->typeSize = 2;
                result->typeAlign = 2;
                break;
            case TokenType::KEY_I32: case TokenType::KEY_U32: case TokenType::KEY_F32:
                result->typeSize = 4;
                result->typeAlign = 4;
                break;
            case TokenType::KEY_I64: case TokenType::KEY_U64: case TokenType::KEY_F64:
                result->typeSize = 8;
                result->typeAlign = 8;
                break;
            case TokenType::KEY_VOID:
                result->typeSize = 0;
                result->typeAlign = 1;
                break;
            default:
                throw std::runtime_error(std::format("E0208 invalid type start {} at {}:{}", baseTkn.text, path, baseTkn.location.line)); // E0208
        }

    } else {
        throw std::runtime_error("E0209 unexpected EOF while parsing type"); // E0209
    }

    // parse type modifiers
    while (tp.canPop(1)) {
        Token& tkn = tp.pop();
        switch (tkn.objType) {
            case TokenType::OP_MUL: // pointer
                {
                    std::unique_ptr<TypeNode> ptrType = std::make_unique<TypeNode>(TypeNodeType::POINTER, "*");
                    ptrType->location = result->location;
                    ptrType->typeSize = arch;
                    ptrType->typeAlign = arch;
                    ptrType->direct = std::move(result);
                    result = std::move(ptrType);
                }
                break;

            case TokenType::OP_LBRACKET:
                if (result->typeSize == 0) {
                    throw std::runtime_error(std::format("E0210 cannot create array/slice of void type at {}:{}", path, tkn.location.line)); // E0210
                }
                if (tp.match({TokenType::OP_RBRACKET})) { // slice
                    tp.pop();
                    std::unique_ptr<TypeNode> sliceType = std::make_unique<TypeNode>(TypeNodeType::SLICE, "[]");
                    sliceType->location = result->location;
                    sliceType->typeSize = arch * 2; // ptr + length
                    sliceType->typeAlign = arch;
                    if (result->subType == TypeNodeType::ARRAY || result->subType == TypeNodeType::SLICE) { // nested array
                        TypeNode* curr = result.get();
                        while (curr->direct && (curr->direct->subType == TypeNodeType::ARRAY || curr->direct->subType == TypeNodeType::SLICE)) {
                            curr = curr->direct.get();
                        }
                        sliceType->direct = std::move(curr->direct);
                        curr->direct = std::move(sliceType);
                    } else {
                        sliceType->direct = std::move(result);
                        result = std::move(sliceType);
                    }

                } else if (tp.match({TokenType::LIT_INT, TokenType::OP_RBRACKET})) { // array
                    Token& lenTkn = tp.pop();
                    int64_t len = lenTkn.value.intValue;
                    if (len <= 0) {
                        throw std::runtime_error(std::format("E0211 invalid array length {} at {}:{}", len, path, lenTkn.location.line)); // E0211
                    }
                    tp.pop();
                    std::unique_ptr<TypeNode> arrType = std::make_unique<TypeNode>(TypeNodeType::ARRAY, std::format("[{}]", len));
                    arrType->location = result->location;
                    arrType->length = len;
                    if (result->subType == TypeNodeType::ARRAY || result->subType == TypeNodeType::SLICE) { // nested array
                        TypeNode* curr = result.get();
                        while (curr->direct && (curr->direct->subType == TypeNodeType::ARRAY || curr->direct->subType == TypeNodeType::SLICE)) {
                            curr = curr->direct.get();
                        }
                        arrType->direct = std::move(curr->direct);
                        curr->direct = std::move(arrType);
                    } else {
                        arrType->direct = std::move(result);
                        result = std::move(arrType);
                    }

                } else if (tp.match({TokenType::IDENTIFIER, TokenType::OP_RBRACKET})) { // array with defined size
                    Token& lenTkn = tp.pop();
                    Literal lenLit = current.findDefinedLiteral(lenTkn.text);
                    if (lenLit.objType == LiteralType::NONE) {
                        throw std::runtime_error(std::format("E0212 name {} not found at {}:{}", lenTkn.text, path, lenTkn.location.line)); // E0212
                    }
                    if (lenLit.objType != LiteralType::INT || lenLit.intValue <= 0) {
                        throw std::runtime_error(std::format("E0213 name {} cannot be used as array length at {}:{}", lenTkn.text, path, lenTkn.location.line)); // E0213
                    }
                    int64_t len = lenLit.intValue;
                    tp.pop();
                    std::unique_ptr<TypeNode> arrType = std::make_unique<TypeNode>(TypeNodeType::ARRAY, std::format("[{}]", len));
                    arrType->location = result->location;
                    arrType->length = len;
                    if (result->subType == TypeNodeType::ARRAY || result->subType == TypeNodeType::SLICE) { // nested array
                        TypeNode* curr = result.get();
                        while (curr->direct && (curr->direct->subType == TypeNodeType::ARRAY || curr->direct->subType == TypeNodeType::SLICE)) {
                            curr = curr->direct.get();
                        }
                        arrType->direct = std::move(curr->direct);
                        curr->direct = std::move(arrType);
                    } else {
                        arrType->direct = std::move(result);
                        result = std::move(arrType);
                    }

                } else {
                    throw std::runtime_error(std::format("E0214 expected ']' at {}:{}", path, tkn.location.line)); // E0214
                }
                break;

            case TokenType::OP_LPAREN: // function
                {
                    std::unique_ptr<TypeNode> funcType = std::make_unique<TypeNode>(TypeNodeType::FUNCTION, "()");
                    funcType->location = result->location;
                    funcType->typeSize = arch;
                    funcType->typeAlign = arch;
                    funcType->direct = std::move(result);
                    result = std::move(funcType);
                    if (tp.seek().objType != TokenType::OP_RPAREN) {
                        while (tp.canPop(1)) {
                            std::unique_ptr<TypeNode> argType = parseType(tp, current, arch);
                            result->indirect.push_back(std::move(argType));
                            if (tp.seek().objType == TokenType::OP_COMMA) {
                                tp.pop();
                            } else if (tp.seek().objType == TokenType::OP_RPAREN) {
                                break;
                            } else {
                                throw std::runtime_error(std::format("E0215 expected ')' at {}:{}", path, tkn.location.line)); // E0215
                            }
                        }
                    }
                    if (tp.pop().objType != TokenType::OP_RPAREN) {
                        throw std::runtime_error(std::format("E0216 expected ')' at {}:{}", path, tkn.location.line)); // E0216
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

bool ASTGen::isTypeStart(TokenProvider& tp, SrcFile& src) {
    if (isPrimitive(tp.seek().objType)) { // primitive type
        return true;
    }

    if (tp.match({TokenType::IDENTIFIER, TokenType::OP_DOT, TokenType::IDENTIFIER})) { // foreign type
        Token& includeTkn = tp.pop();
        tp.pop();
        Token& nameTkn = tp.pop();
        Token& nextTkn = tp.pop();
        tp.pos -= 4;
        IncludeNode* includeNode = static_cast<IncludeNode*>(src.findNodeByName(ASTNodeType::INCLUDE, includeTkn.text, false));
        if (includeNode == nullptr) {
            return false;
        }
        int index = findSource(includeNode->path);
        if (index == -1) {
            throw std::runtime_error(std::format("E0301 included module {} not found at {}", includeNode->path, getLocString(includeTkn.location))); // E0301
        }
        if (srcFiles[index]->findNodeByName(ASTNodeType::DECL_STRUCT, nameTkn.text, true) != nullptr && nextTkn.objType != TokenType::OP_DOT) {
            return true; // struct.member is not a type
        }
        if (srcFiles[index]->findNodeByName(ASTNodeType::DECL_ENUM, nameTkn.text, true) != nullptr && nextTkn.objType != TokenType::OP_DOT) {
            return true; // enum.member is not a type
        }

    } else if (tp.match({TokenType::IDENTIFIER})) { // tmp, struct, enum
        Token& nameTkn = tp.pop();
        Token& nextTkn = tp.pop();
        tp.pos -= 2;
        if (src.findNodeByName(ASTNodeType::DECL_TEMPLATE, nameTkn.text, false) != nullptr) {
            return true; // template
        }
        if (src.findNodeByName(ASTNodeType::DECL_STRUCT, nameTkn.text, false) != nullptr && nextTkn.objType != TokenType::OP_DOT) {
            return true; // struct.member is not a type
        }
        if (src.findNodeByName(ASTNodeType::DECL_ENUM, nameTkn.text, false) != nullptr && nextTkn.objType != TokenType::OP_DOT) {
            return true; // enum.member is not a type
        }
    }
    return false;
}

// ASTNode functions
Literal ASTGen::foldNode(ASTNode& tgt, ScopeNode& current, SrcFile& src) {
    if (tgt.objType == ASTNodeType::LITERAL) { // literal
        return static_cast<AtomicExprNode*>(&tgt)->literal;
    } else if (tgt.objType == ASTNodeType::NAME) { // check if defined literal
        return current.findDefinedLiteral(tgt.text);
    } else if (tgt.objType == ASTNodeType::OPERATION) { // fold operation
        OperationNode* opNode = static_cast<OperationNode*>(&tgt);

        // fold operands first
        Literal folded0 = Literal();
        Literal folded1 = Literal();
        Literal folded2 = Literal();
        if (opNode->operand0) {
            folded0 = foldNode(*opNode->operand0, current, src);
            if (folded0.objType != LiteralType::NONE) {
                Location loc = opNode->operand0->location;
                opNode->operand0 = std::make_unique<AtomicExprNode>(folded0);
                opNode->operand0->location = loc;
            }
        }
        if (opNode->operand1) {
            folded1 = foldNode(*opNode->operand1, current, src);
            if (folded1.objType != LiteralType::NONE) {
                Location loc = opNode->operand1->location;
                opNode->operand1 = std::make_unique<AtomicExprNode>(folded1);
                opNode->operand1->location = loc;
            }
        }
        if (opNode->operand2) {
            folded2 = foldNode(*opNode->operand2, current, src);
            if (folded2.objType != LiteralType::NONE) {
                Location loc = opNode->operand2->location;
                opNode->operand2 = std::make_unique<AtomicExprNode>(folded2);
                opNode->operand2->location = loc;
            }
        }

        // convert literal_key to int for logic operations
        if (opNode->subType == OperationType::U_LOGIC_NOT) {
            if (opNode->operand0->objType == ASTNodeType::LITERAL_KEY) {
                folded0 = Literal(opNode->operand0->text == "true" ? (int64_t)1 : (int64_t)0);
                Location loc = opNode->operand0->location;
                opNode->operand0 = std::make_unique<AtomicExprNode>(folded0);
                opNode->operand0->location = loc;
            }
        } else if (opNode->subType == OperationType::B_EQ || opNode->subType == OperationType::B_NE
                || opNode->subType == OperationType::B_LOGIC_AND || opNode->subType == OperationType::B_LOGIC_OR) {
            if (opNode->operand0->objType == ASTNodeType::LITERAL_KEY) {
                folded0 = Literal(opNode->operand0->text == "true" ? (int64_t)1 : (int64_t)0);
                Location loc = opNode->operand0->location;
                opNode->operand0 = std::make_unique<AtomicExprNode>(folded0);
                opNode->operand0->location = loc;
            }
            if (opNode->operand1->objType == ASTNodeType::LITERAL_KEY) {
                folded1 = Literal(opNode->operand1->text == "true" ? (int64_t)1 : (int64_t)0);
                Location loc = opNode->operand1->location;
                opNode->operand1 = std::make_unique<AtomicExprNode>(folded1);
                opNode->operand1->location = loc;
            }
        }

        int opnum = getOperandNum(opNode->subType);
        if (opnum == 1) { // try to fold unary
            switch (opNode->subType) {
                case OperationType::U_PLUS:
                    if (opNode->operand0->objType == ASTNodeType::LITERAL) {
                        if (folded0.objType == LiteralType::INT || folded0.objType == LiteralType::CHAR || folded0.objType == LiteralType::FLOAT) {
                            return folded0;
                        }
                    }
                    break;

                case OperationType::U_MINUS:
                    if (opNode->operand0->objType == ASTNodeType::LITERAL) {
                        if (folded0.objType == LiteralType::INT || folded0.objType == LiteralType::CHAR) {
                            return Literal(-folded0.intValue);
                        } else if (folded0.objType == LiteralType::FLOAT) {
                            return Literal(-folded0.floatValue);
                        }
                    }
                    break;

                case OperationType::U_LOGIC_NOT:
                    if (opNode->operand0->objType == ASTNodeType::LITERAL) {
                        if (folded0.objType == LiteralType::INT || folded0.objType == LiteralType::CHAR) {
                            return Literal(folded0.intValue == 0 ? (int64_t)1 : (int64_t)0);
                        }
                    }
                    break;

                case OperationType::U_BIT_NOT:
                    if (opNode->operand0->objType == ASTNodeType::LITERAL) {
                        if (folded0.objType == LiteralType::INT || folded0.objType == LiteralType::CHAR) {
                            return Literal(~folded0.intValue);
                        }
                    }
                    break;

                case OperationType::U_SIZEOF:
                    if (opNode->operand0->objType == ASTNodeType::LITERAL) {
                        if (folded0.objType == LiteralType::INT || folded0.objType == LiteralType::FLOAT) {
                            return Literal((int64_t)8);
                        } else if (folded0.objType == LiteralType::CHAR) {
                            return Literal((int64_t)1);
                        } else if (folded0.objType == LiteralType::STRING) {
                            return Literal((int64_t)arch * 2);
                        }
                    } else if (opNode->operand0->objType == ASTNodeType::TYPE) {
                        TypeNode* tp = static_cast<TypeNode*>(opNode->operand0.get());
                        if (tp->typeSize > 0) {
                            return Literal((int64_t)tp->typeSize);
                        }
                    }
                    break;
            }

        } else if (opnum == 2 && opNode->subType != OperationType::B_DOT) { // try to fold binary
            switch (opNode->subType) {
                case OperationType::B_MUL:
                    if (opNode->operand0->objType == ASTNodeType::LITERAL && opNode->operand1->objType == ASTNodeType::LITERAL) {
                        if ((folded0.objType == LiteralType::INT || folded0.objType == LiteralType::CHAR) &&
                                (folded1.objType == LiteralType::INT || folded1.objType == LiteralType::CHAR)) {
                            return Literal(folded0.intValue * folded1.intValue);
                        } else if (folded0.objType == LiteralType::FLOAT && folded1.objType == LiteralType::FLOAT) {
                            return Literal(folded0.floatValue * folded1.floatValue);
                        }
                    }
                    break;

                case OperationType::B_DIV:
                    if (opNode->operand0->objType == ASTNodeType::LITERAL && opNode->operand1->objType == ASTNodeType::LITERAL) {
                        if ((folded0.objType == LiteralType::INT || folded0.objType == LiteralType::CHAR) &&
                                (folded1.objType == LiteralType::INT || folded1.objType == LiteralType::CHAR)) {
                            if (folded1.intValue == 0) throw std::runtime_error(std::format("E0302 division by zero at {}", getLocString(opNode->location))); // E0302
                            if (folded0.intValue == INT64_MIN && folded1.intValue == -1) throw std::runtime_error(std::format("E03xx division overflow at {}", getLocString(opNode->location))); // E03xx
                            return Literal(folded0.intValue / folded1.intValue);
                        } else if (folded0.objType == LiteralType::FLOAT && folded1.objType == LiteralType::FLOAT) {
                            if (folded1.floatValue == 0.0) throw std::runtime_error(std::format("E0303 division by zero at {}", getLocString(opNode->location))); // E0303
                            return Literal(folded0.floatValue / folded1.floatValue);
                        }
                    }
                    break;

                case OperationType::B_MOD:
                    if (opNode->operand0->objType == ASTNodeType::LITERAL && opNode->operand1->objType == ASTNodeType::LITERAL) {
                        if ((folded0.objType == LiteralType::INT || folded0.objType == LiteralType::CHAR) &&
                                (folded1.objType == LiteralType::INT || folded1.objType == LiteralType::CHAR)) {
                            if (folded1.intValue == 0) throw std::runtime_error(std::format("E0304 division by zero at {}", getLocString(opNode->location))); // E0304
                            if (folded0.intValue == INT64_MIN && folded1.intValue == -1) throw std::runtime_error(std::format("E0305 division overflow at {}", getLocString(opNode->location))); // E0305
                            return Literal(folded0.intValue % folded1.intValue);
                        }
                    }
                    break;

                case OperationType::B_ADD:
                    if (opNode->operand0->objType == ASTNodeType::LITERAL && opNode->operand1->objType == ASTNodeType::LITERAL) {
                        if ((folded0.objType == LiteralType::INT || folded0.objType == LiteralType::CHAR) &&
                                (folded1.objType == LiteralType::INT || folded1.objType == LiteralType::CHAR)) {
                            return Literal(folded0.intValue + folded1.intValue);
                        } else if (folded0.objType == LiteralType::FLOAT && folded1.objType == LiteralType::FLOAT) {
                            return Literal(folded0.floatValue + folded1.floatValue);
                        }
                    }
                    break;

                case OperationType::B_SUB:
                    if (opNode->operand0->objType == ASTNodeType::LITERAL && opNode->operand1->objType == ASTNodeType::LITERAL) {
                        if ((folded0.objType == LiteralType::INT || folded0.objType == LiteralType::CHAR) &&
                                (folded1.objType == LiteralType::INT || folded1.objType == LiteralType::CHAR)) {
                            return Literal(folded0.intValue - folded1.intValue);
                        } else if (folded0.objType == LiteralType::FLOAT && folded1.objType == LiteralType::FLOAT) {
                            return Literal(folded0.floatValue - folded1.floatValue);
                        }
                    }
                    break;

                case OperationType::B_SHL:
                    if (opNode->operand0->objType == ASTNodeType::LITERAL && opNode->operand1->objType == ASTNodeType::LITERAL) {
                        if ((folded0.objType == LiteralType::INT || folded0.objType == LiteralType::CHAR) &&
                                (folded1.objType == LiteralType::INT || folded1.objType == LiteralType::CHAR)) {
                            if (folded1.intValue < 0 || folded1.intValue > 63) throw std::runtime_error(std::format("E0306 shift amount out of range at {}", getLocString(opNode->location))); // E0306
                            return Literal(folded0.intValue << folded1.intValue);
                        }
                    }
                    break;

                case OperationType::B_SHR:
                    if (opNode->operand0->objType == ASTNodeType::LITERAL && opNode->operand1->objType == ASTNodeType::LITERAL) {
                        if ((folded0.objType == LiteralType::INT || folded0.objType == LiteralType::CHAR) &&
                                (folded1.objType == LiteralType::INT || folded1.objType == LiteralType::CHAR)) {
                            if (folded1.intValue < 0 || folded1.intValue > 63) throw std::runtime_error(std::format("E0307 shift amount out of range at {}", getLocString(opNode->location))); // E0307
                            return Literal(folded0.intValue >> folded1.intValue);
                        }
                    }
                    break;

                case OperationType::B_LT:
                    if (opNode->operand0->objType == ASTNodeType::LITERAL && opNode->operand1->objType == ASTNodeType::LITERAL) {
                        if ((folded0.objType == LiteralType::INT || folded0.objType == LiteralType::CHAR) &&
                                (folded1.objType == LiteralType::INT || folded1.objType == LiteralType::CHAR)) {
                            return Literal(folded0.intValue < folded1.intValue ? (int64_t)1 : (int64_t)0);
                        } else if (folded0.objType == LiteralType::FLOAT && folded1.objType == LiteralType::FLOAT) {
                            return Literal(folded0.floatValue < folded1.floatValue ? (int64_t)1 : (int64_t)0);
                        }
                    }
                    break;

                case OperationType::B_LE:
                    if (opNode->operand0->objType == ASTNodeType::LITERAL && opNode->operand1->objType == ASTNodeType::LITERAL) {
                        if ((folded0.objType == LiteralType::INT || folded0.objType == LiteralType::CHAR) &&
                                (folded1.objType == LiteralType::INT || folded1.objType == LiteralType::CHAR)) {
                            return Literal(folded0.intValue <= folded1.intValue ? (int64_t)1 : (int64_t)0);
                        } else if (folded0.objType == LiteralType::FLOAT && folded1.objType == LiteralType::FLOAT) {
                            return Literal(folded0.floatValue <= folded1.floatValue ? (int64_t)1 : (int64_t)0);
                        }
                    }
                    break;

                case OperationType::B_GT:
                    if (opNode->operand0->objType == ASTNodeType::LITERAL && opNode->operand1->objType == ASTNodeType::LITERAL) {
                        if ((folded0.objType == LiteralType::INT || folded0.objType == LiteralType::CHAR) &&
                                (folded1.objType == LiteralType::INT || folded1.objType == LiteralType::CHAR)) {
                            return Literal(folded0.intValue > folded1.intValue ? (int64_t)1 : (int64_t)0);
                        } else if (folded0.objType == LiteralType::FLOAT && folded1.objType == LiteralType::FLOAT) {
                            return Literal(folded0.floatValue > folded1.floatValue ? (int64_t)1 : (int64_t)0);
                        }
                    }
                    break;

                case OperationType::B_GE:
                    if (opNode->operand0->objType == ASTNodeType::LITERAL && opNode->operand1->objType == ASTNodeType::LITERAL) {
                        if ((folded0.objType == LiteralType::INT || folded0.objType == LiteralType::CHAR) &&
                                (folded1.objType == LiteralType::INT || folded1.objType == LiteralType::CHAR)) {
                            return Literal(folded0.intValue >= folded1.intValue ? (int64_t)1 : (int64_t)0);
                        } else if (folded0.objType == LiteralType::FLOAT && folded1.objType == LiteralType::FLOAT) {
                            return Literal(folded0.floatValue >= folded1.floatValue ? (int64_t)1 : (int64_t)0);
                        }
                    }
                    break;

                case OperationType::B_EQ:
                    if (opNode->operand0->objType == ASTNodeType::LITERAL && opNode->operand1->objType == ASTNodeType::LITERAL) {
                        if ((folded0.objType == LiteralType::INT || folded0.objType == LiteralType::CHAR) &&
                                (folded1.objType == LiteralType::INT || folded1.objType == LiteralType::CHAR)) {
                            return Literal(folded0.intValue == folded1.intValue ? (int64_t)1 : (int64_t)0);
                        } else if (folded0.objType == LiteralType::FLOAT && folded1.objType == LiteralType::FLOAT) {
                            return Literal(folded0.floatValue == folded1.floatValue ? (int64_t)1 : (int64_t)0);
                        }
                    }
                    break;

                case OperationType::B_NE:
                    if (opNode->operand0->objType == ASTNodeType::LITERAL && opNode->operand1->objType == ASTNodeType::LITERAL) {
                        if ((folded0.objType == LiteralType::INT || folded0.objType == LiteralType::CHAR) &&
                                (folded1.objType == LiteralType::INT || folded1.objType == LiteralType::CHAR)) {
                            return Literal(folded0.intValue == folded1.intValue ? (int64_t)0 : (int64_t)1);
                        } else if (folded0.objType == LiteralType::FLOAT && folded1.objType == LiteralType::FLOAT) {
                            return Literal(folded0.floatValue == folded1.floatValue ? (int64_t)0 : (int64_t)1);
                        }
                    }
                    break;

                case OperationType::B_BIT_AND:
                    if (opNode->operand0->objType == ASTNodeType::LITERAL && opNode->operand1->objType == ASTNodeType::LITERAL) {
                        if ((folded0.objType == LiteralType::INT || folded0.objType == LiteralType::CHAR) &&
                                (folded1.objType == LiteralType::INT || folded1.objType == LiteralType::CHAR)) {
                            return Literal(folded0.intValue & folded1.intValue);
                        }
                    }
                    break;

                case OperationType::B_BIT_XOR:
                    if (opNode->operand0->objType == ASTNodeType::LITERAL && opNode->operand1->objType == ASTNodeType::LITERAL) {
                        if ((folded0.objType == LiteralType::INT || folded0.objType == LiteralType::CHAR) &&
                                (folded1.objType == LiteralType::INT || folded1.objType == LiteralType::CHAR)) {
                            return Literal(folded0.intValue ^ folded1.intValue);
                        }
                    }
                    break;

                case OperationType::B_BIT_OR:
                    if (opNode->operand0->objType == ASTNodeType::LITERAL && opNode->operand1->objType == ASTNodeType::LITERAL) {
                        if ((folded0.objType == LiteralType::INT || folded0.objType == LiteralType::CHAR) &&
                                (folded1.objType == LiteralType::INT || folded1.objType == LiteralType::CHAR)) {
                            return Literal(folded0.intValue | folded1.intValue);
                        }
                    }
                    break;

                case OperationType::B_LOGIC_AND:
                    if (opNode->operand0->objType == ASTNodeType::LITERAL && opNode->operand1->objType == ASTNodeType::LITERAL) {
                        if ((folded0.objType == LiteralType::INT || folded0.objType == LiteralType::CHAR) &&
                                (folded1.objType == LiteralType::INT || folded1.objType == LiteralType::CHAR)) {
                            return Literal(folded0.intValue != 0 && folded1.intValue != 0 ? (int64_t)1 : (int64_t)0);
                        }
                    }
                    break;

                case OperationType::B_LOGIC_OR:
                    if (opNode->operand0->objType == ASTNodeType::LITERAL && opNode->operand1->objType == ASTNodeType::LITERAL) {
                        if ((folded0.objType == LiteralType::INT || folded0.objType == LiteralType::CHAR) &&
                                (folded1.objType == LiteralType::INT || folded1.objType == LiteralType::CHAR)) {
                            return Literal(folded0.intValue != 0 || folded1.intValue != 0 ? (int64_t)1 : (int64_t)0);
                        }
                    }
                    break;
            }

        } else if (opNode->subType == OperationType::B_DOT) { // enum value or include name
            if (opNode->operand0->objType == ASTNodeType::NAME) {
                std::string name0 = opNode->operand0->text;
                ASTNode* search = src.findNodeByName(ASTNodeType::DECL_ENUM, name0, false);
                if (search != nullptr && opNode->operand1->objType == ASTNodeType::NAME) { // enum value
                    std::string name1 = opNode->operand1->text;
                    return src.findConstByName(name0 + "." + name1, false);
                }
                search = src.findNodeByName(ASTNodeType::INCLUDE, name0, false);
                int pos = -1;
                if (search != nullptr) {
                    pos = findSource(((IncludeNode*)search)->path);
                }
                if (pos != -1 && opNode->operand1->objType == ASTNodeType::NAME) { // foreign defined literal
                    return srcFiles[pos]->findConstByName(opNode->operand1->text, true);
                } else if (pos != -1 && opNode->operand1->objType == ASTNodeType::OPERATION) { // foreign enum value
                    OperationNode* subOpNode = (OperationNode*)opNode->operand1.get();
                    if (subOpNode->subType == OperationType::B_DOT && subOpNode->operand0->objType == ASTNodeType::NAME && subOpNode->operand1->objType == ASTNodeType::NAME) {
                        return srcFiles[pos]->findConstByName(subOpNode->operand0->text + "." + subOpNode->operand1->text, true);
                    }
                }
            }
        }
    }
    return Literal();
}

// parse raw code
std::unique_ptr<RawCodeNode> ASTGen::parseRawCode(TokenProvider& tp) {
    Token& orderTkn = tp.pop();
    std::unique_ptr<RawCodeNode> rawCodeNode = std::make_unique<RawCodeNode>();
    rawCodeNode->location = orderTkn.location;
    if (orderTkn.objType == TokenType::ORDER_RAW_C) {
        rawCodeNode->objType = ASTNodeType::RAW_C;
    } else if (orderTkn.objType == TokenType::ORDER_RAW_IR) {
        rawCodeNode->objType = ASTNodeType::RAW_IR;
    } else {
        throw std::runtime_error(std::format("E0401 expected 'raw_c' at {}", getLocString(orderTkn.location))); // E0401
    }
    Token& textTkn = tp.pop();
    if (textTkn.objType != TokenType::LIT_STRING) {
        throw std::runtime_error(std::format("E0402 expected string literal at {}", getLocString(textTkn.location))); // E0402
    }
    rawCodeNode->code = textTkn.text;
    return rawCodeNode;
}

// parse Struct declaration, after struct keyword
std::unique_ptr<DeclStructNode> ASTGen::parseStruct(TokenProvider& tp, ScopeNode& current, SrcFile& src, bool isExported) {
    // check name validity, create struct node
    Token& idTkn = tp.pop();
    if (idTkn.objType != TokenType::IDENTIFIER) {
        throw std::runtime_error(std::format("E0403 expected identifier at {}", getLocString(idTkn.location))); // E0403
    }
    std::string nmValidity = src.isNameUsable(idTkn.text, idTkn.location);
    if (!nmValidity.empty()) {
        throw std::runtime_error(nmValidity);
    }
    std::unique_ptr<DeclStructNode> structNode = std::make_unique<DeclStructNode>();
    structNode->name = idTkn.text;
    structNode->location = idTkn.location;
    if (tp.pop().objType != TokenType::OP_LBRACE) {
        throw std::runtime_error(std::format("E0404 expected '{{' at {}", getLocString(idTkn.location))); // E0404
    }

    // parse members
    while (tp.canPop(1)) {
        std::unique_ptr<TypeNode> fieldType = src.parseType(tp, current, arch);
        if (fieldType->typeSize == 0) {
            throw std::runtime_error(std::format("E0405 member type cannot be void at {}", getLocString(fieldType->location))); // E0405
        }
        Token& fieldIdTkn = tp.pop();
        if (fieldIdTkn.objType != TokenType::IDENTIFIER) {
            throw std::runtime_error(std::format("E0406 expected identifier at {}", getLocString(fieldIdTkn.location))); // E0406
        }
        for (auto& name : structNode->memNames) {
            if (name == fieldIdTkn.text) {
                throw std::runtime_error(std::format("E0407 member name {} already exists at {}", fieldIdTkn.text, getLocString(fieldIdTkn.location))); // E0407
            }
        }

        // push member
        structNode->memTypes.push_back(std::move(fieldType));
        structNode->memNames.push_back(fieldIdTkn.text);
        structNode->memOffsets.push_back(-1);
        Token& sepTkn = tp.seek();
        if (sepTkn.objType == TokenType::OP_RBRACE) {
            break;
        } else if (sepTkn.objType == TokenType::OP_COMMA || sepTkn.objType == TokenType::OP_SEMICOLON) {
            tp.pop();
            if (tp.seek().objType == TokenType::OP_RBRACE) {
                break;
            }
        } else {
            throw std::runtime_error(std::format("E0408 expected ';' at {}", getLocString(sepTkn.location))); // E0408
        }
    }
    if (tp.pop().objType != TokenType::OP_RBRACE) {
        throw std::runtime_error(std::format("E0409 expected '}}' at {}", getLocString(tp.seek().location))); // E0409
    }
    structNode->isExported = isExported;
    return structNode;
}

// parse Enum declaration, after enum keyword
std::unique_ptr<DeclEnumNode> ASTGen::parseEnum(TokenProvider& tp, ScopeNode& current, SrcFile& src, bool isExported) {
    // check name validity, create enum node
    Token& idTkn = tp.pop();
    if (idTkn.objType != TokenType::IDENTIFIER) {
        throw std::runtime_error(std::format("E0410 expected identifier at {}", getLocString(idTkn.location))); // E0410
    }
    std::string nmValidity = src.isNameUsable(idTkn.text, idTkn.location);
    if (!nmValidity.empty()) {
        throw std::runtime_error(nmValidity);
    }
    std::unique_ptr<DeclEnumNode> enumNode = std::make_unique<DeclEnumNode>();
    enumNode->name = idTkn.text;
    enumNode->location = idTkn.location;
    if (tp.pop().objType != TokenType::OP_LBRACE) {
        throw std::runtime_error(std::format("E0411 expected '{{' at {}", getLocString(idTkn.location))); // E0411
    }

    // parse members
    int64_t prevValue = -1;
    int64_t maxValue = 0;
    int64_t minValue = 0;
    while (tp.canPop(1)) {
        Token& nameTkn = tp.pop();
        if (nameTkn.objType != TokenType::IDENTIFIER) {
            throw std::runtime_error(std::format("E0412 expected identifier at {}", getLocString(nameTkn.location))); // E0412
        }
        for (auto& name : enumNode->memNames) {
            if (name == nameTkn.text) {
                throw std::runtime_error(std::format("E0413 member name {} already exists at {}", nameTkn.text, getLocString(nameTkn.location))); // E0413
            }
        }
        enumNode->memNames.push_back(nameTkn.text);

        // init with value
        if (tp.seek().objType == TokenType::OP_EQ) {
            tp.pop();
            std::unique_ptr<ASTNode> value = parseExpr(tp, current, src);
            if (value->objType != ASTNodeType::LITERAL) {
                throw std::runtime_error(std::format("E0414 expected int constexpr at {}", getLocString(value->location))); // E0414
            }
            Literal lit = static_cast<AtomicExprNode*>(value.get())->literal;
            if (lit.objType != LiteralType::INT && lit.objType != LiteralType::CHAR) {
                throw std::runtime_error(std::format("E0415 expected int constexpr at {}", getLocString(value->location))); // E0415
            }
            prevValue = lit.intValue - 1;
        }

        // push member
        prevValue++;
        enumNode->memValues.push_back(prevValue);
        maxValue = std::max(maxValue, prevValue);
        minValue = std::min(minValue, prevValue);
        Token& sepTkn = tp.seek();
        if (sepTkn.objType == TokenType::OP_RBRACE) {
            break;
        } else if (sepTkn.objType == TokenType::OP_COMMA || sepTkn.objType == TokenType::OP_SEMICOLON) {
            tp.pop();
            if (tp.seek().objType == TokenType::OP_RBRACE) {
                break;
            }
        } else {
            throw std::runtime_error(std::format("E0416 expected ',' at {}", getLocString(sepTkn.location))); // E0416
        }
    }
    if (tp.pop().objType != TokenType::OP_RBRACE) {
        throw std::runtime_error(std::format("E0417 expected '}}' at {}", getLocString(tp.seek().location))); // E0417
    }

    // determine size
    if (maxValue <= 127 && minValue >= -128) {
        enumNode->enumSize = 1;
    } else if (maxValue <= 32767 && minValue >= -32768) {
        enumNode->enumSize = 2;
    } else if (maxValue <= 2147483647 && minValue >= -2147483648) {
        enumNode->enumSize = 4;
    } else {
        enumNode->enumSize = 8;
    }
    enumNode->isExported = isExported;
    return enumNode;
}

// parse function declaration
std::unique_ptr<DeclFuncNode> ASTGen::parseFunc(TokenProvider& tp, ScopeNode& current, SrcFile& src, std::unique_ptr<TypeNode> retType, bool isVaArg, bool isExported) {
    // check name validity, create function node
    std::unique_ptr<DeclFuncNode> funcNode = std::make_unique<DeclFuncNode>();
    funcNode->location = retType->location;
    funcNode->retType = std::move(retType);
    funcNode->body = std::make_unique<ScopeNode>(current);
    if (tp.match({TokenType::IDENTIFIER, TokenType::OP_DOT, TokenType::IDENTIFIER})) { // method
        Token& structTkn = tp.pop();
        tp.pop();
        Token& methodTkn = tp.pop();
        funcNode->name = structTkn.text + "." + methodTkn.text;
        funcNode->structNm = structTkn.text;
        funcNode->funcNm = methodTkn.text;
        if (src.findNodeByName(ASTNodeType::DECL_STRUCT, structTkn.text, false) == nullptr) {
            throw std::runtime_error(std::format("E0418 struct {} is not defined at {}", structTkn.text, getLocString(structTkn.location))); // E0418
        }
    } else if (tp.match({TokenType::IDENTIFIER})) { // function
        Token& idTkn = tp.pop();
        funcNode->name = idTkn.text;
    } else {
        throw std::runtime_error(std::format("E0419 expected identifier at {}", getLocString(funcNode->location))); // E0419
    }
    std::string nmValidity = src.isNameUsable(funcNode->name, funcNode->location);
    if (!nmValidity.empty()) {
        throw std::runtime_error(nmValidity);
    }

    // parse parameters
    if (tp.pop().objType != TokenType::OP_LPAREN) {
        throw std::runtime_error(std::format("E0420 expected '(' at {}", getLocString(funcNode->location))); // E0420
    }
    if (tp.seek().objType != TokenType::OP_RPAREN) {
        while (tp.canPop(1)) {
            std::unique_ptr<TypeNode> paramType = src.parseType(tp, current, arch);
            if (paramType->typeSize == 0) {
                throw std::runtime_error(std::format("E0421 parameter type cannot be void at {}", getLocString(paramType->location))); // E0421
            }
            Token& paramNameTkn = tp.pop();
            if (paramNameTkn.objType != TokenType::IDENTIFIER) {
                throw std::runtime_error(std::format("E0422 expected identifier at {}", getLocString(paramNameTkn.location))); // E0422
            }
            for (auto& name : funcNode->paramNames) {
                if (name == paramNameTkn.text) {
                    throw std::runtime_error(std::format("E0423 parameter name {} is already used at {}", paramNameTkn.text, getLocString(paramNameTkn.location))); // E0423
                }
            }

            // push parameter
            funcNode->paramNames.push_back(paramNameTkn.text);
            funcNode->paramTypes.push_back(paramType->clone());
            std::unique_ptr<DeclVarNode> pvar = std::make_unique<DeclVarNode>(std::move(paramType), paramNameTkn.text);
            pvar->location = paramNameTkn.location;
            pvar->isParam = true;
            funcNode->body->body.push_back(std::move(pvar));
            Token& sepTkn = tp.seek();
            if (sepTkn.objType == TokenType::OP_RPAREN) {
                break;
            } else if (sepTkn.objType == TokenType::OP_COMMA) {
                tp.pop();
            } else {
                throw std::runtime_error(std::format("E0424 expected ')' at {}", getLocString(sepTkn.location))); // E0424
            }
        }
    }
    if (tp.pop().objType != TokenType::OP_RPAREN) {
        throw std::runtime_error(std::format("E0425 expected ')' at {}", getLocString(tp.seek().location))); // E0425
    }

    // parse function body, check function
    funcNode->body->body.push_back(parseScope(tp, *funcNode->body, src));
    funcNode->isVaArg = isVaArg;
    funcNode->isExported = isExported;
    if (!funcNode->structNm.empty()) { // check method
        if (funcNode->paramTypes.size() == 0
                || funcNode->paramTypes[0]->subType != TypeNodeType::POINTER
                || funcNode->paramTypes[0]->direct->subType != TypeNodeType::NAME
                || funcNode->paramTypes[0]->direct->name != funcNode->structNm) {
            throw std::runtime_error(std::format("E0426 first parameter must be {}* at {}", funcNode->structNm, getLocString(funcNode->location))); // E0426
        }
    }
    if (funcNode->isVaArg) { // check va_arg
        if (funcNode->paramTypes.size() < 2) {
            throw std::runtime_error(std::format("E0427 last two parameters must be (void**, int) at {}", getLocString(funcNode->location))); // E0427
        }
        TypeNode* arg0 = funcNode->paramTypes[funcNode->paramTypes.size() - 2].get();
        TypeNode* arg1 = funcNode->paramTypes[funcNode->paramTypes.size() - 1].get();
        bool flag0 = false;
        bool flag1 = false;
        if (arg0->subType == TypeNodeType::POINTER
                && arg0->direct->subType == TypeNodeType::POINTER
                && arg0->direct->direct->subType == TypeNodeType::PRIMITIVE
                && arg0->direct->direct->name == "void") {
            flag0 = true;
        }
        if (arg1->subType == TypeNodeType::PRIMITIVE && arg1->typeSize > 0) {
            flag1 = true;
        }
        if (!flag0 || !flag1) {
            throw std::runtime_error(std::format("E0428 last two parameters must be (void**, int) at {}", getLocString(funcNode->location))); // E0428
        }
    }
    return funcNode;
}

// parse atomic expression
std::unique_ptr<ASTNode> ASTGen::parseAtomicExpr(TokenProvider& tp, ScopeNode& current, SrcFile& src) {
    Token& tkn = tp.pop();
    std::unique_ptr<ASTNode> result;
    switch (tkn.objType) {
        case TokenType::LIT_INT: case TokenType::LIT_FLOAT: case TokenType::LIT_CHAR: case TokenType::LIT_STRING: // literal
            {
                std::unique_ptr<AtomicExprNode> litNode = std::make_unique<AtomicExprNode>(tkn.value);
                litNode->location = tkn.location;
                result = std::move(litNode);
            }
            break;

        case TokenType::KEY_NULL: case TokenType::KEY_TRUE: case TokenType::KEY_FALSE: // keyword literals
            {
                std::unique_ptr<AtomicExprNode> litNode = std::make_unique<AtomicExprNode>();
                litNode->objType = ASTNodeType::LITERAL_KEY;
                litNode->location = tkn.location;
                if (tkn.objType == TokenType::KEY_NULL) {
                    litNode->literal = Literal((int64_t)0);
                    litNode->word = "null";
                } else if (tkn.objType == TokenType::KEY_TRUE) {
                    litNode->literal = Literal((int64_t)1);
                    litNode->word = "true";
                } else if (tkn.objType == TokenType::KEY_FALSE) {
                    litNode->literal = Literal((int64_t)0);
                    litNode->word = "false";
                }
                result = std::move(litNode);
            }

        case TokenType::IDENTIFIER: // variable name
            {
                std::unique_ptr<AtomicExprNode> nameNode = std::make_unique<AtomicExprNode>(tkn.text);
                nameNode->location = tkn.location;
                result = std::move(nameNode);
            }
            break;

        case TokenType::OP_LPAREN: // parenthesis expression
            {
                result = parsePrattExpr(tp, current, src, 0);
                if (tp.pop().objType != TokenType::OP_RPAREN) {
                    throw std::runtime_error(std::format("E0501 expected ')' at {}", getLocString(tkn.location))); // E0501
                }
            }
            break;

        case TokenType::OP_LBRACE: // literal array
            {
                std::unique_ptr<AtomicExprNode> arrNode = std::make_unique<AtomicExprNode>();
                arrNode->objType = ASTNodeType::LITERAL_ARRAY;
                arrNode->location = tkn.location;
                while (tp.canPop(1)) {
                    arrNode->elements.push_back(parseExpr(tp, current, src));
                    if (tp.seek().objType == TokenType::OP_COMMA) {
                        tp.pop();
                        if (tp.seek().objType == TokenType::OP_RBRACE) {
                            break;
                        }
                    } else if (tp.seek().objType == TokenType::OP_RBRACE) {
                        break;
                    } else {
                        throw std::runtime_error(std::format("E0502 expected '}}' at {}", getLocString(tkn.location))); // E0502
                    }
                }
                if (tp.pop().objType != TokenType::OP_RBRACE) {
                    throw std::runtime_error(std::format("E0503 expected '}}' at {}", getLocString(tkn.location))); // E0503
                }
                result = std::move(arrNode);
            }
            break;

        case TokenType::OP_PLUS: case TokenType::OP_MINUS: case TokenType::OP_LOGIC_NOT: case TokenType::OP_BIT_NOT: case TokenType::OP_MUL: case TokenType::OP_BIT_AND: // unary operators
            {
                std::unique_ptr<OperationNode> unaryNode = std::make_unique<OperationNode>();
                unaryNode->location = tkn.location;
                if (tkn.objType == TokenType::OP_PLUS) {
                    unaryNode->subType = OperationType::U_PLUS;
                } else if (tkn.objType == TokenType::OP_MINUS) {
                    unaryNode->subType = OperationType::U_MINUS;
                } else if (tkn.objType == TokenType::OP_LOGIC_NOT) {
                    unaryNode->subType = OperationType::U_LOGIC_NOT;
                } else if (tkn.objType == TokenType::OP_BIT_NOT) {
                    unaryNode->subType = OperationType::U_BIT_NOT;
                } else if (tkn.objType == TokenType::OP_MUL) {
                    unaryNode->subType = OperationType::U_DEREF;
                } else if (tkn.objType == TokenType::OP_BIT_AND) {
                    unaryNode->subType = OperationType::U_REF;
                }
                unaryNode->operand0 = parsePrattExpr(tp, current, src, getPrattPrecedence(TokenType::OP_PLUS, true));
                result = std::move(unaryNode);
            }
            break;

        case TokenType::IFUNC_MAKE:
            {
                if (tp.pop().objType != TokenType::OP_LPAREN) {
                    throw std::runtime_error(std::format("E0504 expected '(' at {}", getLocString(tkn.location))); // E0504
                }
                std::unique_ptr<OperationNode> makeNode = std::make_unique<OperationNode>(OperationType::B_MAKE);
                makeNode->location = tkn.location;
                makeNode->operand0 = parsePrattExpr(tp, current, src, 0);
                if (tp.pop().objType != TokenType::OP_COMMA) {
                    throw std::runtime_error(std::format("E0505 expected ',' at {}", getLocString(tkn.location))); // E0505
                }
                makeNode->operand1 = parsePrattExpr(tp, current, src, 0);
                if (tp.pop().objType != TokenType::OP_RPAREN) {
                    throw std::runtime_error(std::format("E0506 expected ')' at {}", getLocString(tkn.location))); // E0506
                }
                result = std::move(makeNode);
            }
            break;

        case TokenType::IFUNC_LEN:
            {
                if (tp.pop().objType != TokenType::OP_LPAREN) {
                    throw std::runtime_error(std::format("E0507 expected '(' at {}", getLocString(tkn.location))); // E0507
                }
                std::unique_ptr<OperationNode> lenNode = std::make_unique<OperationNode>(OperationType::U_LEN);
                lenNode->location = tkn.location;
                lenNode->operand0 = parsePrattExpr(tp, current, src, 0);
                if (tp.pop().objType != TokenType::OP_RPAREN) {
                    throw std::runtime_error(std::format("E0508 expected ')' at {}", getLocString(tkn.location))); // E0508
                }
                result = std::move(lenNode);
            }
            break;

        case TokenType::IFUNC_CAST:
            {
                if (tp.pop().objType != TokenType::OP_LT) {
                    throw std::runtime_error(std::format("E0509 expected '<' at {}", getLocString(tkn.location))); // E0509
                }
                std::unique_ptr<OperationNode> castNode = std::make_unique<OperationNode>(OperationType::B_CAST);
                castNode->location = tkn.location;
                castNode->operand0 = src.parseType(tp, current, arch);
                if (tp.pop().objType != TokenType::OP_GT) {
                    throw std::runtime_error(std::format("E0510 expected '>' at {}", getLocString(tkn.location))); // E0510
                }
                if (tp.pop().objType != TokenType::OP_LPAREN) {
                    throw std::runtime_error(std::format("E0511 expected '(' at {}", getLocString(tkn.location))); // E0511
                }
                castNode->operand1 = parsePrattExpr(tp, current, src, 0);
                if (tp.pop().objType != TokenType::OP_RPAREN) {
                    throw std::runtime_error(std::format("E0512 expected ')' at {}", getLocString(tkn.location))); // E0512
                }
                result = std::move(castNode);
            }
            break;

        case TokenType::IFUNC_SIZEOF:
            {
                if (tp.pop().objType != TokenType::OP_LPAREN) {
                    throw std::runtime_error(std::format("E0513 expected '(' at {}", getLocString(tkn.location))); // E0513
                }
                std::unique_ptr<OperationNode> sizeofNode = std::make_unique<OperationNode>(OperationType::U_SIZEOF);
                sizeofNode->location = tkn.location;
                if (isTypeStart(tp, src)) {
                    sizeofNode->operand0 = src.parseType(tp, current, arch);
                } else {
                    sizeofNode->operand0 = parsePrattExpr(tp, current, src, 0);
                }
                if (tp.pop().objType != TokenType::OP_RPAREN) {
                    throw std::runtime_error(std::format("E0514 expected ')' at {}", getLocString(tkn.location))); // E0514
                }
                result = std::move(sizeofNode);
            }
            break;

        default:
            throw std::runtime_error(std::format("E0515 invalid atomic expr start {} at {}", tkn.text, getLocString(tkn.location))); // E0515
    }
    return result;
}

// parse pratt expression
std::unique_ptr<ASTNode> ASTGen::parsePrattExpr(TokenProvider& tp, ScopeNode& current, SrcFile& src, int level) {
    std::unique_ptr<ASTNode> lhs = parseAtomicExpr(tp, current, src); // LHS is start of expression
    while (tp.canPop(1)) {
        int mylvl = getPrattPrecedence(tp.seek().objType, false);
        if (mylvl < level) {
            break; // end of expression
        }

        Token& opTkn = tp.pop(); // operator can be binary or postfix unary
        switch (opTkn.objType) {
            case TokenType::OP_DOT: // member access
                {
                    Token& memberTkn = tp.pop();
                    if (memberTkn.objType != TokenType::IDENTIFIER) {
                        throw std::runtime_error(std::format("E0516 expected identifier after '.' at {}", getLocString(opTkn.location))); // E0516
                    }
                    std::unique_ptr<OperationNode> memberNode = std::make_unique<OperationNode>(OperationType::B_DOT);
                    memberNode->location = opTkn.location;
                    memberNode->operand0 = std::move(lhs);
                    memberNode->operand1 = std::make_unique<AtomicExprNode>(memberTkn.text);
                    memberNode->operand1->location = memberTkn.location;
                    lhs = std::move(memberNode);
                }
                break;

            case TokenType::OP_LPAREN: // function call
                {
                    std::unique_ptr<FuncCallNode> callNode = std::make_unique<FuncCallNode>();
                    callNode->location = opTkn.location;
                    callNode->funcExpr = std::move(lhs);
                    if (tp.seek().objType != TokenType::OP_RPAREN) {
                        while (tp.canPop(1)) {
                            std::unique_ptr<ASTNode> argExpr = parsePrattExpr(tp, current, src, 0);
                            callNode->args.push_back(std::move(argExpr));
                            if (tp.seek().objType == TokenType::OP_COMMA) {
                                tp.pop();
                            } else if (tp.seek().objType == TokenType::OP_RPAREN) {
                                break;
                            } else {
                                throw std::runtime_error(std::format("E0517 expected ')' at {}", getLocString(opTkn.location))); // E0517
                            }
                        }
                    }
                    if (tp.pop().objType != TokenType::OP_RPAREN) {
                        throw std::runtime_error(std::format("E0518 expected ')' at {}", getLocString(opTkn.location))); // E0518
                    }
                    lhs = std::move(callNode);
                }
                break;

            case TokenType::OP_LBRACKET: // index, slice
                {
                    std::unique_ptr<ASTNode> left = nullptr;
                    std::unique_ptr<ASTNode> right = nullptr;
                    left = parsePrattExpr(tp, current, src, 0);
                    if (tp.seek().objType == TokenType::OP_COLON) { // slice
                        tp.pop();
                        right = parsePrattExpr(tp, current, src, 0);
                    }
                    if (tp.pop().objType != TokenType::OP_RBRACKET) {
                        throw std::runtime_error(std::format("E0519 expected ']' at {}", getLocString(opTkn.location))); // E0519
                    }
                    if (right == nullptr) { // index
                        std::unique_ptr<OperationNode> indexNode = std::make_unique<OperationNode>(OperationType::B_INDEX);
                        indexNode->location = opTkn.location;
                        indexNode->operand0 = std::move(lhs);
                        indexNode->operand1 = std::move(left);
                        lhs = std::move(indexNode);
                    } else { // slice
                        std::unique_ptr<OperationNode> sliceNode = std::make_unique<OperationNode>(OperationType::T_SLICE);
                        sliceNode->location = opTkn.location;
                        sliceNode->operand0 = std::move(lhs);
                        sliceNode->operand1 = std::move(left);
                        sliceNode->operand2 = std::move(right);
                        lhs = std::move(sliceNode);
                    }
                }
                break;

            default: // binary operator
                {
                    std::unique_ptr<OperationNode> binOpNode = std::make_unique<OperationNode>(getBinaryOpType(opTkn.objType));
                    binOpNode->location = opTkn.location;
                    if (binOpNode->subType == OperationType::NONE) {
                        throw std::runtime_error(std::format("E0520 invalid binary operator {} at {}", opTkn.text, getLocString(opTkn.location))); // E0520
                    }
                    binOpNode->operand0 = std::move(lhs);
                    binOpNode->operand1 = parsePrattExpr(tp, current, src, mylvl + 1);
                    lhs = std::move(binOpNode);
                }
        }
    }
    return lhs;
}

// parse expression, fold if possible
std::unique_ptr<ASTNode> ASTGen::parseExpr(TokenProvider& tp, ScopeNode& current, SrcFile& src) {
    std::unique_ptr<ASTNode> expr = parsePrattExpr(tp, current, src, 0);
    Literal lit = foldNode(*expr, current, src);
    if (lit.objType != LiteralType::NONE) {
        Location loc = expr->location;
        expr = std::make_unique<AtomicExprNode>(lit);
        expr->location = loc;
    }
    return expr;
}

// parse variable declaration, after type declaration
std::unique_ptr<DeclVarNode> ASTGen::parseVarDecl(TokenProvider& tp, ScopeNode& current, SrcFile& src, std::unique_ptr<TypeNode> varType, bool isDefine, bool isExtern, bool isExported) {
    // check type validity, create variable declaration node
    if (varType->typeSize == 0) {
        throw std::runtime_error(std::format("E0601 variable cannot be void type at {}", getLocString(varType->location))); // E0601
    }
    Token& nameTkn = tp.pop();
    if (nameTkn.objType != TokenType::IDENTIFIER) {
        throw std::runtime_error(std::format("E0602 expected identifier at {}", getLocString(nameTkn.location))); // E0602
    }
    std::unique_ptr<DeclVarNode> varDecl = std::make_unique<DeclVarNode>(std::move(varType), nameTkn.text);
    varDecl->location = nameTkn.location;

    // check name duplication
    for (auto& stat : current.body) {
        if (stat->objType == ASTNodeType::DECL_VAR && stat->text == nameTkn.text) {
            throw std::runtime_error(std::format("E0603 variable name {} already exists at {}", nameTkn.text, getLocString(nameTkn.location))); // E0603
        }
    }

    // parse variable initialization
    Token& opTkn = tp.pop();
    if (opTkn.objType == TokenType::OP_ASSIGN) {
        varDecl->varExpr = parseExpr(tp, current, src);
        opTkn = tp.pop();
    }
    if (opTkn.objType != TokenType::OP_SEMICOLON) {
        throw std::runtime_error(std::format("E0604 expected ';' at {}", getLocString(opTkn.location))); // E0604
    }

    // check variable declaration
    varDecl->isDefine = isDefine;
    varDecl->isExtern = isExtern;
    varDecl->isExported = isExported;
    if (isDefine) { // define variable should be initialized with literal
        if (varDecl->varExpr == nullptr
                || (varDecl->varExpr->objType != ASTNodeType::LITERAL && varDecl->varExpr->objType != ASTNodeType::LITERAL_KEY)) {
            throw std::runtime_error(std::format("E0605 variable should be initialized with constexpr at {}", getLocString(nameTkn.location))); // E0605
        }
    }
    if (isExtern && varDecl->varExpr != nullptr) { // extern variable should not be initialized
        throw std::runtime_error(std::format("E0606 extern variable should not be initialized at {}", getLocString(nameTkn.location))); // E0606
    }
    if (isExtern && isExported) { // cannot be both extern and exported
        throw std::runtime_error(std::format("E0607 cannot be both extern and exported at {}", getLocString(nameTkn.location))); // E0607
    }
    return varDecl;
}

// parse variable assignment, after lvalue =
std::unique_ptr<AssignNode> ASTGen::parseVarAssign(TokenProvider& tp, ScopeNode& current, SrcFile& src, std::unique_ptr<ASTNode> lvalue, TokenType endExpect) {
    std::unique_ptr<AssignNode> varAssign = std::make_unique<AssignNode>();
    varAssign->location = lvalue->location;
    varAssign->lvalue = std::move(lvalue);
    varAssign->rvalue = parseExpr(tp, current, src);
    if (tp.pop().objType != endExpect) {
        throw std::runtime_error(std::format("E0608 invalid statement ending at {}", getLocString(varAssign->location))); // E0608
    }
    return varAssign;
}

// parse general statement
std::unique_ptr<ASTNode> ASTGen::parseStatement(TokenProvider& tp, ScopeNode& current, SrcFile& src) {
    bool isDefine = false;
    bool isExtern = false;
    bool isVaArg = false;
    while (tp.canPop(1)) {
        Token& tkn = tp.seek();
        switch (tkn.objType) {
            case TokenType::KEY_IF: // if statement
            {
                tp.pop();
                std::unique_ptr<IfNode> ifNode = std::make_unique<IfNode>();
                ifNode->location = tkn.location;
                if (tp.pop().objType != TokenType::OP_LPAREN) {
                    throw std::runtime_error(std::format("E0609 expected '(' at {}", getLocString(ifNode->location))); // E0609
                }
                ifNode->cond = parseExpr(tp, current, src);
                if (tp.pop().objType != TokenType::OP_RPAREN) {
                    throw std::runtime_error(std::format("E0610 expected ')' at {}", getLocString(ifNode->location))); // E0610
                }
                ifNode->ifBody = parseStatement(tp, current, src);
                if (tp.seek().objType == TokenType::KEY_ELSE) { // else statement
                    tp.pop();
                    ifNode->elseBody = parseStatement(tp, current, src);
                }
                return ifNode;
            }

            case TokenType::KEY_WHILE: // while statement
            {
                tp.pop();
                std::unique_ptr<WhileNode> whileNode = std::make_unique<WhileNode>();
                whileNode->location = tkn.location;
                if (tp.pop().objType != TokenType::OP_LPAREN) {
                    throw std::runtime_error(std::format("E0611 expected '(' at {}", getLocString(whileNode->location))); // E0611
                }
                whileNode->cond = parseExpr(tp, current, src);
                if (tp.pop().objType != TokenType::OP_RPAREN) {
                    throw std::runtime_error(std::format("E0612 expected ')' at {}", getLocString(whileNode->location))); // E0612
                }
                whileNode->body = parseStatement(tp, current, src);
                return whileNode;
            }

            case TokenType::KEY_FOR: // for statement
            {
                // init statement will be at another outside scope
                tp.pop();
                std::unique_ptr<ScopeNode> forScope = std::make_unique<ScopeNode>(current);
                forScope->location = tkn.location;
                std::unique_ptr<ForNode> forNode = std::make_unique<ForNode>();
                forNode->location = tkn.location;
                if (tp.pop().objType != TokenType::OP_LPAREN) {
                    throw std::runtime_error(std::format("E0613 expected '(' at {}", getLocString(forNode->location))); // E0613
                }
                std::unique_ptr<ASTNode> initNode = parseStatement(tp, current, src);
                if (initNode->objType != ASTNodeType::DECL_VAR && initNode->objType != ASTNodeType::ASSIGN && initNode->objType != ASTNodeType::EMPTY) {
                    throw std::runtime_error(std::format("E0614 invalid for_init statement at {}", getLocString(initNode->location))); // E0614
                }
                forScope->body.push_back(std::move(initNode));

                // parse cond and step
                if (tp.seek().objType == TokenType::OP_SEMICOLON) {
                    forNode->cond = std::make_unique<AtomicExprNode>(Literal((int64_t)1));
                } else {
                    forNode->cond = parseExpr(tp, *forScope, src);
                }
                if (tp.pop().objType != TokenType::OP_SEMICOLON) {
                    throw std::runtime_error(std::format("E0615 expected ';' at {}", getLocString(forNode->location))); // E0615
                }
                if (tp.seek().objType == TokenType::OP_RPAREN) {
                    tp.pop();
                } else {
                    std::unique_ptr<ASTNode> left = parseExpr(tp, *forScope, src);
                    Token& opTkn = tp.pop();
                    if (opTkn.objType == TokenType::OP_ASSIGN) {
                        left = parseVarAssign(tp, *forScope, src, std::move(left), TokenType::OP_RPAREN);
                    } else if (opTkn.objType != TokenType::OP_RPAREN) {
                        throw std::runtime_error(std::format("E0616 expected ')' at {}", getLocString(forNode->location))); // E0616
                    }
                    forNode->step = std::move(left);
                }

                // parse body
                forNode->body = parseStatement(tp, *forScope, src);
                forScope->body.push_back(std::move(forNode));
                return forScope;
            }

            case TokenType::KEY_SWITCH: // switch statement
            {
                // parse switch_expr, make switch node
                tp.pop();
                std::unique_ptr<SwitchNode> switchNode = std::make_unique<SwitchNode>();
                switchNode->location = tkn.location;
                if (tp.pop().objType != TokenType::OP_LPAREN) {
                    throw std::runtime_error(std::format("E0617 expected '(' at {}", getLocString(switchNode->location))); // E0617
                }
                switchNode->cond = parseExpr(tp, current, src);
                if (tp.pop().objType != TokenType::OP_RPAREN) {
                    throw std::runtime_error(std::format("E0618 expected ')' at {}", getLocString(switchNode->location))); // E0618
                }
                if (tp.pop().objType != TokenType::OP_LBRACE) {
                    throw std::runtime_error(std::format("E0619 expected '{' at {}", getLocString(switchNode->location))); // E0619
                }

                // parse case/default bodies
                bool defaultFound = false;
                bool pushCase = false;
                while (tp.canPop(1)) {
                    Token& caseTkn = tp.seek();
                    if (caseTkn.objType == TokenType::KEY_CASE) { // case statement
                        tp.pop();
                        pushCase = true;
                        if (defaultFound) {
                            throw std::runtime_error(std::format("E0620 case should be before default at {}", getLocString(caseTkn.location))); // E0620
                        }
                        std::unique_ptr<ASTNode> value = parseExpr(tp, current, src);
                        if (value->objType != ASTNodeType::LITERAL) {
                            throw std::runtime_error(std::format("E0621 case_expr must be int constexpr at {}", getLocString(value->location))); // E0621
                        }
                        Literal lit = static_cast<AtomicExprNode*>(value.get())->literal;
                        if (lit.objType != LiteralType::INT && lit.objType != LiteralType::CHAR) {
                            throw std::runtime_error(std::format("E0622 case_expr must be int constexpr at {}", getLocString(value->location))); // E0622
                        }
                        if (tp.pop().objType != TokenType::OP_COLON) {
                            throw std::runtime_error(std::format("E0623 expected ':' at {}", getLocString(caseTkn.location))); // E0623
                        }
                        for (auto& cond : switchNode->caseConds) {
                            if (cond == lit.intValue) {
                                throw std::runtime_error(std::format("E0624 case value {} already exists at {}", lit.intValue, getLocString(caseTkn.location))); // E0624
                            }
                        }
                        switchNode->caseConds.push_back(lit.intValue);
                        switchNode->caseBodies.push_back(std::vector<std::unique_ptr<ASTNode>>());

                    } else if (caseTkn.objType == TokenType::KEY_DEFAULT) { // default statement
                        tp.pop();
                        pushCase = false;
                        if (defaultFound) {
                            throw std::runtime_error(std::format("E0625 default already exists at {}", getLocString(caseTkn.location))); // E0625
                        }
                        defaultFound = true;
                        if (tp.pop().objType != TokenType::OP_COLON) {
                            throw std::runtime_error(std::format("E0626 expected ':' at {}", getLocString(caseTkn.location))); // E0626
                        }

                    } else if (caseTkn.objType == TokenType::OP_RBRACE) { // end
                        tp.pop();
                        break;

                    } else if (pushCase) { // push to case body
                        switchNode->caseBodies.back().push_back(parseStatement(tp, current, src));

                    } else if (!pushCase && defaultFound) { // push to default body
                        switchNode->defaultBody.push_back(parseStatement(tp, current, src));

                    } else { // statement before case
                        throw std::runtime_error(std::format("E0627 statement before case at {}", getLocString(caseTkn.location))); // E0627
                    }
                }
                return switchNode;
            }

            case TokenType::KEY_BREAK:
            {
                tp.pop();
                std::unique_ptr<ASTNode> result = std::make_unique<ShortStatNode>(ASTNodeType::BREAK);
                result->location = tkn.location;
                return result;
            }

            case TokenType::KEY_CONTINUE:
            {
                tp.pop();
                std::unique_ptr<ASTNode> result = std::make_unique<ShortStatNode>(ASTNodeType::CONTINUE);
                result->location = tkn.location;
                return result;
            }

            case TokenType::KEY_FALL:
            {
                tp.pop();
                std::unique_ptr<ASTNode> result = std::make_unique<ShortStatNode>(ASTNodeType::FALL);
                result->location = tkn.location;
                return result;
            }

            case TokenType::KEY_RETURN: // return statement
            {
                tp.pop();
                std::unique_ptr<ShortStatNode> result = std::make_unique<ShortStatNode>(ASTNodeType::RETURN);
                result->location = tkn.location;
                if (tp.seek().objType == TokenType::OP_SEMICOLON) {
                    result->statExpr = std::make_unique<ShortStatNode>(ASTNodeType::EMPTY);
                    result->statExpr->location = tkn.location;
                } else {
                    result->statExpr = parseExpr(tp, current, src);
                }
                if (tp.pop().objType != TokenType::OP_SEMICOLON) {
                    throw std::runtime_error(std::format("E0628 expected ';' at {}", getLocString(result->location))); // E0628
                }
                return result;
            }

            case TokenType::ORDER_DEFER: // defer statement
            {
                tp.pop();
                std::unique_ptr<ShortStatNode> result = std::make_unique<ShortStatNode>(ASTNodeType::DEFER);
                result->location = tkn.location;
                result->statExpr = parseExpr(tp, current, src);
                if (tp.pop().objType != TokenType::OP_SEMICOLON) {
                    throw std::runtime_error(std::format("E0629 expected ';' at {}", getLocString(result->location))); // E0629
                }
                return result;
            }

            case TokenType::OP_LBRACE: // scope
                return parseScope(tp, current, src);

            case TokenType::OP_SEMICOLON: // empty statement
            {
                tp.pop();
                std::unique_ptr<ShortStatNode> result = std::make_unique<ShortStatNode>(ASTNodeType::EMPTY);
                result->location = tkn.location;
                return result;
            }

            case TokenType::ORDER_DEFINE:
                if (isDefine) {
                    throw std::runtime_error(std::format("E0630 duplicated define at {}", getLocString(tkn.location))); // E0630
                }
                isDefine = true;
                tp.pop();
                break;

            case TokenType::ORDER_EXTERN:
                if (isExtern) {
                    throw std::runtime_error(std::format("E0631 duplicated extern at {}", getLocString(tkn.location))); // E0631
                }
                isExtern = true;
                tp.pop();
                break;

            case TokenType::ORDER_RAW_C: case TokenType::ORDER_RAW_IR:
                return parseRawCode(tp);

            default:
                if (isTypeStart(tp, src)) { // var declaration
                    return parseVarDecl(tp, current, src, src.parseType(tp, current, arch), isDefine, isExtern, false);
                } else {
                    std::unique_ptr<ASTNode> left = parseExpr(tp, current, src);
                    Token& opTkn = tp.pop();
                    if (opTkn.objType == TokenType::OP_ASSIGN) { // assign statement
                        return parseVarAssign(tp, current, src, std::move(left), TokenType::OP_SEMICOLON);
                    } else if (opTkn.objType == TokenType::OP_SEMICOLON) { // expression statement
                        return left;
                    } else {
                        throw std::runtime_error(std::format("E0632 expected ';' at {}", getLocString(opTkn.location))); // E0632
                    }
                }
        }
    }
    throw std::runtime_error(std::format("E0633 unexpected EOF while parsing statement at {}", getLocString(current.location))); // E0633
}

// parse scope
std::unique_ptr<ScopeNode> ASTGen::parseScope(TokenProvider& tp, ScopeNode& current, SrcFile& src) {
    if (tp.pop().objType != TokenType::OP_LBRACE) {
        throw std::runtime_error(std::format("E0634 expected '{{' at {}", getLocString(current.location))); // E0634
    }
    std::unique_ptr<ScopeNode> scope = std::make_unique<ScopeNode>();
    scope->location = current.location;
    scope->parent = &current;
    while (tp.canPop(1)) {
        Token& tkn = tp.seek();
        if (tkn.objType == TokenType::OP_RBRACE) {
            tp.pop();
            break;
        }
        scope->body.push_back(parseStatement(tp, *scope, src));
    }
    return scope;
}

// parse toplevel declaration
std::unique_ptr<ASTNode> ASTGen::parseTopLevel(TokenProvider& tp, ScopeNode& current, SrcFile& src) {
    bool isDefine = false;
    bool isExtern = false;
    bool isExported = false;
    bool isVaArg = false;
    while (tp.canPop(1)) {
        Token& tkn = tp.seek();
        switch (tkn.objType) {
            case TokenType::ORDER_INCLUDE:
            {
                tp.pop();
                std::unique_ptr<IncludeNode> result = std::make_unique<IncludeNode>();
                result->location = tkn.location;
                if (tp.seek().objType == TokenType::OP_LT) { // template arguments
                    while (tp.canPop(1)) {
                        result->args.push_back(src.parseType(tp, current, arch));
                        Token& opTkn = tp.seek();
                        if (opTkn.objType == TokenType::OP_COMMA) {
                            tp.pop();
                        } else if (opTkn.objType == TokenType::OP_GT) {
                            tp.pop();
                            break;
                        } else {
                            throw std::runtime_error(std::format("E0635 expected '>' at {}", getLocString(opTkn.location))); // E0635
                        }
                    }
                }
                if (tp.match({TokenType::LIT_STRING, TokenType::IDENTIFIER})) {
                    result->path = tp.pop().text;
                    result->name = tp.pop().text;
                } else {
                    throw std::runtime_error(std::format("E0636 expected module filepath at {}", getLocString(tkn.location))); // E0636
                }
                std::string nmValidity = src.isNameUsable(result->name, tkn.location);
                if (!nmValidity.empty()) {
                    throw std::runtime_error(nmValidity);
                }
                return result;
            }

            case TokenType::ORDER_TEMPLATE:
            {
                tp.pop();
                std::unique_ptr<DeclTemplateNode> result = std::make_unique<DeclTemplateNode>();
                result->location = tkn.location;
                Token& tmpTkn = tp.pop();
                if (tmpTkn.objType != TokenType::IDENTIFIER) {
                    throw std::runtime_error(std::format("E0637 expected typename at {}", getLocString(tmpTkn.location))); // E0637
                }
                std::string nmValidity = src.isNameUsable(tmpTkn.text, tmpTkn.location);
                if (!nmValidity.empty()) {
                    throw std::runtime_error(nmValidity);
                }
                result->name = tmpTkn.text;
                return result;
            }

            case TokenType::ORDER_RAW_C: case TokenType::ORDER_RAW_IR:
                return parseRawCode(tp);

            case TokenType::OP_SEMICOLON:
            {
                tp.pop();
                std::unique_ptr<ShortStatNode> result = std::make_unique<ShortStatNode>(ASTNodeType::EMPTY);
                result->location = tkn.location;
                return result;
            }

            case TokenType::ORDER_DEFINE:
                if (isDefine) {
                    throw std::runtime_error(std::format("E0638 duplicate define at {}", getLocString(tkn.location))); // E0638
                }
                tp.pop();
                isDefine = true;
                break;

            case TokenType::ORDER_EXTERN:
                if (isExtern) {
                    throw std::runtime_error(std::format("E0639 duplicate extern at {}", getLocString(tkn.location))); // E0639
                }
                tp.pop();
                isExtern = true;
                break;

            case TokenType::ORDER_EXPORT:
                if (isExported) {
                    throw std::runtime_error(std::format("E0640 duplicate export at {}", getLocString(tkn.location))); // E0640
                }
                tp.pop();
                isExported = true;
                break;

            case TokenType::ORDER_VA_ARG:
                if (isVaArg) {
                    throw std::runtime_error(std::format("E0641 duplicate va_arg at {}", getLocString(tkn.location))); // E0641
                }
                tp.pop();
                isVaArg = true;
                break;

            case TokenType::KEY_STRUCT:
                tp.pop();
                return parseStruct(tp, current, src, isExported);

            case TokenType::KEY_ENUM:
                tp.pop();
                return parseEnum(tp, current, src, isExported);

            default:
            {
                std::unique_ptr<TypeNode> vtype = src.parseType(tp, current, arch);
                if (tp.match({TokenType::IDENTIFIER, TokenType::OP_SEMICOLON}) || tp.match({TokenType::IDENTIFIER, TokenType::OP_ASSIGN})) { // var declaration
                    std::unique_ptr<DeclVarNode> varDecl = parseVarDecl(tp, current, src, std::move(vtype), isDefine, isExtern, isExported);
                    if (varDecl->varExpr != nullptr && varDecl->varExpr->objType != ASTNodeType::LITERAL && varDecl->varExpr->objType != ASTNodeType::LITERAL_KEY) {
                        throw std::runtime_error(std::format("E0642 variable should be initialized with constexpr at {}", getLocString(vtype->location))); // E0642
                    }
                    std::string nmValidity = src.isNameUsable(varDecl->name, varDecl->location);
                    if (!nmValidity.empty()) {
                        throw std::runtime_error(nmValidity);
                    }
                    return varDecl;
                } else { // function declaration
                    return parseFunc(tp, current, src, std::move(vtype), isVaArg, isExported);
                }
            }
        }
    }
    throw std::runtime_error(std::format("E0643 unexpected EOF while parsing toplevel at {}", getLocString(current.location))); // E0643
}

// calculate type size, return true if modified
bool ASTGen::completeType(SrcFile& src, TypeNode& tgt) {
    bool modified = false;
    if (tgt.direct != nullptr) {
        modified = modified | completeType(src, *tgt.direct);
    }
    for (auto& indirect : tgt.indirect) {
        modified = modified | completeType(src, *indirect);
    }
    if (tgt.typeSize != -1) {
        return modified;
    }
    switch (tgt.subType) {
        case TypeNodeType::ARRAY:
            if (tgt.direct->typeSize == 0) {
                throw std::runtime_error(std::format("E0701 cannot create array/slice of void type at {}", getLocString(tgt.location))); // E0701
            }
            if (tgt.direct->typeSize != -1) {
                tgt.typeSize = tgt.direct->typeSize * tgt.length;
                tgt.typeAlign = tgt.direct->typeAlign;
                modified = true;
            }
            break;

        case TypeNodeType::NAME:
            {
                DeclStructNode* structNode = static_cast<DeclStructNode*>(src.findNodeByName(ASTNodeType::DECL_STRUCT, tgt.name, false));
                if (structNode != nullptr && structNode->structSize != -1) {
                    tgt.typeSize = structNode->structSize;
                    tgt.typeAlign = structNode->structAlign;
                    modified = true;
                }
                DeclEnumNode* enumNode = static_cast<DeclEnumNode*>(src.findNodeByName(ASTNodeType::DECL_ENUM, tgt.name, false));
                if (enumNode != nullptr) {
                    tgt.typeSize = enumNode->enumSize;
                    tgt.typeAlign = enumNode->enumSize;
                    modified = true;
                }
                DeclTemplateNode* templateNode = static_cast<DeclTemplateNode*>(src.findNodeByName(ASTNodeType::DECL_TEMPLATE, tgt.name, false));
                if (templateNode != nullptr && templateNode->tmpSize != -1) {
                    tgt.typeSize = templateNode->tmpSize;
                    tgt.typeAlign = templateNode->tmpAlign;
                    modified = true;
                }
                if (structNode == nullptr && enumNode == nullptr && templateNode == nullptr) {
                    throw std::runtime_error(std::format("E0702 type {} not found at {}", tgt.name, getLocString(tgt.location))); // E0702
                }
            }
            break;

        case TypeNodeType::FOREIGN:
            {
                IncludeNode* includeNode = static_cast<IncludeNode*>(src.findNodeByName(ASTNodeType::INCLUDE, tgt.includeName, false));
                if (includeNode == nullptr) {
                    throw std::runtime_error(std::format("E0703 include name {} not found at {}", tgt.name, getLocString(tgt.location))); // E0703
                }
                int index = findSource(includeNode->path);
                if (index == -1) {
                    throw std::runtime_error(std::format("E0704 included module {} not found at {}", includeNode->path, getLocString(tgt.location))); // E0704
                }
                DeclStructNode* structNode = static_cast<DeclStructNode*>(srcFiles[index]->findNodeByName(ASTNodeType::DECL_STRUCT, tgt.name, true));
                if (structNode != nullptr && structNode->structSize != -1) {
                    tgt.typeSize = structNode->structSize;
                    tgt.typeAlign = structNode->structAlign;
                    modified = true;
                }
                DeclEnumNode* enumNode = static_cast<DeclEnumNode*>(srcFiles[index]->findNodeByName(ASTNodeType::DECL_ENUM, tgt.name, true));
                if (enumNode != nullptr) {
                    tgt.typeSize = enumNode->enumSize;
                    tgt.typeAlign = enumNode->enumSize;
                    modified = true;
                }
                if (structNode == nullptr && enumNode == nullptr) {
                    throw std::runtime_error(std::format("E0705 type {}.{} not found at {}", tgt.includeName, tgt.name, getLocString(tgt.location))); // E0705
                }
            }
            break;
    }
    return modified;
}

// complete struct size, return true if modified
bool ASTGen::completeStruct(SrcFile& src, DeclStructNode& tgt) {
    bool isModified = false;
    for (auto& mem : tgt.memTypes) {
        isModified = isModified | completeType(src, *mem);
    }
    for (auto& mem : tgt.memTypes) {
        if (mem->typeSize == -1) {
            return isModified;
        }
    }
    tgt.structSize = 0;
    tgt.structAlign = 1;
    for (size_t i = 0; i < tgt.memTypes.size(); i++) {
        if (tgt.structSize % tgt.memTypes[i]->typeAlign != 0) {
            tgt.structSize += tgt.memTypes[i]->typeAlign - tgt.structSize % tgt.memTypes[i]->typeAlign;
        }
        tgt.memOffsets[i] =  tgt.structSize;
        tgt.structSize += tgt.memTypes[i]->typeSize;
        tgt.structAlign = std::max(tgt.structAlign, tgt.memTypes[i]->typeAlign);
    }
    if (tgt.structSize % tgt.structAlign != 0) {
        tgt.structSize += tgt.structAlign - tgt.structSize % tgt.structAlign;
    }
    return true;
}

// jump variable or function declaration
void jumpDecl(TokenProvider& tp, ScopeNode& current, SrcFile& src) {
    src.parseType(tp, current, 1);
    if (tp.match({TokenType::IDENTIFIER, TokenType::OP_SEMICOLON}) || tp.match({TokenType::IDENTIFIER, TokenType::OP_ASSIGN})) { // variable
        while (tp.canPop(1)) {
            if (tp.pop().objType == TokenType::OP_SEMICOLON) {
                break;
            }
        }
    } else { // function
        int count = 0;
        while (tp.canPop(1)) {
            if (tp.pop().objType == TokenType::OP_LBRACE) {
                count++;
                break;
            }
        }
        while (tp.canPop(1)) {
            Token& tkn = tp.pop();
            if (tkn.objType == TokenType::OP_LBRACE) {
                count++;
            } else if (tkn.objType == TokenType::OP_RBRACE) {
                count--;
                if (count == 0) {
                    break;
                }
            }
        }
    }
}

// final parser, returns error message or empty if ok
std::string ASTGen::parse(const std::string& path) {
    // check if already parsed
    int index = findSource(path);
    if (index != -1) {
        return std::format("E0706 source {} already parsed", path); // E0706
    }

    try {
        // find unique name
        std::string name = getFileName(path);
        std::string dir = getWorkingDir(path);
        int count = 0;
        bool isIncluded = true;
        while (isIncluded) {
            isIncluded = false;
            std::string uname;
            if (count == 0) {
                uname = name;
            } else {
                uname = std::format("{}_{}", name, count);
            }
            for (auto& src : srcFiles) {
                if (src->uniqueName == uname) {
                    isIncluded = true;
                    break;
                }
            }
            if (isIncluded) {
                count++;
            } else {
                srcFiles.push_back(std::make_unique<SrcFile>(path, uname));
                index = static_cast<int>(srcFiles.size() - 1);
            }
        }

        // tokenize source
        std::string text = readFile(path);
        std::unique_ptr<std::vector<Token>> tokens = tokenize(text, path, index);
        TokenProvider tp = TokenProvider(*tokens);

        // source parsing pass1, parse structs and enums
        std::vector<int> reserved;
        while (tp.canPop(1)) {
            Token& tkn = tp.seek();
            switch (tkn.objType) {
                case TokenType::ORDER_INCLUDE: // include source, detect import cycle
                {
                    std::unique_ptr<ASTNode> node = parseTopLevel(tp, *srcFiles[index]->code, *srcFiles[index]);
                    if (node->objType != ASTNodeType::INCLUDE) {
                        return std::format("E0707 invalid include statement at {}", getLocString(tkn.location)); // E0707
                    }
                    IncludeNode* inc = static_cast<IncludeNode*>(node.get());
                    int idx = findSource(inc->path);
                    if (idx == -1) { // not found, parse and add
                        std::string emsg = parse(inc->path);
                        if (!emsg.empty()) {
                            return emsg;
                        }
                    } else if (!srcFiles[idx]->isFinished) { // import cycle, return error message
                        return std::format("E0708 import cycle detected with {} at {}", inc->path, getLocString(tkn.location)); // E0708
                    } // else: already parsed, skip
                }
                break;

                case TokenType::ORDER_TEMPLATE: case TokenType::ORDER_RAW_C: case TokenType::ORDER_RAW_IR: case TokenType::ORDER_DEFINE: // need to be parsed for deciding types
                    srcFiles[index]->code->body.push_back(parseTopLevel(tp, *srcFiles[index]->code, *srcFiles[index]));
                    break;

                case TokenType::KEY_STRUCT: case TokenType::KEY_ENUM: case TokenType::OP_SEMICOLON: // definitions of structs and enums are parsed now
                    srcFiles[index]->code->body.push_back(parseTopLevel(tp, *srcFiles[index]->code, *srcFiles[index]));
                    break;

                case TokenType::ORDER_EXPORT:
                    if (tp.match({TokenType::ORDER_EXPORT, TokenType::KEY_STRUCT}) || tp.match({TokenType::ORDER_EXPORT, TokenType::KEY_ENUM})) {
                        srcFiles[index]->code->body.push_back(parseTopLevel(tp, *srcFiles[index]->code, *srcFiles[index]));
                    } else {
                        reserved.push_back(tp.pos);
                        tp.pop();
                    }
                    break;

                case TokenType::ORDER_EXTERN: case TokenType::ORDER_VA_ARG: // order before variable or function declaration
                    reserved.push_back(tp.pos);
                    tp.pop();
                    break;

                default: // declarations of functions and variables are parsed later
                    reserved.push_back(tp.pos);
                    jumpDecl(tp, *srcFiles[index]->code, *srcFiles[index]);
                    break;
            }
        }

        // source parsing pass2, static calculation of struct size
        bool isModified = true;
        while (isModified) {
            isModified = false;
            for (auto& node : srcFiles[index]->code->body) {
                if (node->objType == ASTNodeType::DECL_STRUCT) {
                    DeclStructNode* decl = static_cast<DeclStructNode*>(node.get());
                    isModified = isModified | completeStruct(*srcFiles[index], *decl);
                }
            }
        }

        // source parsing pass3, parse functions and variables
        tp.pos = 0;
        for (int i : reserved) {
            if (i >= tp.pos) { // parse only if not already parsed
                tp.pos = i;
                srcFiles[index]->code->body.push_back(parseTopLevel(tp, *srcFiles[index]->code, *srcFiles[index]));
            }
        }

        // finish parsing
        srcFiles[index]->isFinished = true;
    } catch (std::exception& e) {
        return e.what();
    }
    return "";
}