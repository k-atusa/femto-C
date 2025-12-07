#include <algorithm>
#include <cstdint>
#include "baseFunc.h"
#include "ast1.h"

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
                return 11;
            case TokenType::OP_PLUS: case TokenType::OP_MINUS:
                return 10;
            case TokenType::OP_BIT_LSHIFT: case TokenType::OP_BIT_RSHIFT:
                return 9;
            case TokenType::OP_LT: case TokenType::OP_LT_EQ: case TokenType::OP_GT: case TokenType::OP_GT_EQ:
                return 8;
            case TokenType::OP_EQ: case TokenType::OP_NOT_EQ:
                return 7;
            case TokenType::OP_BIT_AND:
                return 6;
            case TokenType::OP_BIT_XOR:
                return 5;
            case TokenType::OP_BIT_OR:
                return 4;
            case TokenType::OP_LOGIC_AND:
                return 3;
            case TokenType::OP_LOGIC_OR:
                return 2;
            case TokenType::OP_QMARK:
                return 1;
            default:
                return -1;
        }
    }
}

// get pratt operator type
A1ExprOpType getBinaryOpType(TokenType tknType) {
    switch (tknType) {
        case TokenType::OP_MUL:
            return A1ExprOpType::B_MUL;
        case TokenType::OP_DIV:
            return A1ExprOpType::B_DIV;
        case TokenType::OP_REMAIN:
            return A1ExprOpType::B_MOD;
        case TokenType::OP_PLUS:
            return A1ExprOpType::B_ADD;
        case TokenType::OP_MINUS:
            return A1ExprOpType::B_SUB;
        case TokenType::OP_BIT_LSHIFT:
            return A1ExprOpType::B_SHL;
        case TokenType::OP_BIT_RSHIFT:
            return A1ExprOpType::B_SHR;
        case TokenType::OP_LT:
            return A1ExprOpType::B_LT;
        case TokenType::OP_GT:
            return A1ExprOpType::B_GT;
        case TokenType::OP_LT_EQ:
            return A1ExprOpType::B_LE;
        case TokenType::OP_GT_EQ:
            return A1ExprOpType::B_GE;
        case TokenType::OP_EQ:
            return A1ExprOpType::B_EQ;
        case TokenType::OP_NOT_EQ:
            return A1ExprOpType::B_NE;
        case TokenType::OP_BIT_AND:
            return A1ExprOpType::B_BIT_AND;
        case TokenType::OP_BIT_XOR:
            return A1ExprOpType::B_BIT_XOR;
        case TokenType::OP_BIT_OR:
            return A1ExprOpType::B_BIT_OR;
        case TokenType::OP_LOGIC_AND:
            return A1ExprOpType::B_LOGIC_AND;
        case TokenType::OP_LOGIC_OR:
            return A1ExprOpType::B_LOGIC_OR;
        case TokenType::OP_QMARK:
            return A1ExprOpType::T_COND;
        default:
            return A1ExprOpType::NONE;
    }
}

// get operand number
int getOperandNum(A1ExprOpType op) {
    switch (op) {
        case A1ExprOpType::NONE:
            return 0;
        case A1ExprOpType::T_SLICE: case A1ExprOpType::T_COND:
            return 3;
        case A1ExprOpType::U_PLUS: case A1ExprOpType::U_MINUS: case A1ExprOpType::U_LOGIC_NOT: case A1ExprOpType::U_BIT_NOT:
        case A1ExprOpType::U_REF: case A1ExprOpType::U_DEREF: case A1ExprOpType::U_SIZEOF: case A1ExprOpType::U_LEN:
            return 1;
        default:
            return 2;
    }
}

// get assign type
A1StatAssignType getAssignType(Token& tkn) {
    switch (tkn.objType) {
        case TokenType::OP_ASSIGN:
            return A1StatAssignType::ASSIGN;
        case TokenType::OP_ASSIGN_ADD:
            return A1StatAssignType::ASSIGN_ADD;
        case TokenType::OP_ASSIGN_SUB:
            return A1StatAssignType::ASSIGN_SUB;
        case TokenType::OP_ASSIGN_MUL:
            return A1StatAssignType::ASSIGN_MUL;
        case TokenType::OP_ASSIGN_DIV:
            return A1StatAssignType::ASSIGN_DIV;
        case TokenType::OP_ASSIGN_REMAIN:
            return A1StatAssignType::ASSIGN_REMAIN;
        default:
            return A1StatAssignType::NONE;
    }
}

// A1StatScope functions
A1Decl* A1StatScope::findDeclaration(const std::string& name) {
    for (auto& node : body) {
        if (node->objType == A1StatType::DECL) {
            A1Decl* d = static_cast<A1StatDecl*>(node.get())->decl.get();
            if (d->name == name) return d;
        }
    }
    if (parent) { // find in parent
        return parent->findDeclaration(name);
    }
    return nullptr;
}

Literal A1StatScope::findLiteral(const std::string& name) {
    A1Decl* dNode = findDeclaration(name);
    if (dNode == nullptr || dNode->objType != A1DeclType::VAR) {
        return Literal();
    }
    A1DeclVar* vNode = static_cast<A1DeclVar*>(dNode);
    if (!vNode->isDefine || vNode->init == nullptr || vNode->init->objType != A1ExprType::LITERAL) {
        return Literal();
    }
    A1ExprLiteral* lNode = static_cast<A1ExprLiteral*>(vNode->init.get());
    return lNode->value;
}

// A1Module functions
A1Decl* A1Module::findDeclaration(const std::string& name, bool checkExported) {
    A1Decl* dNode = code->findDeclaration(name);
    if (dNode == nullptr || !checkExported) {
        return dNode;
    }
    switch (dNode->objType) {
        case A1DeclType::INCLUDE: case A1DeclType::TEMPLATE: case A1DeclType::TYPEDEF:
            return nullptr; // include, template, typedef are not exported
        case A1DeclType::VAR: case A1DeclType::STRUCT: case A1DeclType::ENUM:
            if ('A' <= dNode->name[0] && dNode->name[0] <= 'Z') {
                return dNode;
            } else {
                return nullptr;
            }
        case A1DeclType::FUNC:
            {
                A1DeclFunc* fNode = static_cast<A1DeclFunc*>(dNode);
                if (fNode->structNm.empty()) { // global function
                    if ('A' <= fNode->funcNm[0] && fNode->funcNm[0] <= 'Z') {
                        return fNode;
                    } else {
                        return nullptr;
                    }
                } else { // method
                    if ('A' <= fNode->structNm[0] && fNode->structNm[0] <= 'Z' && 'A' <= fNode->funcNm[0] && fNode->funcNm[0] <= 'Z') {
                        return fNode;
                    } else {
                        return nullptr;
                    }
                }
            }
    }
    return nullptr; // others are not exported
}

A1Decl* A1Module::findDeclaration(const std::string& name, A1DeclType type, bool checkExported) {
    A1Decl* dNode = findDeclaration(name, checkExported);
    if (dNode == nullptr || dNode->objType != type) {
        return nullptr;
    }
    return dNode;
}

Literal A1Module::findLiteral(const std::string& name, bool checkExported) {
    if (name.contains('.')) { // enum member
        size_t pos = name.find('.');
        std::string enumName = name.substr(0, pos);
        std::string memberName = name.substr(pos + 1);
        if (checkExported && (enumName[0] < 'A' || 'Z' < enumName[0] || memberName[0] < 'A' || 'Z' < memberName[0])) {
            return Literal();
        }
        A1DeclEnum* eNode = static_cast<A1DeclEnum*>(findDeclaration(enumName, A1DeclType::ENUM, checkExported));
        if (eNode == nullptr) {
            return Literal();
        }
        for (size_t i = 0; i < eNode->memNames.size(); i++) {
            if (eNode->memNames[i] == memberName) {
                return Literal(eNode->memValues[i]);
            }
        }
        return Literal();
    } else { // defined literal
        if (checkExported && (name[0] < 'A' || 'Z' < name[0])) {
            return Literal();
        }
        return code->findLiteral(name);
    }
}

std::string A1Module::isNameUsable(const std::string& name, Location loc) {
    // toplevel names used: include, template, var, func, struct, enum
    if (findDeclaration(name, false) != nullptr) {
        return std::format("E0201 global name {} already used at {}:{}", name, path, loc.line); // E0201
    }
    return "";
}

std::unique_ptr<A1Type> A1Module::parseType(TokenProvider& tp, A1StatScope& current, int arch) {
    // parse base type
    std::unique_ptr<A1Type> result;
    if (tp.match({TokenType::IDENTIFIER, TokenType::OP_DOT, TokenType::IDENTIFIER})) { // foreign type
        Token& includeTkn = tp.pop();
        tp.pop();
        Token& nameTkn = tp.pop();
        if (findDeclaration(includeTkn.text, A1DeclType::INCLUDE, false) == nullptr) {
            throw std::runtime_error(std::format("E0202 include name {} not found at {}:{}", includeTkn.text, path, includeTkn.location.line)); // E0202
        }
        result = std::make_unique<A1Type>(includeTkn.text, nameTkn.text);
        result->location = includeTkn.location;

    } else if (tp.match({TokenType::IDENTIFIER})) { // typedef, template, struct, enum
        Token& nameTkn = tp.pop();
        A1Decl* dNode = current.findDeclaration(nameTkn.text);
        if (dNode == nullptr || dNode->objType != A1DeclType::TYPEDEF) { // template, struct, enum
            result = std::make_unique<A1Type>(dNode->name, nameTkn.text);
        } else { // typedef, replace to original
            result = dNode->type->Clone();
        }
        result->location = nameTkn.location;

    } else if (tp.canPop(1)) { // primitive
        Token& baseTkn = tp.pop();
        if (baseTkn.objType == TokenType::KEY_AUTO) { // auto
            result = std::make_unique<A1Type>(A1TypeType::AUTO, baseTkn.text);
            result->location = baseTkn.location;
            return result;
        }

        result = std::make_unique<A1Type>(A1TypeType::PRIMITIVE, baseTkn.text);
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
            case TokenType::KEY_INT: case TokenType::KEY_UINT:
                result->typeSize = arch;
                result->typeAlign = arch;
                break;
            case TokenType::KEY_BOOL:
                result->typeSize = 1;
                result->typeAlign = 1;
                break;
            case TokenType::KEY_VOID:
                result->typeSize = 0;
                result->typeAlign = 1;
                break;
            default:
                throw std::runtime_error(std::format("E0203 invalid type start {} at {}:{}", baseTkn.text, path, baseTkn.location.line)); // E0203
        }

    } else {
        throw std::runtime_error("E0204 unexpected EOF while parsing type"); // E0204
    }

    // parse type modifiers
    while (tp.canPop(1)) {
        Token& tkn = tp.pop();
        switch (tkn.objType) {
            case TokenType::OP_MUL: // pointer
                {
                    std::unique_ptr<A1Type> ptrType = std::make_unique<A1Type>(A1TypeType::POINTER, "*");
                    ptrType->location = result->location;
                    ptrType->typeSize = arch;
                    ptrType->typeAlign = arch;
                    ptrType->direct = std::move(result);
                    result = std::move(ptrType);
                }
                break;

            case TokenType::OP_LBRACKET:
                if (result->typeSize == 0) {
                    throw std::runtime_error(std::format("E0205 cannot create array/slice of void type at {}:{}", path, tkn.location.line)); // E0205
                }
                if (tp.match({TokenType::OP_RBRACKET})) { // slice
                    tp.pop();
                    std::unique_ptr<A1Type> sliceType = std::make_unique<A1Type>(A1TypeType::SLICE, "[]");
                    sliceType->location = result->location;
                    sliceType->typeSize = arch * 2; // ptr + length
                    sliceType->typeAlign = arch;
                    if (result->objType == A1TypeType::ARRAY || result->objType == A1TypeType::SLICE) { // nested array
                        A1Type* curr = result.get();
                        while (curr->direct && (curr->direct->objType == A1TypeType::ARRAY || curr->direct->objType == A1TypeType::SLICE)) {
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
                    int64_t len = std::get<int64_t>(lenTkn.value.value);
                    if (len <= 0) {
                        throw std::runtime_error(std::format("E0206 invalid array length {} at {}:{}", len, path, lenTkn.location.line)); // E0206
                    }
                    tp.pop();
                    std::unique_ptr<A1Type> arrType = std::make_unique<A1Type>(A1TypeType::ARRAY, std::format("[{}]", len));
                    arrType->location = result->location;
                    arrType->arrLen = len;
                    if (result->objType == A1TypeType::ARRAY || result->objType == A1TypeType::SLICE) { // nested array
                        A1Type* curr = result.get();
                        while (curr->direct && (curr->direct->objType == A1TypeType::ARRAY || curr->direct->objType == A1TypeType::SLICE)) {
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
                    Literal lenLit = current.findLiteral(lenTkn.text);
                    if (lenLit.objType == LiteralType::NONE) {
                        throw std::runtime_error(std::format("E0207 name {} not found at {}:{}", lenTkn.text, path, lenTkn.location.line)); // E0207
                    }
                    if (lenLit.objType != LiteralType::INT || std::get<int64_t>(lenLit.value) <= 0) {
                        throw std::runtime_error(std::format("E0208 name {} cannot be used as array length at {}:{}", lenTkn.text, path, lenTkn.location.line)); // E0208
                    }
                    int64_t len = std::get<int64_t>(lenLit.value);
                    tp.pop();
                    std::unique_ptr<A1Type> arrType = std::make_unique<A1Type>(A1TypeType::ARRAY, std::format("[{}]", len));
                    arrType->location = result->location;
                    arrType->arrLen = len;
                    if (result->objType == A1TypeType::ARRAY || result->objType == A1TypeType::SLICE) { // nested array
                        A1Type* curr = result.get();
                        while (curr->direct && (curr->direct->objType == A1TypeType::ARRAY || curr->direct->objType == A1TypeType::SLICE)) {
                            curr = curr->direct.get();
                        }
                        arrType->direct = std::move(curr->direct);
                        curr->direct = std::move(arrType);
                    } else {
                        arrType->direct = std::move(result);
                        result = std::move(arrType);
                    }

                } else {
                    throw std::runtime_error(std::format("E0209 expected ']' at {}:{}", path, tkn.location.line)); // E0209
                }
                break;

            case TokenType::OP_LPAREN: // function
                {
                    std::unique_ptr<A1Type> funcType = std::make_unique<A1Type>(A1TypeType::FUNCTION, "()");
                    funcType->location = result->location;
                    funcType->typeSize = arch;
                    funcType->typeAlign = arch;
                    funcType->direct = std::move(result);
                    result = std::move(funcType);
                    if (tp.seek().objType != TokenType::OP_RPAREN) {
                        while (tp.canPop(1)) {
                            std::unique_ptr<A1Type> argType = parseType(tp, current, arch);
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
int A1Gen::findModule(const std::string& path) {
    for (size_t i = 0; i < modules.size(); i++) {
        if (modules[i]->path == path) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool A1Gen::isTypeStart(TokenProvider& tp, A1Module& mod) {
    if (isPrimitive(tp.seek().objType)) { // primitive type
        return true;
    }

    if (tp.match({TokenType::IDENTIFIER, TokenType::OP_DOT, TokenType::IDENTIFIER})) { // foreign type
        Token& includeTkn = tp.pop();
        tp.pop();
        Token& nameTkn = tp.pop();
        Token& nextTkn = tp.pop();
        tp.pos -= 4;
        A1DeclInclude* includeNode = static_cast<A1DeclInclude*>(mod.findDeclaration(includeTkn.text, A1DeclType::INCLUDE, false));
        if (includeNode == nullptr) {
            return false;
        }
        int index = findModule(includeNode->tgtPath);
        if (index == -1) {
            throw std::runtime_error(std::format("E0301 included module {} not found at {}", includeNode->tgtPath, getLocString(includeTkn.location))); // E0301
        }
        if (modules[index]->findDeclaration(nameTkn.text, A1DeclType::STRUCT, true) != nullptr && nextTkn.objType != TokenType::OP_DOT) {
            return true; // struct.member is not a type
        }
        if (modules[index]->findDeclaration(nameTkn.text, A1DeclType::ENUM, true) != nullptr && nextTkn.objType != TokenType::OP_DOT) {
            return true; // enum.member is not a type
        }

    } else if (tp.match({TokenType::IDENTIFIER})) { // tmp, struct, enum
        Token& nameTkn = tp.pop();
        Token& nextTkn = tp.pop();
        tp.pos -= 2;
        if (mod.findDeclaration(nameTkn.text, A1DeclType::TEMPLATE, false) != nullptr) {
            return true; // template
        }
        if (mod.findDeclaration(nameTkn.text, A1DeclType::STRUCT, false) != nullptr && nextTkn.objType != TokenType::OP_DOT) {
            return true; // struct.member is not a type
        }
        if (mod.findDeclaration(nameTkn.text, A1DeclType::ENUM, false) != nullptr && nextTkn.objType != TokenType::OP_DOT) {
            return true; // enum.member is not a type
        }
    }
    return false;
}

// fold node to literal, NONE if not foldable
Literal A1Gen::foldNode(A1Expr& tgt, A1StatScope& current, A1Module& mod) {
    if (tgt.objType == A1ExprType::LITERAL) { // literal
        return static_cast<A1ExprLiteral*>(&tgt)->value;
    } else if (tgt.objType == A1ExprType::NAME) { // check if defined literal
        return current.findLiteral(static_cast<A1ExprName*>(&tgt)->name);
    } else if (tgt.objType == A1ExprType::OPERATION) { // fold operation
        A1ExprOperation* opNode = static_cast<A1ExprOperation*>(&tgt);

        // fold operands first
        Literal folded0 = Literal();
        Literal folded1 = Literal();
        Literal folded2 = Literal();
        if (opNode->operand0) {
            folded0 = foldNode(*opNode->operand0, current, mod);
            if (folded0.objType != LiteralType::NONE) {
                Location loc = opNode->operand0->location;
                opNode->operand0 = std::make_unique<A1ExprLiteral>(folded0);
                opNode->operand0->location = loc;
            }
        }
        if (opNode->operand1) {
            folded1 = foldNode(*opNode->operand1, current, mod);
            if (folded1.objType != LiteralType::NONE) {
                Location loc = opNode->operand1->location;
                opNode->operand1 = std::make_unique<A1ExprLiteral>(folded1);
                opNode->operand1->location = loc;
            }
        }
        if (opNode->operand2) {
            folded2 = foldNode(*opNode->operand2, current, mod);
            if (folded2.objType != LiteralType::NONE) {
                Location loc = opNode->operand2->location;
                opNode->operand2 = std::make_unique<A1ExprLiteral>(folded2);
                opNode->operand2->location = loc;
            }
        }

        int opnum = getOperandNum(opNode->subType);
        if (opnum == 1) { // try to fold unary
            switch (opNode->subType) {
                case A1ExprOpType::U_PLUS:
                    if (opNode->operand0->objType == A1ExprType::LITERAL) {
                        if (folded0.objType == LiteralType::INT || folded0.objType == LiteralType::FLOAT) {
                            return folded0;
                        }
                    }
                    break;

                case A1ExprOpType::U_MINUS:
                    if (opNode->operand0->objType == A1ExprType::LITERAL) {
                        if (folded0.objType == LiteralType::INT || folded0.objType == LiteralType::FLOAT) {
                            return Literal(-std::get<int64_t>(folded0.value));
                        } else if (folded0.objType == LiteralType::FLOAT) {
                            return Literal(-std::get<double>(folded0.value));
                        }
                    }
                    break;

                case A1ExprOpType::U_LOGIC_NOT:
                    if (opNode->operand0->objType == A1ExprType::LITERAL) {
                        if (folded0.objType == LiteralType::BOOL) {
                            return Literal(std::get<int64_t>(folded0.value) == 0);
                        }
                    }
                    break;

                case A1ExprOpType::U_BIT_NOT:
                    if (opNode->operand0->objType == A1ExprType::LITERAL) {
                        if (folded0.objType == LiteralType::INT) {
                            return Literal(~std::get<int64_t>(folded0.value));
                        }
                    }
                    break;

                case A1ExprOpType::U_SIZEOF:
                    if (opNode->operand0 && opNode->operand0->objType == A1ExprType::LITERAL) {
                        if (folded0.objType == LiteralType::INT || folded0.objType == LiteralType::FLOAT) {
                            return Literal((int64_t)8);
                        } else if (folded0.objType == LiteralType::STRING) {
                            return Literal((int64_t)arch * 2);
                        }
                    } else if (opNode->typeOperand) {
                        if (opNode->typeOperand->typeSize > 0) {
                            return Literal((int64_t)opNode->typeOperand->typeSize);
                        }
                    }
                    break;
            }

        } else if (opnum == 2 && opNode->subType != A1ExprOpType::B_DOT) { // try to fold binary
            switch (opNode->subType) {
                case A1ExprOpType::B_MUL:
                    if (opNode->operand0->objType == A1ExprType::LITERAL && opNode->operand1->objType == A1ExprType::LITERAL) {
                        if (folded0.objType == LiteralType::INT && folded1.objType == LiteralType::INT) {
                            return Literal(std::get<int64_t>(folded0.value) * std::get<int64_t>(folded1.value));
                        } else if (folded0.objType == LiteralType::FLOAT && folded1.objType == LiteralType::FLOAT) {
                            return Literal(std::get<double>(folded0.value) * std::get<double>(folded1.value));
                        }
                    }
                    break;

                case A1ExprOpType::B_DIV:
                    if (opNode->operand0->objType == A1ExprType::LITERAL && opNode->operand1->objType == A1ExprType::LITERAL) {
                        if (folded0.objType == LiteralType::INT && folded1.objType == LiteralType::INT) {
                            if (std::get<int64_t>(folded1.value) == 0) throw std::runtime_error(std::format("E0302 division by zero at {}", getLocString(opNode->location))); // E0302
                            if (std::get<int64_t>(folded0.value) == INT64_MIN && std::get<int64_t>(folded1.value) == -1) throw std::runtime_error(std::format("E03xx division overflow at {}", getLocString(opNode->location))); // E03xx
                            return Literal(std::get<int64_t>(folded0.value) / std::get<int64_t>(folded1.value));
                        } else if (folded0.objType == LiteralType::FLOAT && folded1.objType == LiteralType::FLOAT) {
                            if (std::get<double>(folded1.value) == 0.0) throw std::runtime_error(std::format("E0303 division by zero at {}", getLocString(opNode->location))); // E0303
                            return Literal(std::get<double>(folded0.value) / std::get<double>(folded1.value));
                        }
                    }
                    break;

                case A1ExprOpType::B_MOD:
                    if (opNode->operand0->objType == A1ExprType::LITERAL && opNode->operand1->objType == A1ExprType::LITERAL) {
                        if (folded0.objType == LiteralType::INT && folded1.objType == LiteralType::INT) {
                            if (std::get<int64_t>(folded1.value) == 0) throw std::runtime_error(std::format("E0304 division by zero at {}", getLocString(opNode->location))); // E0304
                            if (std::get<int64_t>(folded0.value) == INT64_MIN && std::get<int64_t>(folded1.value) == -1) throw std::runtime_error(std::format("E0305 division overflow at {}", getLocString(opNode->location))); // E0305
                            return Literal(std::get<int64_t>(folded0.value) % std::get<int64_t>(folded1.value));
                        }
                    }
                    break;

                case A1ExprOpType::B_ADD:
                    if (opNode->operand0->objType == A1ExprType::LITERAL && opNode->operand1->objType == A1ExprType::LITERAL) {
                        if (folded0.objType == LiteralType::INT && folded1.objType == LiteralType::INT) {
                            return Literal(std::get<int64_t>(folded0.value) + std::get<int64_t>(folded1.value));
                        } else if (folded0.objType == LiteralType::FLOAT && folded1.objType == LiteralType::FLOAT) {
                            return Literal(std::get<double>(folded0.value) + std::get<double>(folded1.value));
                        }
                    }
                    break;

                case A1ExprOpType::B_SUB:
                    if (opNode->operand0->objType == A1ExprType::LITERAL && opNode->operand1->objType == A1ExprType::LITERAL) {
                        if (folded0.objType == LiteralType::INT && folded1.objType == LiteralType::INT) {
                            return Literal(std::get<int64_t>(folded0.value) - std::get<int64_t>(folded1.value));
                        } else if (folded0.objType == LiteralType::FLOAT && folded1.objType == LiteralType::FLOAT) {
                            return Literal(std::get<double>(folded0.value) - std::get<double>(folded1.value));
                        }
                    }
                    break;

                case A1ExprOpType::B_SHL:
                    if (opNode->operand0->objType == A1ExprType::LITERAL && opNode->operand1->objType == A1ExprType::LITERAL) {
                        if (folded0.objType == LiteralType::INT && folded1.objType == LiteralType::INT) {
                            if (std::get<int64_t>(folded1.value) < 0 || std::get<int64_t>(folded1.value) > 63) throw std::runtime_error(std::format("E0306 shift amount out of range at {}", getLocString(opNode->location))); // E0306
                            return Literal(std::get<int64_t>(folded0.value) << std::get<int64_t>(folded1.value));
                        }
                    }
                    break;

                case A1ExprOpType::B_SHR:
                    if (opNode->operand0->objType == A1ExprType::LITERAL && opNode->operand1->objType == A1ExprType::LITERAL) {
                        if (folded0.objType == LiteralType::INT && folded1.objType == LiteralType::INT) {
                            if (std::get<int64_t>(folded1.value) < 0 || std::get<int64_t>(folded1.value) > 63) throw std::runtime_error(std::format("E0307 shift amount out of range at {}", getLocString(opNode->location))); // E0307
                            return Literal(std::get<int64_t>(folded0.value) >> std::get<int64_t>(folded1.value));
                        }
                    }
                    break;

                case A1ExprOpType::B_LT:
                    if (opNode->operand0->objType == A1ExprType::LITERAL && opNode->operand1->objType == A1ExprType::LITERAL) {
                        if (folded0.objType == LiteralType::INT && folded1.objType == LiteralType::INT) {
                            return Literal(std::get<int64_t>(folded0.value) < std::get<int64_t>(folded1.value));
                        } else if (folded0.objType == LiteralType::FLOAT && folded1.objType == LiteralType::FLOAT) {
                            return Literal(std::get<double>(folded0.value) < std::get<double>(folded1.value));
                        }
                    }
                    break;

                case A1ExprOpType::B_LE:
                    if (opNode->operand0->objType == A1ExprType::LITERAL && opNode->operand1->objType == A1ExprType::LITERAL) {
                        if (folded0.objType == LiteralType::INT && folded1.objType == LiteralType::INT) {
                            return Literal(std::get<int64_t>(folded0.value) <= std::get<int64_t>(folded1.value));
                        } else if (folded0.objType == LiteralType::FLOAT && folded1.objType == LiteralType::FLOAT) {
                            return Literal(std::get<double>(folded0.value) <= std::get<double>(folded1.value));
                        }
                    }
                    break;

                case A1ExprOpType::B_GT:
                    if (opNode->operand0->objType == A1ExprType::LITERAL && opNode->operand1->objType == A1ExprType::LITERAL) {
                        if (folded0.objType == LiteralType::INT && folded1.objType == LiteralType::INT) {
                            return Literal(std::get<int64_t>(folded0.value) > std::get<int64_t>(folded1.value));
                        } else if (folded0.objType == LiteralType::FLOAT && folded1.objType == LiteralType::FLOAT) {
                            return Literal(std::get<double>(folded0.value) > std::get<double>(folded1.value));
                        }
                    }
                    break;

                case A1ExprOpType::B_GE:
                    if (opNode->operand0->objType == A1ExprType::LITERAL && opNode->operand1->objType == A1ExprType::LITERAL) {
                        if (folded0.objType == LiteralType::INT && folded1.objType == LiteralType::INT) {
                            return Literal(std::get<int64_t>(folded0.value) >= std::get<int64_t>(folded1.value));
                        } else if (folded0.objType == LiteralType::FLOAT && folded1.objType == LiteralType::FLOAT) {
                            return Literal(std::get<double>(folded0.value) >= std::get<double>(folded1.value));
                        }
                    }
                    break;

                case A1ExprOpType::B_EQ:
                    if (opNode->operand0->objType == A1ExprType::LITERAL && opNode->operand1->objType == A1ExprType::LITERAL) {
                        if (folded0.objType == LiteralType::INT && folded1.objType == LiteralType::INT) {
                            return Literal(std::get<int64_t>(folded0.value) == std::get<int64_t>(folded1.value));
                        } else if (folded0.objType == LiteralType::FLOAT && folded1.objType == LiteralType::FLOAT) {
                            return Literal(std::get<double>(folded0.value) == std::get<double>(folded1.value));
                        } else if (folded0.objType == LiteralType::BOOL && folded1.objType == LiteralType::BOOL) {
                            return Literal(std::get<int64_t>(folded0.value) == std::get<int64_t>(folded1.value));
                        }
                    }
                    break;

                case A1ExprOpType::B_NE:
                    if (opNode->operand0->objType == A1ExprType::LITERAL && opNode->operand1->objType == A1ExprType::LITERAL) {
                        if (folded0.objType == LiteralType::INT && folded1.objType == LiteralType::INT) {
                            return Literal(std::get<int64_t>(folded0.value) != std::get<int64_t>(folded1.value));
                        } else if (folded0.objType == LiteralType::FLOAT && folded1.objType == LiteralType::FLOAT) {
                            return Literal(std::get<double>(folded0.value) != std::get<double>(folded1.value));
                        } else if (folded0.objType == LiteralType::BOOL && folded1.objType == LiteralType::BOOL) {
                            return Literal(std::get<int64_t>(folded0.value) != std::get<int64_t>(folded1.value));
                        }
                    }
                    break;

                case A1ExprOpType::B_BIT_AND:
                    if (opNode->operand0->objType == A1ExprType::LITERAL && opNode->operand1->objType == A1ExprType::LITERAL) {
                        if (folded0.objType == LiteralType::INT && folded1.objType == LiteralType::INT) {
                            return Literal(std::get<int64_t>(folded0.value) & std::get<int64_t>(folded1.value));
                        }
                    }
                    break;

                case A1ExprOpType::B_BIT_XOR:
                    if (opNode->operand0->objType == A1ExprType::LITERAL && opNode->operand1->objType == A1ExprType::LITERAL) {
                        if (folded0.objType == LiteralType::INT && folded1.objType == LiteralType::INT) {
                            return Literal(std::get<int64_t>(folded0.value) ^ std::get<int64_t>(folded1.value));
                        }
                    }
                    break;

                case A1ExprOpType::B_BIT_OR:
                    if (opNode->operand0->objType == A1ExprType::LITERAL && opNode->operand1->objType == A1ExprType::LITERAL) {
                        if (folded0.objType == LiteralType::INT && folded1.objType == LiteralType::INT) {
                            return Literal(std::get<int64_t>(folded0.value) | std::get<int64_t>(folded1.value));
                        }
                    }
                    break;

                case A1ExprOpType::B_LOGIC_AND:
                    if (opNode->operand0->objType == A1ExprType::LITERAL && opNode->operand1->objType == A1ExprType::LITERAL) {
                        if (folded0.objType == LiteralType::BOOL && folded1.objType == LiteralType::BOOL) {
                            return Literal(std::get<int64_t>(folded0.value) != 0 && std::get<int64_t>(folded1.value) != 0);
                        }
                    }
                    break;

                case A1ExprOpType::B_LOGIC_OR:
                    if (opNode->operand0->objType == A1ExprType::LITERAL && opNode->operand1->objType == A1ExprType::LITERAL) {
                        if (folded0.objType == LiteralType::BOOL && folded1.objType == LiteralType::BOOL) {
                            return Literal(std::get<int64_t>(folded0.value) != 0 || std::get<int64_t>(folded1.value) != 0);
                        }
                    }
                    break;
            }

        } else if (opNode->subType == A1ExprOpType::B_DOT) { // enum value or include name
            if (opNode->operand0->objType == A1ExprType::NAME) {
                std::string name0 = static_cast<A1ExprName*>(opNode->operand0.get())->name;
                A1Decl* search = mod.findDeclaration(name0, A1DeclType::ENUM, false);
                if (search != nullptr && opNode->operand1->objType == A1ExprType::NAME) { // enum value
                    std::string name1 = static_cast<A1ExprName*>(opNode->operand1.get())->name;
                    return mod.findLiteral(name0 + "." + name1, false);
                }
                search = mod.findDeclaration(name0, A1DeclType::INCLUDE, false);
                int pos = -1;
                if (search != nullptr) {
                    pos = findModule(((A1DeclInclude*)search)->tgtPath);
                }
                if (pos != -1 && opNode->operand1->objType == A1ExprType::NAME) { // foreign defined literal
                    return modules[pos]->findLiteral(static_cast<A1ExprName*>(opNode->operand1.get())->name, true);
                } else if (pos != -1 && opNode->operand1->objType == A1ExprType::OPERATION) { // foreign enum value
                    A1ExprOperation* subOpNode = (A1ExprOperation*)opNode->operand1.get();
                    if (subOpNode->subType == A1ExprOpType::B_DOT && subOpNode->operand0->objType == A1ExprType::NAME && subOpNode->operand1->objType == A1ExprType::NAME) {
                        return modules[pos]->findLiteral(static_cast<A1ExprName*>(subOpNode->operand0.get())->name + "." + static_cast<A1ExprName*>(subOpNode->operand1.get())->name, true);
                    }
                }
            }
        } else if (opNode->subType == A1ExprOpType::T_COND) { // try to fold ternary operator
            if (folded0.objType == LiteralType::BOOL) {
                if (std::get<int64_t>(folded0.value) != 0) {
                    return foldNode(*opNode->operand1, current, mod);
                } else {
                    return foldNode(*opNode->operand2, current, mod);
                }
            }
        }
    }
    return Literal();
}

// parse raw code
std::unique_ptr<A1StatRaw> A1Gen::parseRawCode(TokenProvider& tp) {
    Token& orderTkn = tp.pop();
    std::unique_ptr<A1StatRaw> rawCodeNode = std::make_unique<A1StatRaw>();
    rawCodeNode->location = orderTkn.location;
    if (orderTkn.objType == TokenType::ORDER_RAW_C) {
        rawCodeNode->objType = A1StatType::RAW_C;
    } else if (orderTkn.objType == TokenType::ORDER_RAW_IR) {
        rawCodeNode->objType = A1StatType::RAW_IR;
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
std::unique_ptr<A1DeclStruct> A1Gen::parseStruct(TokenProvider& tp, A1StatScope& current, A1Module& mod, bool isExported) {
    // check name validity, create struct node
    Token& idTkn = tp.pop();
    if (idTkn.objType != TokenType::IDENTIFIER) {
        throw std::runtime_error(std::format("E0403 expected identifier at {}", getLocString(idTkn.location))); // E0403
    }
    std::string nmValidity = mod.isNameUsable(idTkn.text, idTkn.location);
    if (!nmValidity.empty()) {
        throw std::runtime_error(nmValidity);
    }
    std::unique_ptr<A1DeclStruct> structNode = std::make_unique<A1DeclStruct>();
    structNode->name = idTkn.text;
    structNode->location = idTkn.location;
    if (tp.pop().objType != TokenType::OP_LBRACE) {
        throw std::runtime_error(std::format("E0404 expected '{{' at {}", getLocString(idTkn.location))); // E0404
    }

    // parse members
    while (tp.canPop(1)) {
        std::unique_ptr<A1Type> fieldType = mod.parseType(tp, current, arch);
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
    prt.Log(std::format("AST1 struct {} at {}", structNode->name, getLocString(structNode->location)), 1);
    return structNode;
}

// parse Enum declaration, after enum keyword
std::unique_ptr<A1DeclEnum> A1Gen::parseEnum(TokenProvider& tp, A1StatScope& current, A1Module& mod, bool isExported) {
    // check name validity, create enum node
    Token& idTkn = tp.pop();
    if (idTkn.objType != TokenType::IDENTIFIER) {
        throw std::runtime_error(std::format("E0410 expected identifier at {}", getLocString(idTkn.location))); // E0410
    }
    std::string nmValidity = mod.isNameUsable(idTkn.text, idTkn.location);
    if (!nmValidity.empty()) {
        throw std::runtime_error(nmValidity);
    }
    std::unique_ptr<A1DeclEnum> enumNode = std::make_unique<A1DeclEnum>();
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
            std::unique_ptr<A1Expr> value = parseExpr(tp, current, mod);
            if (value->objType != A1ExprType::LITERAL) {
                throw std::runtime_error(std::format("E0414 expected int constexpr at {}", getLocString(value->location))); // E0414
            }
            Literal lit = static_cast<A1ExprLiteral*>(value.get())->value;
            if (lit.objType != LiteralType::INT) {
                throw std::runtime_error(std::format("E0415 expected int constexpr at {}", getLocString(value->location))); // E0415
            }
            prevValue = std::get<int64_t>(lit.value) - 1;
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
    prt.Log(std::format("AST1 enum {} at {}", enumNode->name, getLocString(enumNode->location)), 1);
    return enumNode;
}

// parse function declaration
std::unique_ptr<A1DeclFunc> A1Gen::parseFunc(TokenProvider& tp, A1StatScope& current, A1Module& mod, std::unique_ptr<A1Type> retType, bool isVaArg, bool isExported) {
    // check name validity, create function node
    std::unique_ptr<A1DeclFunc> funcNode = std::make_unique<A1DeclFunc>();
    funcNode->location = retType->location;
    funcNode->retType = std::move(retType);
    funcNode->body = std::make_unique<A1StatScope>(&current);
    if (tp.match({TokenType::IDENTIFIER, TokenType::OP_DOT, TokenType::IDENTIFIER})) { // method
        Token& structTkn = tp.pop();
        tp.pop();
        Token& methodTkn = tp.pop();
        funcNode->name = structTkn.text + "." + methodTkn.text;
        funcNode->structNm = structTkn.text;
        funcNode->funcNm = methodTkn.text;
        if (mod.findDeclaration(structTkn.text, A1DeclType::STRUCT, false) == nullptr) {
            throw std::runtime_error(std::format("E0418 struct {} is not defined at {}", structTkn.text, getLocString(structTkn.location))); // E0418
        }
    } else if (tp.match({TokenType::IDENTIFIER})) { // function
        Token& idTkn = tp.pop();
        funcNode->name = idTkn.text;
    } else {
        throw std::runtime_error(std::format("E0419 expected identifier at {}", getLocString(funcNode->location))); // E0419
    }
    std::string nmValidity = mod.isNameUsable(funcNode->name, funcNode->location);
    if (!nmValidity.empty()) {
        throw std::runtime_error(nmValidity);
    }

    // parse parameters
    if (tp.pop().objType != TokenType::OP_LPAREN) {
        throw std::runtime_error(std::format("E0420 expected '(' at {}", getLocString(funcNode->location))); // E0420
    }
    if (tp.seek().objType != TokenType::OP_RPAREN) {
        while (tp.canPop(1)) {
            std::unique_ptr<A1Type> paramType = mod.parseType(tp, current, arch);
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
            funcNode->paramTypes.push_back(paramType->Clone());
            std::unique_ptr<A1DeclVar> pvar = std::make_unique<A1DeclVar>(std::move(paramType), paramNameTkn.text);
            pvar->location = paramNameTkn.location;
            pvar->isParam = true;
            std::unique_ptr<A1StatDecl> pvarStat = std::make_unique<A1StatDecl>();
            pvarStat->decl = std::move(pvar);
            funcNode->body->body.push_back(std::move(pvarStat));
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
    funcNode->body->body.push_back(parseScope(tp, *funcNode->body, mod));
    funcNode->isVaArg = isVaArg;
    funcNode->isExported = isExported;
    if (!funcNode->structNm.empty()) { // check method
        if (funcNode->paramTypes.size() == 0
                || funcNode->paramTypes[0]->objType != A1TypeType::POINTER
                || funcNode->paramTypes[0]->direct->objType != A1TypeType::NAME
                || funcNode->paramTypes[0]->direct->name != funcNode->structNm) {
            throw std::runtime_error(std::format("E0426 first parameter must be {}* at {}", funcNode->structNm, getLocString(funcNode->location))); // E0426
        }
    }
    if (funcNode->isVaArg) { // check va_arg
        if (funcNode->paramTypes.size() < 2) {
            throw std::runtime_error(std::format("E0427 last two parameters must be (void**, int) at {}", getLocString(funcNode->location))); // E0427
        }
        A1Type* arg0 = funcNode->paramTypes[funcNode->paramTypes.size() - 2].get();
        A1Type* arg1 = funcNode->paramTypes[funcNode->paramTypes.size() - 1].get();
        bool flag0 = false;
        bool flag1 = false;
        if (arg0->objType == A1TypeType::POINTER
                && arg0->direct->objType == A1TypeType::POINTER
                && arg0->direct->direct->objType == A1TypeType::PRIMITIVE
                && arg0->direct->direct->name == "void") {
            flag0 = true;
        }
        if (arg1->objType == A1TypeType::PRIMITIVE && arg1->name == "int") {
            flag1 = true;
        }
        if (!flag0 || !flag1) {
            throw std::runtime_error(std::format("E0428 last two parameters must be (void**, int) at {}", getLocString(funcNode->location))); // E0428
        }
    }
    prt.Log(std::format("AST1 func {} at {}", funcNode->name, getLocString(funcNode->location)), 1);
    return funcNode;
}

// parse after #typedef
std::unique_ptr<A1DeclTypedef> A1Gen::parseTypedef(TokenProvider& tp, A1StatScope& current, A1Module& mod) {
    Token& nameTkn = tp.pop();
    if (nameTkn.objType != TokenType::IDENTIFIER) {
        throw std::runtime_error(std::format("E0429 expected identifier at {}", getLocString(nameTkn.location))); // E0429
    }
    std::unique_ptr<A1DeclTypedef> typedefNode = std::make_unique<A1DeclTypedef>();
    typedefNode->location = nameTkn.location;
    typedefNode->name = nameTkn.text;
    typedefNode->type = mod.parseType(tp, current, arch);
    return typedefNode;
}

// parse atomic expression
std::unique_ptr<A1Expr> A1Gen::parseAtomicExpr(TokenProvider& tp, A1StatScope& current, A1Module& mod) {
    Token& tkn = tp.pop();
    std::unique_ptr<A1Expr> result;
    switch (tkn.objType) {
        case TokenType::LIT_INT: case TokenType::LIT_FLOAT: case TokenType::LIT_STRING: // literal
            {
                std::unique_ptr<A1ExprLiteral> litNode = std::make_unique<A1ExprLiteral>(tkn.value);
                litNode->location = tkn.location;
                result = std::move(litNode);
            }
            break;

        case TokenType::KEY_NULL: case TokenType::KEY_TRUE: case TokenType::KEY_FALSE: // keyword literals
            {
                std::unique_ptr<A1ExprLiteral> litNode;
                if (tkn.objType == TokenType::KEY_NULL) {
                    litNode = std::make_unique<A1ExprLiteral>(Literal(nullptr));
                } else if (tkn.objType == TokenType::KEY_TRUE) {
                    litNode = std::make_unique<A1ExprLiteral>(Literal(true));
                } else if (tkn.objType == TokenType::KEY_FALSE) {
                    litNode = std::make_unique<A1ExprLiteral>(Literal(false));
                }
                litNode->location = tkn.location;
                result = std::move(litNode);
            }
            break;

        case TokenType::IDENTIFIER: // variable name
            {
                std::unique_ptr<A1ExprName> nameNode = std::make_unique<A1ExprName>(tkn.text);
                nameNode->location = tkn.location;
                result = std::move(nameNode);
            }
            break;

        case TokenType::OP_LPAREN: // parenthesis expression
            {
                result = parsePrattExpr(tp, current, mod, 0);
                if (tp.pop().objType != TokenType::OP_RPAREN) {
                    throw std::runtime_error(std::format("E0501 expected ')' at {}", getLocString(tkn.location))); // E0501
                }
            }
            break;

        case TokenType::OP_LBRACE: // literal array, struct
            {
                std::unique_ptr<A1ExprLiteralData> arrNode = std::make_unique<A1ExprLiteralData>();
                arrNode->location = tkn.location;
                while (tp.canPop(1)) {
                    arrNode->elements.push_back(parseExpr(tp, current, mod));
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
                std::unique_ptr<A1ExprOperation> unaryNode = std::make_unique<A1ExprOperation>();
                unaryNode->location = tkn.location;
                if (tkn.objType == TokenType::OP_PLUS) {
                    unaryNode->subType = A1ExprOpType::U_PLUS;
                } else if (tkn.objType == TokenType::OP_MINUS) {
                    unaryNode->subType = A1ExprOpType::U_MINUS;
                } else if (tkn.objType == TokenType::OP_LOGIC_NOT) {
                    unaryNode->subType = A1ExprOpType::U_LOGIC_NOT;
                } else if (tkn.objType == TokenType::OP_BIT_NOT) {
                    unaryNode->subType = A1ExprOpType::U_BIT_NOT;
                } else if (tkn.objType == TokenType::OP_MUL) {
                    unaryNode->subType = A1ExprOpType::U_DEREF;
                } else if (tkn.objType == TokenType::OP_BIT_AND) {
                    unaryNode->subType = A1ExprOpType::U_REF;
                }
                unaryNode->operand0 = parsePrattExpr(tp, current, mod, getPrattPrecedence(TokenType::OP_PLUS, true));
                result = std::move(unaryNode);
            }
            break;

        case TokenType::IFUNC_MAKE:
            {
                if (tp.pop().objType != TokenType::OP_LPAREN) {
                    throw std::runtime_error(std::format("E0504 expected '(' at {}", getLocString(tkn.location))); // E0504
                }
                std::unique_ptr<A1ExprOperation> makeNode = std::make_unique<A1ExprOperation>(A1ExprOpType::B_MAKE);
                makeNode->location = tkn.location;
                makeNode->operand0 = parsePrattExpr(tp, current, mod, 0);
                if (tp.pop().objType != TokenType::OP_COMMA) {
                    throw std::runtime_error(std::format("E0505 expected ',' at {}", getLocString(tkn.location))); // E0505
                }
                makeNode->operand1 = parsePrattExpr(tp, current, mod, 0);
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
                std::unique_ptr<A1ExprOperation> lenNode = std::make_unique<A1ExprOperation>(A1ExprOpType::U_LEN);
                lenNode->location = tkn.location;
                lenNode->operand0 = parsePrattExpr(tp, current, mod, 0);
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
                std::unique_ptr<A1ExprOperation> castNode = std::make_unique<A1ExprOperation>(A1ExprOpType::B_CAST);
                castNode->location = tkn.location;
                castNode->typeOperand = mod.parseType(tp, current, arch);
                if (tp.pop().objType != TokenType::OP_GT) {
                    throw std::runtime_error(std::format("E0510 expected '>' at {}", getLocString(tkn.location))); // E0510
                }
                if (tp.pop().objType != TokenType::OP_LPAREN) {
                    throw std::runtime_error(std::format("E0511 expected '(' at {}", getLocString(tkn.location))); // E0511
                }
                castNode->operand1 = parsePrattExpr(tp, current, mod, 0);
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
                std::unique_ptr<A1ExprOperation> sizeofNode = std::make_unique<A1ExprOperation>(A1ExprOpType::U_SIZEOF);
                sizeofNode->location = tkn.location;
                if (isTypeStart(tp, mod)) {
                    sizeofNode->typeOperand = mod.parseType(tp, current, arch);
                } else {
                    sizeofNode->operand0 = parsePrattExpr(tp, current, mod, 0);
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
std::unique_ptr<A1Expr> A1Gen::parsePrattExpr(TokenProvider& tp, A1StatScope& current, A1Module& mod, int level) {
    std::unique_ptr<A1Expr> lhs = parseAtomicExpr(tp, current, mod); // LHS is start of expression
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
                    std::unique_ptr<A1ExprOperation> memberNode = std::make_unique<A1ExprOperation>(A1ExprOpType::B_DOT);
                    memberNode->location = opTkn.location;
                    memberNode->operand0 = std::move(lhs);
                    memberNode->operand1 = std::make_unique<A1ExprName>(memberTkn.text);
                    memberNode->operand1->location = memberTkn.location;
                    lhs = std::move(memberNode);
                }
                break;

            case TokenType::OP_LPAREN: // function call
                {
                    std::unique_ptr<A1ExprFuncCall> callNode = std::make_unique<A1ExprFuncCall>();
                    callNode->location = opTkn.location;
                    callNode->func = std::move(lhs);
                    if (tp.seek().objType != TokenType::OP_RPAREN) {
                        while (tp.canPop(1)) {
                            std::unique_ptr<A1Expr> argExpr = parsePrattExpr(tp, current, mod, 0);
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
                    bool isIndex = true;
                    std::unique_ptr<A1Expr> left = std::make_unique<A1Expr>();
                    left->location = opTkn.location;
                    std::unique_ptr<A1Expr> right = std::make_unique<A1Expr>();
                    right->location = opTkn.location;
                    if (tp.seek().objType != TokenType::OP_COLON) {
                        left = parsePrattExpr(tp, current, mod, 0);
                    }
                    if (tp.seek().objType == TokenType::OP_COLON) { // slice
                        tp.pop();
                        isIndex = false;
                        if (tp.seek().objType != TokenType::OP_RBRACKET) {
                            right = parsePrattExpr(tp, current, mod, 0);
                        }
                    }
                    if (tp.pop().objType != TokenType::OP_RBRACKET) {
                        throw std::runtime_error(std::format("E0519 expected ']' at {}", getLocString(opTkn.location))); // E0519
                    }
                    if (isIndex) { // index
                        std::unique_ptr<A1ExprOperation> indexNode = std::make_unique<A1ExprOperation>(A1ExprOpType::B_INDEX);
                        indexNode->location = opTkn.location;
                        indexNode->operand0 = std::move(lhs);
                        indexNode->operand1 = std::move(left);
                        lhs = std::move(indexNode);
                    } else { // slice
                        std::unique_ptr<A1ExprOperation> sliceNode = std::make_unique<A1ExprOperation>(A1ExprOpType::T_SLICE);
                        sliceNode->location = opTkn.location;
                        sliceNode->operand0 = std::move(lhs);
                        sliceNode->operand1 = std::move(left);
                        sliceNode->operand2 = std::move(right);
                        lhs = std::move(sliceNode);
                    }
                }
                break;
            
            case TokenType::OP_QMARK: // ternary operator
                {
                    std::unique_ptr<A1ExprOperation> ternOpNode = std::make_unique<A1ExprOperation>(A1ExprOpType::T_COND);
                    ternOpNode->location = opTkn.location;
                    ternOpNode->operand0 = std::move(lhs);
                    ternOpNode->operand1 = parsePrattExpr(tp, current, mod, 0);
                    if (tp.pop().objType != TokenType::OP_COLON) {
                        throw std::runtime_error(std::format("E0520 expected ':' at {}", getLocString(opTkn.location))); // E0520
                    }
                    ternOpNode->operand2 = parsePrattExpr(tp, current, mod, 0);
                    lhs = std::move(ternOpNode);
                }
                break;

            default: // binary operator
                {
                    std::unique_ptr<A1ExprOperation> binOpNode = std::make_unique<A1ExprOperation>(getBinaryOpType(opTkn.objType));
                    binOpNode->location = opTkn.location;
                    if (binOpNode->subType == A1ExprOpType::NONE) {
                        throw std::runtime_error(std::format("E0521 invalid binary operator {} at {}", opTkn.text, getLocString(opTkn.location))); // E0521
                    }
                    binOpNode->operand0 = std::move(lhs);
                    binOpNode->operand1 = parsePrattExpr(tp, current, mod, mylvl + 1);
                    lhs = std::move(binOpNode);
                }
        }
    }
    return lhs;
}

// parse expression, fold if possible
std::unique_ptr<A1Expr> A1Gen::parseExpr(TokenProvider& tp, A1StatScope& current, A1Module& mod) {
    std::unique_ptr<A1Expr> expr = parsePrattExpr(tp, current, mod, 0);
    Literal lit = foldNode(*expr, current, mod);
    if (lit.objType != LiteralType::NONE) {
        Location loc = expr->location;
        expr = std::make_unique<A1ExprLiteral>(lit);
        expr->location = loc;
    }
    return expr;
}

// parse variable declaration, after type declaration
std::unique_ptr<A1DeclVar> A1Gen::parseVarDecl(TokenProvider& tp, A1StatScope& current, A1Module& mod, std::unique_ptr<A1Type> varType, bool isDefine, bool isConst, bool isVolatile, bool isExtern, bool isExported) {
    // check type validity, create variable declaration node
    if (varType->typeSize == 0) {
        throw std::runtime_error(std::format("E0601 variable cannot be void type at {}", getLocString(varType->location))); // E0601
    }
    Token& nameTkn = tp.pop();
    if (nameTkn.objType != TokenType::IDENTIFIER) {
        throw std::runtime_error(std::format("E0602 expected identifier at {}", getLocString(nameTkn.location))); // E0602
    }
    std::unique_ptr<A1DeclVar> varDecl = std::make_unique<A1DeclVar>(std::move(varType), nameTkn.text);
    varDecl->location = nameTkn.location;

    // check name duplication inside scope (must check global scope for toplevels)
    for (auto& stat : current.body) {
        if (stat->objType == A1StatType::DECL && static_cast<A1StatDecl*>(stat.get())->decl->name == nameTkn.text) {
            throw std::runtime_error(std::format("E0603 variable name {} already exists at {}", nameTkn.text, getLocString(nameTkn.location))); // E0603
        }
    }

    // parse variable initialization
    Token& opTkn = tp.pop();
    if (opTkn.objType == TokenType::OP_ASSIGN) {
        varDecl->init = parseExpr(tp, current, mod);
        opTkn = tp.pop();
    }
    if (opTkn.objType != TokenType::OP_SEMICOLON) {
        throw std::runtime_error(std::format("E0604 expected ';' at {}", getLocString(opTkn.location))); // E0604
    }

    // check variable declaration
    varDecl->isDefine = isDefine;
    varDecl->isConst = isConst;
    varDecl->isVolatile = isVolatile;
    varDecl->isExtern = isExtern;
    varDecl->isExported = isExported;
    if (isDefine || isConst) { // define or const should be initialized with literal
        if (varDecl->init == nullptr
                || (varDecl->init->objType != A1ExprType::LITERAL)) {
            throw std::runtime_error(std::format("E0605 variable should be initialized with constexpr at {}", getLocString(nameTkn.location))); // E0605
        }
    }
    if (isExtern && varDecl->init != nullptr) { // extern variable should not be initialized
        throw std::runtime_error(std::format("E0606 extern variable should not be initialized at {}", getLocString(nameTkn.location))); // E0606
    }
    if (isExtern && isExported) { // cannot be both extern and exported
        throw std::runtime_error(std::format("E0607 cannot be both extern and exported at {}", getLocString(nameTkn.location))); // E0607
    }
    prt.Log(std::format("AST1 var decl {} at {}", varDecl->name, getLocString(varDecl->location)), 1);
    return varDecl;
}

// parse variable assignment, after lvalue =
std::unique_ptr<A1StatAssign> A1Gen::parseVarAssign(TokenProvider& tp, A1StatScope& current, A1Module& mod, std::unique_ptr<A1Expr> lvalue, A1StatAssignType assignType, TokenType endExpect) {
    std::unique_ptr<A1StatAssign> varAssign = std::make_unique<A1StatAssign>();
    varAssign->location = lvalue->location;
    varAssign->subType = assignType;
    varAssign->left = std::move(lvalue);
    varAssign->right = parseExpr(tp, current, mod);
    if (tp.pop().objType != endExpect) {
        throw std::runtime_error(std::format("E0608 invalid statement ending at {}", getLocString(varAssign->location))); // E0608
    }
    return varAssign;
}

// parse general statement
std::unique_ptr<A1Stat> A1Gen::parseStatement(TokenProvider& tp, A1StatScope& current, A1Module& mod) {
    bool isDefine = false;
    bool isConst = false;
    bool isVolatile = false;
    bool isExtern = false;
    while (tp.canPop(1)) {
        Token& tkn = tp.seek();
        switch (tkn.objType) {
            case TokenType::KEY_IF: // if statement
            {
                tp.pop();
                std::unique_ptr<A1StatIf> ifNode = std::make_unique<A1StatIf>();
                ifNode->location = tkn.location;
                if (tp.pop().objType != TokenType::OP_LPAREN) {
                    throw std::runtime_error(std::format("E0609 expected '(' at {}", getLocString(ifNode->location))); // E0609
                }
                ifNode->cond = parseExpr(tp, current, mod);
                if (tp.pop().objType != TokenType::OP_RPAREN) {
                    throw std::runtime_error(std::format("E0610 expected ')' at {}", getLocString(ifNode->location))); // E0610
                }
                ifNode->thenBody = parseStatement(tp, current, mod);
                if (tp.seek().objType == TokenType::KEY_ELSE) { // else statement
                    tp.pop();
                    ifNode->elseBody = parseStatement(tp, current, mod);
                }
                return ifNode;
            }

            case TokenType::KEY_WHILE: // while statement
            {
                tp.pop();
                std::unique_ptr<A1StatWhile> whileNode = std::make_unique<A1StatWhile>();
                whileNode->location = tkn.location;
                if (tp.pop().objType != TokenType::OP_LPAREN) {
                    throw std::runtime_error(std::format("E0611 expected '(' at {}", getLocString(whileNode->location))); // E0611
                }
                whileNode->cond = parseExpr(tp, current, mod);
                if (tp.pop().objType != TokenType::OP_RPAREN) {
                    throw std::runtime_error(std::format("E0612 expected ')' at {}", getLocString(whileNode->location))); // E0612
                }
                whileNode->body = parseStatement(tp, current, mod);
                return whileNode;
            }

            case TokenType::KEY_FOR: // for statement
            {
                // init statement will be at another outside scope
                tp.pop();
                std::unique_ptr<A1StatScope> forScope = std::make_unique<A1StatScope>(&current);
                forScope->location = tkn.location;
                std::unique_ptr<A1StatFor> forNode = std::make_unique<A1StatFor>();
                forNode->location = tkn.location;
                if (tp.pop().objType != TokenType::OP_LPAREN) {
                    throw std::runtime_error(std::format("E0613 expected '(' at {}", getLocString(forScope->location))); // E0613
                }

                // parse init
                std::unique_ptr<A1Stat> initNode = parseStatement(tp, *forScope, mod);
                if (initNode->objType != A1StatType::DECL && initNode->objType != A1StatType::ASSIGN && initNode->objType != A1StatType::NONE) {
                    throw std::runtime_error(std::format("E0614 invalid for_init statement at {}", getLocString(initNode->location))); // E0614
                }
                forScope->body.push_back(std::move(initNode));

                // parse cond and step
                if (tp.seek().objType == TokenType::OP_SEMICOLON) {
                    forNode->cond = std::make_unique<A1ExprLiteral>(Literal(true));
                } else {
                    forNode->cond = parseExpr(tp, *forScope, mod);
                }
                if (tp.pop().objType != TokenType::OP_SEMICOLON) {
                    throw std::runtime_error(std::format("E0615 expected ';' at {}", getLocString(forNode->location))); // E0615
                }
                if (tp.seek().objType == TokenType::OP_RPAREN) {
                    tp.pop();
                } else {
                    std::unique_ptr<A1Expr> left = parseExpr(tp, *forScope, mod);
                    Token& opTkn = tp.pop();
                    A1StatAssignType assignType = getAssignType(opTkn);
                    if (opTkn.objType != TokenType::OP_RPAREN) {
                        if (assignType == A1StatAssignType::NONE) {
                            throw std::runtime_error(std::format("E0616 expected ')' at {}", getLocString(forNode->location))); // E0616
                        }
                        forNode->step = parseVarAssign(tp, *forScope, mod, std::move(left), assignType, TokenType::OP_RPAREN);
                    } else {
                        std::unique_ptr<A1StatExpr> stepNode = std::make_unique<A1StatExpr>();
                        stepNode->location = tkn.location;
                        stepNode->expr = parseExpr(tp, *forScope, mod);
                        forNode->step = std::move(stepNode);
                    }
                }

                // parse body
                forNode->body = parseStatement(tp, *forScope, mod);
                forScope->body.push_back(std::move(forNode));
                return forScope;
            }

            case TokenType::KEY_SWITCH: // switch statement
            {
                // parse switch_expr, make switch node
                tp.pop();
                std::unique_ptr<A1StatSwitch> switchNode = std::make_unique<A1StatSwitch>();
                switchNode->location = tkn.location;
                if (tp.pop().objType != TokenType::OP_LPAREN) {
                    throw std::runtime_error(std::format("E0617 expected '(' at {}", getLocString(switchNode->location))); // E0617
                }
                switchNode->cond = parseExpr(tp, current, mod);
                if (tp.pop().objType != TokenType::OP_RPAREN) {
                    throw std::runtime_error(std::format("E0618 expected ')' at {}", getLocString(switchNode->location))); // E0618
                }
                if (tp.pop().objType != TokenType::OP_LBRACE) {
                    throw std::runtime_error(std::format("E0619 expected '{{' at {}", getLocString(switchNode->location))); // E0619
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
                        std::unique_ptr<A1Expr> value = parseExpr(tp, current, mod);
                        if (value->objType != A1ExprType::LITERAL) {
                            throw std::runtime_error(std::format("E0621 case_expr must be int constexpr at {}", getLocString(value->location))); // E0621
                        }
                        Literal lit = static_cast<A1ExprLiteral*>(value.get())->value;
                        if (lit.objType != LiteralType::INT) {
                            throw std::runtime_error(std::format("E0622 case_expr must be int constexpr at {}", getLocString(value->location))); // E0622
                        }
                        int64_t caseValue = std::get<int64_t>(lit.value);
                        if (tp.pop().objType != TokenType::OP_COLON) {
                            throw std::runtime_error(std::format("E0623 expected ':' at {}", getLocString(caseTkn.location))); // E0623
                        }
                        for (auto& cond : switchNode->caseConds) {
                            if (cond == caseValue) {
                                throw std::runtime_error(std::format("E0624 case value {} already exists at {}", caseValue, getLocString(caseTkn.location))); // E0624
                            }
                        }
                        switchNode->caseConds.push_back(caseValue);
                        switchNode->caseBodies.push_back(std::vector<std::unique_ptr<A1Stat>>());

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
                        switchNode->caseBodies.back().push_back(parseStatement(tp, current, mod));

                    } else if (!pushCase && defaultFound) { // push to default body
                        switchNode->defaultBody.push_back(parseStatement(tp, current, mod));

                    } else { // statement before case
                        throw std::runtime_error(std::format("E0627 statement before case at {}", getLocString(caseTkn.location))); // E0627
                    }
                }
                return switchNode;
            }

            case TokenType::KEY_BREAK:
            {
                tp.pop();
                std::unique_ptr<A1StatCtrl> result = std::make_unique<A1StatCtrl>(A1StatType::BREAK);
                result->location = tkn.location;
                return result;
            }

            case TokenType::KEY_CONTINUE:
            {
                tp.pop();
                std::unique_ptr<A1StatCtrl> result = std::make_unique<A1StatCtrl>(A1StatType::CONTINUE);
                result->location = tkn.location;
                return result;
            }

            case TokenType::KEY_FALL:
            {
                tp.pop();
                std::unique_ptr<A1StatCtrl> result = std::make_unique<A1StatCtrl>(A1StatType::FALL);
                result->location = tkn.location;
                return result;
            }

            case TokenType::KEY_RETURN: // return statement
            {
                tp.pop();
                std::unique_ptr<A1StatCtrl> result = std::make_unique<A1StatCtrl>(A1StatType::RETURN);
                result->location = tkn.location;
                if (tp.seek().objType == TokenType::OP_SEMICOLON) {
                    result->body = std::make_unique<A1Expr>();
                    result->body->location = tkn.location;
                } else {
                    result->body = parseExpr(tp, current, mod);
                }
                if (tp.pop().objType != TokenType::OP_SEMICOLON) {
                    throw std::runtime_error(std::format("E0628 expected ';' at {}", getLocString(result->location))); // E0628
                }
                return result;
            }

            case TokenType::ORDER_DEFER: // defer statement
            {
                tp.pop();
                std::unique_ptr<A1StatCtrl> result = std::make_unique<A1StatCtrl>(A1StatType::DEFER);
                result->location = tkn.location;
                result->body = parseExpr(tp, current, mod);
                if (tp.pop().objType != TokenType::OP_SEMICOLON) {
                    throw std::runtime_error(std::format("E0629 expected ';' at {}", getLocString(result->location))); // E0629
                }
                return result;
            }

            case TokenType::ORDER_TYPEDEF: // typedef
            {
                tp.pop();
                std::unique_ptr<A1StatDecl> result = std::make_unique<A1StatDecl>();
                result->location = tkn.location;
                result->decl = parseTypedef(tp, current, mod);
                return result;
            }

            case TokenType::OP_LBRACE: // scope
                return parseScope(tp, current, mod);

            case TokenType::OP_SEMICOLON: // empty statement
            {
                tp.pop();
                std::unique_ptr<A1Stat> result = std::make_unique<A1Stat>();
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

            case TokenType::ORDER_CONST:
                if (isConst) {
                    throw std::runtime_error(std::format("E0631 duplicated const at {}", getLocString(tkn.location))); // E0631
                }
                isConst = true;
                tp.pop();
                break;

            case TokenType::ORDER_VOLATILE:
                if (isVolatile) {
                    throw std::runtime_error(std::format("E0632 duplicated volatile at {}", getLocString(tkn.location))); // E0632
                }
                isVolatile = true;
                tp.pop();
                break;

            case TokenType::ORDER_EXTERN:
                if (isExtern) {
                    throw std::runtime_error(std::format("E0633 duplicated extern at {}", getLocString(tkn.location))); // E0633
                }
                isExtern = true;
                tp.pop();
                break;

            case TokenType::ORDER_RAW_C: case TokenType::ORDER_RAW_IR:
                return parseRawCode(tp);

            default:
                if (isTypeStart(tp, mod)) { // var declaration
                    std::unique_ptr<A1StatDecl> result = std::make_unique<A1StatDecl>();
                    result->location = tkn.location;
                    result->decl = parseVarDecl(tp, current, mod, mod.parseType(tp, current, arch), isDefine, isConst, isVolatile, isExtern, false);
                    return result;
                } else {
                    std::unique_ptr<A1Expr> left = parseExpr(tp, current, mod);
                    Token& opTkn = tp.pop();
                    A1StatAssignType assignType = getAssignType(opTkn);
                    if (opTkn.objType == TokenType::OP_SEMICOLON) { // expression statement
                        std::unique_ptr<A1StatExpr> result = std::make_unique<A1StatExpr>();
                        result->location = tkn.location;
                        result->expr = std::move(left);
                        return result;
                    } else if (assignType != A1StatAssignType::NONE) { // assign statement
                        return parseVarAssign(tp, current, mod, std::move(left), assignType, TokenType::OP_SEMICOLON);
                    } else {
                        throw std::runtime_error(std::format("E0634 expected ';' at {}", getLocString(opTkn.location))); // E0634
                    }
                }
        }
    }
    throw std::runtime_error(std::format("E0635 unexpected EOF while parsing statement at {}", getLocString(current.location))); // E0635
}

// parse scope
std::unique_ptr<A1StatScope> A1Gen::parseScope(TokenProvider& tp, A1StatScope& current, A1Module& mod) {
    if (tp.pop().objType != TokenType::OP_LBRACE) {
        throw std::runtime_error(std::format("E0636 expected '{{' at {}", getLocString(current.location))); // E0636
    }
    std::unique_ptr<A1StatScope> scope = std::make_unique<A1StatScope>(&current);
    scope->location = current.location;
    while (tp.canPop(1)) {
        Token& tkn = tp.seek();
        if (tkn.objType == TokenType::OP_RBRACE) {
            tp.pop();
            break;
        }
        scope->body.push_back(parseStatement(tp, *scope, mod));
    }
    return scope;
}

// parse toplevel declaration
std::unique_ptr<A1StatDecl> A1Gen::parseTopLevel(TokenProvider& tp, A1StatScope& current, A1Module& mod) {
    bool isDefine = false;
    bool isConst = false;
    bool isVolatile = false;
    bool isExtern = false;
    bool isExported = false;
    bool isVaArg = false;
    while (tp.canPop(1)) {
        Token& tkn = tp.seek();
        switch (tkn.objType) {
            case TokenType::ORDER_INCLUDE:
            {
                tp.pop();
                std::unique_ptr<A1DeclInclude> incNode = std::make_unique<A1DeclInclude>();
                incNode->location = tkn.location;
                if (tp.seek().objType == TokenType::OP_LT) { // template arguments
                    while (tp.canPop(1)) {
                        incNode->argTypes.push_back(mod.parseType(tp, current, arch));
                        Token& opTkn = tp.seek();
                        if (opTkn.objType == TokenType::OP_COMMA) {
                            tp.pop();
                        } else if (opTkn.objType == TokenType::OP_GT) {
                            tp.pop();
                            break;
                        } else {
                            throw std::runtime_error(std::format("E0637 expected '>' at {}", getLocString(opTkn.location))); // E0637
                        }
                    }
                }
                if (tp.match({TokenType::LIT_STRING, TokenType::IDENTIFIER})) {
                    incNode->tgtPath = absPath(tp.pop().text, getWorkingDir(mod.path));
                    incNode->name = tp.pop().text;
                } else {
                    throw std::runtime_error(std::format("E0638 expected module filepath at {}", getLocString(tkn.location))); // E0638
                }
                std::string nmValidity = mod.isNameUsable(incNode->name, tkn.location);
                if (!nmValidity.empty()) {
                    throw std::runtime_error(nmValidity);
                }
                std::unique_ptr<A1StatDecl> result = std::make_unique<A1StatDecl>();
                result->location = tkn.location;
                result->decl = std::move(incNode);
                return result;
            }

            case TokenType::ORDER_TEMPLATE:
            {
                tp.pop();
                mod.tmpArgsCount++;
                std::unique_ptr<A1DeclTemplate> dNode = std::make_unique<A1DeclTemplate>();
                dNode->location = tkn.location;
                Token& tmpTkn = tp.pop();
                if (tmpTkn.objType != TokenType::IDENTIFIER) {
                    throw std::runtime_error(std::format("E0639 expected identifier at {}", getLocString(tmpTkn.location))); // E0639
                }
                std::string nmValidity = mod.isNameUsable(tmpTkn.text, tmpTkn.location);
                if (!nmValidity.empty()) {
                    throw std::runtime_error(nmValidity);
                }
                dNode->name = tmpTkn.text;
                std::unique_ptr<A1StatDecl> result = std::make_unique<A1StatDecl>();
                result->location = tkn.location;
                result->decl = std::move(dNode);
                return result;
            }

            case TokenType::ORDER_TYPEDEF:
            {
                tp.pop();
                std::unique_ptr<A1StatDecl> result = std::make_unique<A1StatDecl>();
                result->location = tkn.location;
                result->decl = parseTypedef(tp, current, mod);
                return result;
            }

            case TokenType::ORDER_RAW_C: case TokenType::ORDER_RAW_IR:
            {
                std::unique_ptr<A1StatRaw> raw = parseRawCode(tp);
                std::unique_ptr<A1DeclRaw> rNode = std::make_unique<A1DeclRaw>(raw->objType == A1StatType::RAW_C ? A1DeclType::RAW_C : A1DeclType::RAW_IR);
                rNode->location = tkn.location;
                rNode->code = std::move(raw->code);
                std::unique_ptr<A1StatDecl> result = std::make_unique<A1StatDecl>();
                result->location = tkn.location;
                result->decl = std::move(rNode);
                return result;
            }

            case TokenType::OP_SEMICOLON:
            {
                tp.pop();
                std::unique_ptr<A1StatDecl> result = std::make_unique<A1StatDecl>();
                result->location = tkn.location;
                return result;
            }

            case TokenType::ORDER_DEFINE:
                if (isDefine) {
                    throw std::runtime_error(std::format("E0640 duplicate define at {}", getLocString(tkn.location))); // E0640
                }
                tp.pop();
                isDefine = true;
                break;

            case TokenType::ORDER_CONST:
                if (isConst) {
                    throw std::runtime_error(std::format("E0641 duplicate const at {}", getLocString(tkn.location))); // E0641
                }
                tp.pop();
                isConst = true;
                break;

            case TokenType::ORDER_VOLATILE:
                if (isVolatile) {
                    throw std::runtime_error(std::format("E0642 duplicate volatile at {}", getLocString(tkn.location))); // E0642
                }
                tp.pop();
                isVolatile = true;
                break;

            case TokenType::ORDER_EXTERN:
                if (isExtern) {
                    throw std::runtime_error(std::format("E0643 duplicate extern at {}", getLocString(tkn.location))); // E0643
                }
                tp.pop();
                isExtern = true;
                break;

            case TokenType::ORDER_EXPORT:
                if (isExported) {
                    throw std::runtime_error(std::format("E0644 duplicate export at {}", getLocString(tkn.location))); // E0644
                }
                tp.pop();
                isExported = true;
                break;

            case TokenType::ORDER_VA_ARG:
                if (isVaArg) {
                    throw std::runtime_error(std::format("E0645 duplicate va_arg at {}", getLocString(tkn.location))); // E0645
                }
                tp.pop();
                isVaArg = true;
                break;

            case TokenType::KEY_STRUCT:
            {
                tp.pop();
                std::unique_ptr<A1StatDecl> result = std::make_unique<A1StatDecl>();
                result->location = tkn.location;
                result->decl = parseStruct(tp, current, mod, isExported);
                return result;
            }

            case TokenType::KEY_ENUM:
            {
                tp.pop();
                std::unique_ptr<A1StatDecl> result = std::make_unique<A1StatDecl>();
                result->location = tkn.location;
                result->decl = parseEnum(tp, current, mod, isExported);
                return result;
            }

            default:
            {
                std::unique_ptr<A1StatDecl> result = std::make_unique<A1StatDecl>();
                result->location = tkn.location;
                std::unique_ptr<A1Type> vtype = mod.parseType(tp, current, arch);
                if (tp.match({TokenType::IDENTIFIER, TokenType::OP_SEMICOLON}) || tp.match({TokenType::IDENTIFIER, TokenType::OP_ASSIGN})) { // var declaration
                    std::unique_ptr<A1DeclVar> varDecl = parseVarDecl(tp, current, mod, std::move(vtype), isDefine, isConst, isVolatile, isExtern, isExported);
                    if (varDecl->init != nullptr && varDecl->init->objType != A1ExprType::LITERAL) {
                        throw std::runtime_error(std::format("E0646 variable should be initialized with constexpr at {}", getLocString(vtype->location))); // E0646
                    }
                    std::string nmValidity = mod.isNameUsable(varDecl->name, varDecl->location);
                    if (!nmValidity.empty()) {
                        throw std::runtime_error(nmValidity);
                    }
                    result->decl = std::move(varDecl);
                    return result;
                } else { // function declaration
                    result->decl = parseFunc(tp, current, mod, std::move(vtype), isVaArg, isExported);
                    return result;
                }
            }
        }
    }
    throw std::runtime_error(std::format("E0647 unexpected EOF while parsing toplevel at {}", getLocString(current.location))); // E0647
}

// calculate type size, return true if modified
bool A1Gen::completeType(A1Module& mod, A1Type& tgt) {
    bool modified = false;
    if (tgt.direct != nullptr) {
        modified = modified | completeType(mod, *tgt.direct);
    }
    for (auto& indirect : tgt.indirect) {
        modified = modified | completeType(mod, *indirect);
    }
    if (tgt.typeSize != -1) {
        return modified;
    }
    switch (tgt.objType) {
        case A1TypeType::ARRAY:
            if (tgt.direct->typeSize == 0) {
                throw std::runtime_error(std::format("E0701 cannot create array/slice of void type at {}", getLocString(tgt.location))); // E0701
            }
            if (tgt.direct->typeSize != -1) {
                tgt.typeSize = tgt.direct->typeSize * tgt.arrLen;
                tgt.typeAlign = tgt.direct->typeAlign;
                modified = true;
            }
            break;

        case A1TypeType::NAME:
            {
                A1DeclStruct* structNode = static_cast<A1DeclStruct*>(mod.findDeclaration(tgt.name, A1DeclType::STRUCT, false));
                if (structNode != nullptr && structNode->structSize != -1) {
                    tgt.typeSize = structNode->structSize;
                    tgt.typeAlign = structNode->structAlign;
                    modified = true;
                }
                A1DeclEnum* enumNode = static_cast<A1DeclEnum*>(mod.findDeclaration(tgt.name, A1DeclType::ENUM, false));
                if (enumNode != nullptr) {
                    tgt.typeSize = enumNode->enumSize;
                    tgt.typeAlign = enumNode->enumSize;
                    modified = true;
                }
                A1DeclTemplate* templateNode = static_cast<A1DeclTemplate*>(mod.findDeclaration(tgt.name, A1DeclType::TEMPLATE, false));
                if (structNode == nullptr && enumNode == nullptr && templateNode == nullptr) {
                    throw std::runtime_error(std::format("E0702 type {} not found at {}", tgt.name, getLocString(tgt.location))); // E0702
                }
            }
            break;

        case A1TypeType::FOREIGN:
            {
                A1DeclInclude* includeNode = static_cast<A1DeclInclude*>(mod.findDeclaration(tgt.incName, A1DeclType::INCLUDE, false));
                if (includeNode == nullptr) {
                    throw std::runtime_error(std::format("E0703 include name {} not found at {}", tgt.incName, getLocString(tgt.location))); // E0703
                }
                int index = findModule(includeNode->tgtPath);
                if (index == -1) {
                    throw std::runtime_error(std::format("E0704 included module {} not found at {}", includeNode->tgtPath, getLocString(tgt.location))); // E0704
                }
                A1DeclStruct* structNode = static_cast<A1DeclStruct*>(modules[index]->findDeclaration(tgt.name, A1DeclType::STRUCT, true));
                if (structNode != nullptr && structNode->structSize != -1) {
                    tgt.typeSize = structNode->structSize;
                    tgt.typeAlign = structNode->structAlign;
                    modified = true;
                }
                A1DeclEnum* enumNode = static_cast<A1DeclEnum*>(modules[index]->findDeclaration(tgt.name, A1DeclType::ENUM, true));
                if (enumNode != nullptr) {
                    tgt.typeSize = enumNode->enumSize;
                    tgt.typeAlign = enumNode->enumSize;
                    modified = true;
                }
                if (structNode == nullptr && enumNode == nullptr) {
                    throw std::runtime_error(std::format("E0705 type {}.{} not found at {}", tgt.incName, tgt.name, getLocString(tgt.location))); // E0705
                }
            }
            break;
    }
    return modified;
}

// complete struct size, return true if modified
bool A1Gen::completeStruct(A1Module& mod, A1DeclStruct& tgt) {
    bool isModified = false;
    for (auto& mem : tgt.memTypes) {
        isModified = isModified | completeType(mod, *mem);
    }
    for (auto& mem : tgt.memTypes) {
        if (mem->typeSize <= 0) {
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
    prt.Log(std::format("calculated struct size {} at {}", tgt.name, getLocString(tgt.location)), 1);
    return true;
}

// jump variable or function declaration
void jumpDecl(TokenProvider& tp, A1StatScope& current, A1Module& mod) {
    mod.parseType(tp, current, 1);
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
std::string A1Gen::parse(const std::string& path, int nameCut) {
    // check if already parsed
    int index = findModule(path);
    if (index != -1) {
        return std::format("E0706 module {} already parsed", path); // E0706
    }

    try {
        // find unique name
        std::string name = getFileName(path);
        std::string dir = getWorkingDir(path);
        if (nameCut > 0) { // cut name
            name = name.substr(nameCut);
        }
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
            for (auto& mod : modules) {
                if (mod->uname == uname) {
                    isIncluded = true;
                    break;
                }
            }
            if (isIncluded) {
                count++;
            } else {
                modules.push_back(std::make_unique<A1Module>(path, uname));
                index = static_cast<int>(modules.size() - 1);
            }
        }
        prt.Log(std::format("start parsing source {} as {}", path, modules[index]->uname), 2);

        // tokenize source
        std::string text = readFile(path);
        std::unique_ptr<std::vector<Token>> tokens = tokenize(text, path, index);
        TokenProvider tp = TokenProvider(*tokens);
        prt.Log(std::format("tokenized source {}", modules[index]->uname), 2);

        // source parsing pass1, parse structs and enums
        std::vector<int> reserved;
        while (tp.canPop(1)) {
            Token& tkn = tp.seek();
            switch (tkn.objType) {
                case TokenType::ORDER_INCLUDE: // include source, detect import cycle
                {
                    std::unique_ptr<A1StatDecl> node = parseTopLevel(tp, *modules[index]->code, *modules[index]);
                    if (node->decl->objType != A1DeclType::INCLUDE) {
                        return std::format("E0707 invalid include statement at {}", getLocString(tkn.location)); // E0707
                    }
                    A1DeclInclude* inc = static_cast<A1DeclInclude*>(node->decl.get());
                    int idx = findModule(inc->tgtPath);
                    if (idx == -1) { // not found, parse and add
                        std::string emsg = parse(inc->tgtPath, nameCut);
                        if (!emsg.empty()) {
                            return emsg;
                        }
                    } else if (!modules[idx]->isFinished) { // import cycle, return error message
                        return std::format("E0708 import cycle detected with {} at {}", inc->tgtPath, getLocString(tkn.location)); // E0708
                    } // else: already parsed, skip
                }
                break;

                case TokenType::ORDER_TYPEDEF: case TokenType::ORDER_TEMPLATE: case TokenType::ORDER_RAW_C: case TokenType::ORDER_RAW_IR: case TokenType::ORDER_DEFINE: // need to be parsed for deciding types
                    modules[index]->code->body.push_back(parseTopLevel(tp, *modules[index]->code, *modules[index]));
                    break;

                case TokenType::KEY_STRUCT: case TokenType::KEY_ENUM: case TokenType::OP_SEMICOLON: // definitions of structs and enums are parsed now
                    modules[index]->code->body.push_back(parseTopLevel(tp, *modules[index]->code, *modules[index]));
                    break;

                case TokenType::ORDER_EXPORT:
                    if (tp.match({TokenType::ORDER_EXPORT, TokenType::KEY_STRUCT}) || tp.match({TokenType::ORDER_EXPORT, TokenType::KEY_ENUM})) {
                        modules[index]->code->body.push_back(parseTopLevel(tp, *modules[index]->code, *modules[index]));
                    } else {
                        reserved.push_back(tp.pos);
                        tp.pop();
                    }
                    break;

                case TokenType::ORDER_CONST: case TokenType::ORDER_VOLATILE: case TokenType::ORDER_EXTERN: case TokenType::ORDER_VA_ARG: // order before variable or function declaration
                    reserved.push_back(tp.pos);
                    tp.pop();
                    break;

                default: // declarations of functions and variables are parsed later
                    reserved.push_back(tp.pos);
                    jumpDecl(tp, *modules[index]->code, *modules[index]);
                    break;
            }
        }
        prt.Log(std::format("pass1 finished for source {}", modules[index]->uname), 2);

        // source parsing pass2, static calculation of struct size
        bool isModified = true;
        while (isModified) {
            isModified = false;
            for (auto& node : modules[index]->code->body) {
                if (node->objType == A1StatType::DECL) {
                    A1StatDecl* dNode = static_cast<A1StatDecl*>(node.get());
                    if (dNode->decl->objType == A1DeclType::STRUCT) {
                        A1DeclStruct* sNode = static_cast<A1DeclStruct*>(dNode->decl.get());
                        if (sNode->structSize < 0) {
                            isModified = isModified | completeStruct(*modules[index], *sNode);
                        }
                    }
                }
            }
        }
        prt.Log(std::format("pass2 finished for source {}", modules[index]->uname), 2);

        // source parsing pass3, parse functions and variables
        tp.pos = 0;
        for (int i : reserved) {
            if (i >= tp.pos) { // parse only if not already parsed
                tp.pos = i;
                modules[index]->code->body.push_back(parseTopLevel(tp, *modules[index]->code, *modules[index]));
            }
        }
        prt.Log(std::format("pass3 finished for source {}", modules[index]->uname), 2);

        // finish parsing
        modules[index]->isFinished = true;
        prt.Log(std::format("finished parsing source {} as {}", path, modules[index]->uname), 3);
    } catch (std::exception& e) {
        return e.what();
    }
    return "";
}
