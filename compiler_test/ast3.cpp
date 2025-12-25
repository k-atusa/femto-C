#include "ast3.h"
#include "ast2.h"

/*
lowering 주요 변환규칙
- A3의 배열은 값 타입처럼 표현되나 실제로는 포인터로 취급 (할당 같은 작업에서 포인터 memcpy 등 처리)
- 일정 크기 이상(bigCopyAlert)의 메모리 작업은 경고
- 구현 순서 : ast2의 선언만 위상 정렬로 추가 -> 함수 본문 채우기
- 일부 expr/stat은 preStats가 필요 : A3Gen의 버퍼를 사용하여 사전실행문을 채우고 최종적으로 통합

- literal_data는 미리 변수를 선언하고 초기화하는 stat으로 변환 -ok
- 문자열 슬라이스는 문자열 포인터를 받아 슬라이스 구조체를 생성 -ok
- 삼항연산자 안에서 prestat 발생 시 if-else로 변환 -ok
- 논리연산자 안에서 prestat 발생 시 단축평가 고려하여 if-else로 변환 -ok
- rvalue에 & 취하는 경우 임시변수 할당 -ok

- 가변인자함수는 void*[] (void*들의 슬라이스)로 포장하여 전달 -ok
- 가변인자함수 인자로 값 타입이 들어간다면 임시변수 선언하고 복사 후 해당 변수의 주소를 취해 넣을 것 -ok
- 함수 호출 시 인자에 함수 호출을 포함하는 side-effect 있는 인자는 순서에 맞춰서 미리 임시변수에 할당하고 넣을 것 -ok
- 함수 호출 시 반환값이 배열이라면 반환값을 받을 임시변수를 선언하고 호출 마지막 인자로 전달 -ok

- 배열 대입 시 memcpy로 복사 (이름 = 배열반환함수/리터럴 이면 복사생략) -ok
- 함수 본문에서, 인자로 들어오는 배열을 복사
- 함수 본문에서, 반환값이 배열이라면 호출 마지막 인자에 복사
- 함수 반환 리터럴 RVO
*/

// helper functions
std::unique_ptr<A3ExprLiteral> mkLiteral(Literal v, A3Type* t, Location l) {
    auto expr = std::make_unique<A3ExprLiteral>();
    expr->objType = A3ExprType::LITERAL;
    expr->exprType = t;
    expr->location = l;
    expr->value = v;
    return expr;
}

bool isTypeEqual(A3Type* a, A3Type* b) {
    if (a == nullptr ^ b == nullptr) return false;
    if (a == nullptr && b == nullptr) return true;
    if (a->typeSize != b->typeSize || a->typeAlign != b->typeAlign) return false;
    if (a->objType == A3TypeType::ARRAY) {
        if (a->arrLen != b->arrLen) return false;
    } else if (a->objType == A3TypeType::FUNCTION) {
        if (a->indirect.size() != b->indirect.size()) return false;
    }
    if (a->objType == A3TypeType::ARRAY || a->objType == A3TypeType::POINTER || a->objType == A3TypeType::SLICE || a->objType == A3TypeType::FUNCTION) {
        if (a->objType != b->objType) return false;
    } else if (a->objType == A3TypeType::PRIMITIVE || a->objType == A3TypeType::STRUCT) {
        if (a->objType != b->objType || a->name != b->name) return false;
    }
    if (!isTypeEqual(a->direct.get(), b->direct.get())) return false;
    for (size_t i = 0; i < a->indirect.size(); i++) {
        if (!isTypeEqual(a->indirect[i].get(), b->indirect[i].get())) return false;
    }
    return true;
}

A3Type* getArrayDirect(A3Type* t) {
    while (t->objType == A3TypeType::ARRAY) {
        t = t->direct.get();
    }
    return t;
}

int64_t getArrayLen(A3Type* t) {
    int64_t sz = 1;
    while (t->objType == A3TypeType::ARRAY) {
        sz *= t->arrLen;
        t = t->direct.get();
    }
    return sz;
}

std::unique_ptr<A3Expr> createArraySizeExpr(A3Type* t, A3Type* intType, Location l) {
    // sizeof
    auto sz = std::make_unique<A3ExprOperation>();
    sz->objType = A3ExprType::OPERATION;
    sz->subType = A3ExprOpType::U_SIZEOF;
    sz->typeOperand = getArrayDirect(t)->clone();
    sz->exprType = intType;
    sz->location = l;

    // mul
    auto mul = std::make_unique<A3ExprOperation>();
    mul->objType = A3ExprType::OPERATION;
    mul->subType = A3ExprOpType::B_MUL;
    mul->operand0 = mkLiteral(Literal(getArrayLen(t)), intType, l);
    mul->operand1 = std::move(sz);
    mul->exprType = intType;
    mul->location = l;
    return mul;
}

void checkArrayAccess(A3Type* arrType, A3Expr* st, A3Expr* ed, bool isSlicing, std::string loc) {
    bool stChk = false;
    bool edChk = false;
    bool arrChk = false;
    int64_t stVal = -1;
    int64_t edVal = -1;
    int64_t arrVal = -1;
    if (st != nullptr && st->objType == A3ExprType::LITERAL) {
        auto& lit = ((A3ExprLiteral*)st)->value;
        if (lit.objType == LiteralType::INT) {
            stChk = true;
            stVal = std::get<int64_t>(lit.value);
        }
    }
    if (ed != nullptr && ed->objType == A3ExprType::LITERAL) {
        auto& lit = ((A3ExprLiteral*)ed)->value;
        if (lit.objType == LiteralType::INT) {
            edChk = true;
            edVal = std::get<int64_t>(lit.value);
        }
    }
    if (arrType->objType == A3TypeType::ARRAY) {
        arrChk = true;
        arrVal = arrType->arrLen;
    }
    if (isSlicing) { // check bounds for slicing
        if (arrChk && ((stChk && stVal > arrVal) || (edChk && edVal > arrVal)))
            throw std::runtime_error(std::format("E1901 slicing out of array bound ({}[{}:{}]) at {}", arrVal, stVal, edVal, loc)); // E1901
        if ((stChk && stVal < 0) || (edChk && edVal < 0))
            throw std::runtime_error(std::format("E1902 negative index for slicing ([{}:{}]) at {}", stVal, edVal, loc)); // E1902
        if (stChk && edChk && stVal > edVal)
            throw std::runtime_error(std::format("E1903 invalid range for slicing ([{}:{}]) at {}", stVal, edVal, loc)); // E1903
    } else { // check bounds for indexing
        if (arrChk && (stChk && stVal >= arrVal))
            throw std::runtime_error(std::format("E1904 indexing out of array bound ({}[{}]) at {}", arrVal, stVal, loc)); // E1904
        if (stChk && stVal < 0)
            throw std::runtime_error(std::format("E1905 negative index for indexing ([{}]) at {}", stVal, loc)); // E1905
    }
}

bool hasExprSideEffect(A3Expr* e) {
    switch (e->objType) {
        case A3ExprType::LITERAL: case A3ExprType::VAR_NAME: case A3ExprType::FUNC_NAME:
            return false;
        case A3ExprType::OPERATION:
        {
            auto op = (A3ExprOperation*)e;
            if (op->operand0 != nullptr && hasExprSideEffect(op->operand0.get())) return true;
            if (op->operand1 != nullptr && hasExprSideEffect(op->operand1.get())) return true;
            if (op->operand2 != nullptr && hasExprSideEffect(op->operand2.get())) return true;
            return false;
        }
        default: // calls
            return true;
    }
}

// initialize type pool
std::unique_ptr<A3Type> getPrimitiveType(const std::string& name, int size, int align) {
    auto type = std::make_unique<A3Type>();
    type->objType = A3TypeType::PRIMITIVE;
    type->name = name;
    type->typeSize = size;
    type->typeAlign = align;

    // init additional fields
    type->location = Location();
    type->arrLen = -1;
    type->direct = nullptr;
    type->indirect.clear();
    return type;
}

void A3Gen::initTypePool() {
    // Add integer types
    typePool.push_back(getPrimitiveType("int", arch, arch)); // typePool[0]
    typePool.push_back(getPrimitiveType("i8", 1, 1)); // typePool[1]
    typePool.push_back(getPrimitiveType("i16", 2, 2)); // typePool[2]
    typePool.push_back(getPrimitiveType("i32", 4, 4)); // typePool[3]
    typePool.push_back(getPrimitiveType("i64", 8, 8)); // typePool[4]

    // Add unsigned integer types
    typePool.push_back(getPrimitiveType("uint", arch, arch)); // typePool[5]
    typePool.push_back(getPrimitiveType("u8", 1, 1)); // typePool[6]
    typePool.push_back(getPrimitiveType("u16", 2, 2)); // typePool[7]
    typePool.push_back(getPrimitiveType("u32", 4, 4)); // typePool[8]
    typePool.push_back(getPrimitiveType("u64", 8, 8)); // typePool[9]

    // Add floating-point types
    typePool.push_back(getPrimitiveType("f32", 4, 4)); // typePool[10]
    typePool.push_back(getPrimitiveType("f64", 8, 8)); // typePool[11]

    // Add boolean type
    typePool.push_back(getPrimitiveType("bool", 1, 1)); // typePool[12]

    // Add void type
    typePool.push_back(getPrimitiveType("void", 0, 1)); // typePool[13]

    // Add void* type
    auto voidPtrType = std::make_unique<A3Type>();
    voidPtrType->objType = A3TypeType::POINTER;
    voidPtrType->name = "*";
    voidPtrType->typeSize = arch; 
    voidPtrType->typeAlign = arch;
    voidPtrType->direct = getPrimitiveType("void", 0, 1);
    typePool.push_back(std::move(voidPtrType)); // typePool[14]

    // Add u8[] type
    auto u8ArrayType = std::make_unique<A3Type>();
    u8ArrayType->objType = A3TypeType::SLICE;
    u8ArrayType->name = "u8";
    u8ArrayType->typeSize = arch * 2;
    u8ArrayType->typeAlign = arch;
    u8ArrayType->direct = getPrimitiveType("u8", 1, 1);
    typePool.push_back(std::move(u8ArrayType)); // typePool[15]
}

// find type in type pool
int A3Gen::findType(A3Type* t) {
    for (size_t i = 0; i < typePool.size(); i++) {
        if (isTypeEqual(typePool[i].get(), t)) return i;
    }
    return -1;
}

// generate temporary variable declaration, returns variable name
std::string A3Gen::genTempVar(A3Type* t, Location l) {
    // generate temp name, check copy size
    std::string tName = genName();
    if (t->typeSize >= bigCopyAlert) {
        prt.Log(std::format("W1906 large temporary variable ({} bytes) at {}", t->typeSize, getLocString(l)), 5); // W1906
    }

    // register type
    int tIdx = findType(t);
    if (tIdx == -1) {
        tIdx = typePool.size();
        typePool.push_back(t->clone());
    }
    t = typePool[tIdx].get();

    // decl
    auto decl = std::make_unique<A3DeclVar>();
    decl->objType = A3DeclType::VAR;
    decl->location = l;
    decl->name = tName;
    decl->uid = uidCount++;
    decl->type = t->clone();
    decl->isConst = false;
    decl->isVolatile = false;
    scopes.back()->nameMap[decl->uid] = decl.get();

    auto statDecl = std::make_unique<A3StatDecl>();
    statDecl->objType = A3StatType::DECL;
    statDecl->location = l;
    statDecl->uid = uidCount++;
    statDecl->decl = std::move(decl);
    statBuf.push_back(std::move(statDecl));
    return tName;
}

// generate temporary variable assign, returns variable name
std::string A3Gen::setTempVar(A3Type* t, std::unique_ptr<A3Expr> v) {
    std::string tName = genTempVar(t, v->location);
    if (!isTypeEqual(t, v->exprType)) {
        throw std::runtime_error(std::format("E1907 tempVar type mismatch at {}", getLocString(v->location))); // E1907
    }
    auto nameRef = getTempVar(tName, v->location);
    statBuf.push_back(genAssignStat(std::move(nameRef), std::move(v)));
    return tName;
}

std::unique_ptr<A3ExprName> A3Gen::getTempVar(std::string name, Location l) {
    // find var
    auto var = findVar(name);
    if (!var) throw std::runtime_error(std::format("E1908 undefined variable {} at {}", name, getLocString(l))); // E1908

    // create name expr
    auto nameExpr = std::make_unique<A3ExprName>();
    nameExpr->objType = A3ExprType::VAR_NAME;
    nameExpr->decl = var;
    nameExpr->location = l;
    nameExpr->exprType = var->type.get();
    return nameExpr;
}

// generate reference to variable
std::unique_ptr<A3ExprOperation> A3Gen::refVar(std::string name, Location l) {
    // take address of var
    auto addrOp = std::make_unique<A3ExprOperation>();
    addrOp->objType = A3ExprType::OPERATION;
    addrOp->subType = A3ExprOpType::U_REF;
    addrOp->operand0 = getTempVar(name, l);
    addrOp->location = l;

    // set type of &temp (ptr)
    auto ptrType = std::make_unique<A3Type>();
    ptrType->objType = A3TypeType::POINTER;
    ptrType->name = "*";
    ptrType->typeSize = arch;
    ptrType->typeAlign = arch;
    ptrType->direct = addrOp->operand0->exprType->clone();

    // register type
    int pIdx = findType(ptrType.get());
    if (pIdx == -1) {
        pIdx = typePool.size();
        typePool.push_back(std::move(ptrType));
    }
    addrOp->exprType = typePool[pIdx].get();
    return addrOp;
}

// generate dereference to variable
std::unique_ptr<A3ExprOperation> A3Gen::derefVar(std::string name, Location l) {
    // take address of var
    auto addrOp = std::make_unique<A3ExprOperation>();
    addrOp->objType = A3ExprType::OPERATION;
    addrOp->subType = A3ExprOpType::U_DEREF;
    addrOp->operand0 = getTempVar(name, l);
    addrOp->location = l;

    // set type of *temp, register type
    auto varType = addrOp->operand0->exprType->direct.get();
    int vIdx = findType(varType);
    if (vIdx == -1) {
        vIdx = typePool.size();
        typePool.push_back(varType->clone());
    }
    addrOp->exprType = typePool[vIdx].get();
    return addrOp;
}

// generate assignment statement
std::unique_ptr<A3StatAssign> A3Gen::genAssignStat(std::unique_ptr<A3Expr> left, std::unique_ptr<A3Expr> right) {
    auto assign = std::make_unique<A3StatAssign>();
    assign->objType = A3StatType::ASSIGN;
    assign->location = left->location;
    assign->uid = uidCount++;
    assign->left = std::move(left);
    assign->right = std::move(right);
    return assign;
}

// calculate jumps in a context
int64_t A3Gen::countJumps(A2StatType tp) {
    int64_t count = 0;
    for (int i = scopes.size() - 1; i >= 0; i--) {
        if (scopes[i]->scopeLbl != nullptr) count++;
        if (scopes[i]->whileTgt != nullptr) break;
    }
    if (tp == A2StatType::BREAK) {
        return count;
    } else if (tp == A2StatType::CONTINUE) {
        return count - 1;
    } else {
        return scopes.size() + 1;
    }
}

// main lowering function for types
std::unique_ptr<A3Type> A3Gen::lowerType(A2Type* t) {
    std::unique_ptr<A3Type> newType = std::make_unique<A3Type>();
    newType->location = t->location;
    newType->typeSize = t->typeSize;
    newType->typeAlign = t->typeAlign;
    newType->arrLen = -1;

    switch (t->objType) {
        case A2TypeType::PRIMITIVE:
            newType->objType = A3TypeType::PRIMITIVE;
            newType->name = t->name;
            break;

        case A2TypeType::POINTER:
            newType->objType = A3TypeType::POINTER;
            newType->name = "*";
            newType->direct = lowerType(t->direct.get());
            break;

        case A2TypeType::ARRAY:
            newType->objType = A3TypeType::ARRAY;
            newType->name = t->name;
            newType->direct = lowerType(t->direct.get());
            newType->arrLen = t->arrLen;
            break;

        case A2TypeType::SLICE:
            newType->objType = A3TypeType::SLICE;
            newType->name = t->name;
            newType->direct = lowerType(t->direct.get());
            break;

        case A2TypeType::FUNCTION:
            newType->objType = A3TypeType::FUNCTION;
            newType->name = t->name;
            newType->direct = lowerType(t->direct.get());
            for (auto& indirect : t->indirect) {
                newType->indirect.push_back(lowerType(indirect.get()));
            }
            if (newType->direct->objType == A3TypeType::ARRAY) { // if ret is array, last param is arr to copy ret_value
                newType->indirect.push_back(newType->direct->clone()); // actual direct type is void, but pass array for checking
            }
            break;

        case A2TypeType::STRUCT:
        {
            newType->objType = A3TypeType::STRUCT;
            A2Decl* decl = ast2->modules[ast2->findModule(t->modUname)]->nameMap[t->name];
            if (decl->objType != A2DeclType::STRUCT) {
                throw std::runtime_error(std::format("E2001 invalid struct name {}.{} at {}", t->modUname, t->name, getLocString(t->location)));
            }
            newType->name = scopes[0]->nameMap[decl->uid]->name;
        }
        break;

        case A2TypeType::ENUM:
            newType->objType = A3TypeType::PRIMITIVE;
            if (t->typeSize == 1) {
                newType->name = "i8";
            } else if (t->typeSize == 2) {
                newType->name = "i16";
            } else if (t->typeSize == 4) {
                newType->name = "i32";
            } else if (t->typeSize == 8) {
                newType->name = "i64";
            } else {
                throw std::runtime_error(std::format("E2002 invalid enum size {} at {}", t->typeSize, getLocString(t->location)));
            }
            break;

        default:
            throw std::runtime_error(std::format("E2003 invalid type {} at {}", (int)t->objType, getLocString(t->location)));
    }
    return newType;
}

// main lowering function for expressions
std::unique_ptr<A3Expr> A3Gen::lowerExpr(A2Expr* e, std::string assignVarName) {
    if (!e) return nullptr;
    std::unique_ptr<A3Expr> res = nullptr;

    switch (e->objType) {
        case A2ExprType::LITERAL:
        {
            auto lit = (A2ExprLiteral*)e;
            if (lit->value.objType == LiteralType::STRING) { // string literals
                res = lowerExprLitString(lit);
            } else { // normal literals
                auto r = std::make_unique<A3ExprLiteral>();
                r->objType = A3ExprType::LITERAL;
                r->value = lit->value;
                res = std::move(r);
            }
        }
        break;

        case A2ExprType::VAR_NAME:
        {
            auto name = (A2ExprName*)e;
            auto r = std::make_unique<A3ExprName>();
            r->objType = A3ExprType::VAR_NAME;
            
            A3DeclVar* vDecl = findVar(name->decl->uid);
            if (vDecl == nullptr) throw std::runtime_error(std::format("E2101 variable {} ({}) not found at {}", name->decl->name, name->decl->uid, getLocString(name->location)));
            r->decl = vDecl;
            res = std::move(r);
        }
        break;

        case A2ExprType::FUNC_NAME:
        {
            auto name = (A2ExprName*)e;
            auto r = std::make_unique<A3ExprName>();
            r->objType = A3ExprType::FUNC_NAME;
            
            A3Decl* decl = findDecl(name->decl->uid);
            if (decl == nullptr || decl->objType != A3DeclType::FUNC) throw std::runtime_error(std::format("E2102 function {} ({}) not found at {}", name->decl->name, name->decl->uid, getLocString(name->location)));
            r->decl = decl;
            res = std::move(r);
        }
        break;

        case A2ExprType::LITERAL_DATA:
        {
            std::string setName = assignVarName;
            return lowerExprLitData((A2ExprLiteralData*)e, &setName);
        }

        case A2ExprType::OPERATION:
            return lowerExprOp((A2ExprOperation*)e);

        case A2ExprType::FUNC_CALL:
        {
            auto call = (A2ExprFuncCall*)e;
            auto decl = findDecl(call->func->uid);
            if (!decl || decl->objType != A3DeclType::FUNC) throw std::runtime_error(std::format("E2103 function {} not found at {}", call->func->name, getLocString(call->location)));
            auto fDecl = (A3DeclFunc*)decl;
            
            // check return type, generate param array
            bool isRetArray = (fDecl->type->direct->objType == A3TypeType::ARRAY);
            std::string retName = assignVarName;
            auto a3Args = lowerExprCall(fDecl->type.get(), call->args, fDecl->isVaArg, isRetArray, &retName);

            // register return type
            int idx = findType(fDecl->type->direct.get());
            if (idx == -1) {
                idx = typePool.size();
                typePool.push_back(fDecl->type->direct->clone());
            }
            auto retType = typePool[idx].get();

            // create call expr
            auto resCall = std::make_unique<A3ExprFuncCall>();
            resCall->objType = A3ExprType::FUNC_CALL;
            resCall->location = e->location;
            resCall->func = fDecl;
            resCall->args = std::move(a3Args);

            if (isRetArray) {
                resCall->exprType = typePool[14].get(); // call returns void
                auto statExpr = std::make_unique<A3StatExpr>(); // add call preStat
                statExpr->objType = A3StatType::EXPR;
                statExpr->location = e->location;
                statExpr->uid = uidCount++;
                statExpr->expr = std::move(resCall);
                statBuf.push_back(std::move(statExpr));
                res = getTempVar(retName, e->location); // var_use of retName
            } else {
                resCall->exprType = retType;
                res = std::move(resCall);
            }
        }
        break;

        case A2ExprType::FPTR_CALL:
        {
            // convert fptr expr
            auto call = (A2ExprFptrCall*)e;
            std::unique_ptr<A3Type> fType = lowerType(call->fptr->exprType);
            std::unique_ptr<A3Expr> fExpr = lowerExpr(call->fptr.get(), "");

            // check return type, generate param array
            bool isRetArray = (fType->direct->objType == A3TypeType::ARRAY);
            std::string retName = assignVarName;
            auto a3Args = lowerExprCall(fType.get(), call->args, false, isRetArray, &retName);

            // register return type
            int idx = findType(fType->direct.get());
            if (idx == -1) {
                idx = typePool.size();
                typePool.push_back(fType->direct->clone());
            }
            auto retType = typePool[idx].get();

            // create call expr
            auto resCall = std::make_unique<A3ExprFptrCall>();
            resCall->objType = A3ExprType::FPTR_CALL;
            resCall->location = e->location;
            resCall->fptr = std::move(fExpr);
            resCall->args = std::move(a3Args);

            if (isRetArray) {
                resCall->exprType = typePool[14].get(); // call returns void
                auto statExpr = std::make_unique<A3StatExpr>(); // add call preStat
                statExpr->objType = A3StatType::EXPR;
                statExpr->location = e->location;
                statExpr->uid = uidCount++;
                statExpr->expr = std::move(resCall);
                statBuf.push_back(std::move(statExpr));
                res = getTempVar(retName, e->location); // var_use of retName
            } else {
                resCall->exprType = retType;
                res = std::move(resCall);
            }
        }
        break;

        default:
            throw std::runtime_error("E2104 invalid expression type"); // E2104
    }

    if (res) {
        res->location = e->location;
        auto t = lowerType(e->exprType);
        int idx = findType(t.get());
        if (idx == -1) {
            idx = typePool.size();
            typePool.push_back(std::move(t));
        }
        res->exprType = typePool[idx].get();
    }
    return res;
}

// lowering expression string literal
std::unique_ptr<A3Expr> A3Gen::lowerExprLitString(A2ExprLiteral* l) {
    if (l->exprType->objType == A2TypeType::SLICE) { // str slice -> make("..", sz)
        auto r = std::make_unique<A3ExprOperation>();
        r->objType = A3ExprType::OPERATION;
        r->subType = A3ExprOpType::B_MAKE;

        // string literal (ptr)
        auto ptr = std::make_unique<A3ExprLiteral>();
        ptr->objType = A3ExprType::LITERAL;
        ptr->value = l->value;
        
        auto ptrType = std::make_unique<A3Type>();
        ptrType->objType = A3TypeType::POINTER;
        ptrType->name = "*";
        ptrType->typeSize = arch;
        ptrType->typeAlign = arch;
        ptrType->direct = getPrimitiveType("u8", 1, 1);
        
        int idx = findType(ptrType.get());
        if (idx == -1) {
            idx = typePool.size();
            typePool.push_back(std::move(ptrType));
        }
        ptr->exprType = typePool[idx].get();
        r->operand0 = std::move(ptr);

        // len literal
        r->operand1 = mkLiteral(Literal((int64_t)std::get<std::string>(l->value.value).size()), typePool[0].get(), l->location);
        return r;

    } else { // string array or string pointer
        auto t = lowerType(l->exprType);
        int idx = findType(t.get());
        if (idx == -1) {
            idx = typePool.size();
            typePool.push_back(std::move(t));
        }
        return mkLiteral(l->value, typePool[idx].get(), l->location);
    }
}

// lowering expression literal_data
bool isZeroLiteral(A2Expr* e) { // helper to check if expression is zero literal
    if (e->objType != A2ExprType::LITERAL) return false;
    A2ExprLiteral* lit = static_cast<A2ExprLiteral*>(e);
    if (lit->value.objType == LiteralType::INT) return std::get<int64_t>(lit->value.value) == 0;
    if (lit->value.objType == LiteralType::BOOL) return std::get<int64_t>(lit->value.value) == 0;
    if (lit->value.objType == LiteralType::NPTR) return true;
    if (lit->value.objType == LiteralType::FLOAT) return std::get<double>(lit->value.value) == 0.0;
    return false;
}

std::unique_ptr<A3Expr> A3Gen::lowerExprLitData(A2ExprLiteralData* e, std::string* setName) {
    // 1. create temp var, 2. register type
    auto type = lowerType(e->exprType);
    if (*setName == "") {
        *setName = genTempVar(type.get(), e->location);
    }
    auto typePtr = findVar(*setName)->type.get();

    if (typePtr->objType == A3TypeType::ARRAY) { // 3-1. array initialization
        // memset (memset src is always 0)
        auto memSet = std::make_unique<A3StatMem>();
        memSet->objType = A3StatType::MEMSET;
        memSet->location = e->location;
        memSet->uid = uidCount++;
        memSet->dst = getTempVar(*setName, e->location); // dst is temp var
        memSet->size = createArraySizeExpr(typePtr, typePool[0].get(), e->location); // size: arrlen * sizeof(direct), sizeHint : typeSize
        memSet->sizeHint = typePtr->typeSize;
        statBuf.push_back(std::move(memSet)); // push back declaration

        // 4-1. set elements
        for (size_t i = 0; i < e->elements.size(); i++) {
            if (isZeroLiteral(e->elements[i].get())) continue;

            // left: temp[i]
            auto idxOp = std::make_unique<A3ExprOperation>();
            idxOp->objType = A3ExprType::OPERATION;
            idxOp->subType = A3ExprOpType::B_INDEX;
            idxOp->location = e->location;
            idxOp->operand0 = getTempVar(*setName, e->location); // arrRef
            idxOp->operand1 = mkLiteral(Literal((int64_t)i), typePool[0].get(), e->location);
            idxOp->exprType = typePtr->direct.get(); // array element type
            
            // push back assignment
            statBuf.push_back(genAssignStat(std::move(idxOp), lowerExpr(e->elements[i].get(), "")));
        }

    } else if (typePtr->objType == A3TypeType::STRUCT) { // 3-2. struct initialization
        for (size_t i = 0; i < e->elements.size(); i++) { // 4-2. set elements
            // left: temp.i
            auto dotOp = std::make_unique<A3ExprOperation>();
            dotOp->objType = A3ExprType::OPERATION;
            dotOp->subType = A3ExprOpType::B_DOT;
            dotOp->location = e->location;
            dotOp->accessPos = i;
            dotOp->operand0 = getTempVar(*setName, e->location); // structRef

            // infer element type from source A2Expr
            auto elemType = lowerType(e->elements[i]->exprType);
            int eIdx = findType(elemType.get());
            if (eIdx == -1) {
                eIdx = typePool.size();
                typePool.push_back(std::move(elemType));
            }
            dotOp->exprType = typePool[eIdx].get();

            // push back assignment
            statBuf.push_back(genAssignStat(std::move(dotOp), lowerExpr(e->elements[i].get(), "")));
        }
    }
    return getTempVar(*setName, e->location); // 5. return name_use
}

// lowering expression operation
std::unique_ptr<A3Expr> A3Gen::lowerExprOp(A2ExprOperation* e) {
    bool lower0 = false;
    bool lower1 = false;
    auto newOp = std::make_unique<A3ExprOperation>();
    newOp->objType = A3ExprType::OPERATION;
    newOp->location = e->location;

    switch (e->subType) {
        // operations having same logic
        case A2ExprOpType::B_DOT: newOp->subType = A3ExprOpType::B_DOT; lower0 = true; lower1 = true; break;
        case A2ExprOpType::B_ARROW: newOp->subType = A3ExprOpType::B_ARROW; lower0 = true; lower1 = true; break;
        case A2ExprOpType::B_INDEX: newOp->subType = A3ExprOpType::B_INDEX; lower0 = true; lower1 = true; break;
        
        case A2ExprOpType::U_PLUS: newOp->subType = A3ExprOpType::U_PLUS; lower0 = true; break;
        case A2ExprOpType::U_MINUS: newOp->subType = A3ExprOpType::U_MINUS; lower0 = true; break;
        case A2ExprOpType::U_BIT_NOT: newOp->subType = A3ExprOpType::U_BIT_NOT; lower0 = true; break;
        case A2ExprOpType::U_DEREF: newOp->subType = A3ExprOpType::U_DEREF; lower0 = true; break;

        case A2ExprOpType::B_MUL: newOp->subType = A3ExprOpType::B_MUL; lower0 = true; lower1 = true; break;
        case A2ExprOpType::B_DIV: newOp->subType = A3ExprOpType::B_DIV; lower0 = true; lower1 = true; break;
        case A2ExprOpType::B_MOD: newOp->subType = A3ExprOpType::B_MOD; lower0 = true; lower1 = true; break;
        
        case A2ExprOpType::B_SHL: newOp->subType = A3ExprOpType::B_SHL; lower0 = true; lower1 = true; break;
        case A2ExprOpType::B_SHR: newOp->subType = A3ExprOpType::B_SHR; lower0 = true; lower1 = true; break;
        
        case A2ExprOpType::B_LT: newOp->subType = A3ExprOpType::B_LT; lower0 = true; lower1 = true; break;
        case A2ExprOpType::B_LE: newOp->subType = A3ExprOpType::B_LE; lower0 = true; lower1 = true; break;
        case A2ExprOpType::B_GT: newOp->subType = A3ExprOpType::B_GT; lower0 = true; lower1 = true; break;
        case A2ExprOpType::B_GE: newOp->subType = A3ExprOpType::B_GE; lower0 = true; lower1 = true; break;
        case A2ExprOpType::B_EQ: newOp->subType = A3ExprOpType::B_EQ; lower0 = true; lower1 = true; break;
        case A2ExprOpType::B_NE: newOp->subType = A3ExprOpType::B_NE; lower0 = true; lower1 = true; break;
        
        case A2ExprOpType::B_BIT_AND: newOp->subType = A3ExprOpType::B_BIT_AND; lower0 = true; lower1 = true; break;
        case A2ExprOpType::B_BIT_XOR: newOp->subType = A3ExprOpType::B_BIT_XOR; lower0 = true; lower1 = true; break;
        case A2ExprOpType::B_BIT_OR: newOp->subType = A3ExprOpType::B_BIT_OR; lower0 = true; lower1 = true; break;
        case A2ExprOpType::U_LOGIC_NOT: newOp->subType = A3ExprOpType::U_LOGIC_NOT; lower0 = true; break;
        case A2ExprOpType::B_MAKE: newOp->subType = A3ExprOpType::B_MAKE; lower0 = true; break;

        // number operations + pointer arithmetics
        case A2ExprOpType::B_ADD:
            if (e->operand0->exprType->objType == A2TypeType::POINTER || e->operand1->exprType->objType == A2TypeType::POINTER) {
                newOp->subType = A3ExprOpType::B_PTR_ADD;
            } else {
                newOp->subType = A3ExprOpType::B_ADD;
            }
            lower0 = true;
            lower1 = true;
            break;
        case A2ExprOpType::B_SUB:
            if (e->operand0->exprType->objType == A2TypeType::POINTER || e->operand1->exprType->objType == A2TypeType::POINTER) {
                newOp->subType = A3ExprOpType::B_PTR_SUB;
            } else {
                newOp->subType = A3ExprOpType::B_SUB;
            }
            lower0 = true;
            lower1 = true;
            break;

        // cast type, register
        case A2ExprOpType::B_CAST:
        {
            newOp->subType = A3ExprOpType::B_CAST;
            newOp->typeOperand = lowerType(e->typeOperand.get());
            lower0 = true;
        }
        break;

        // sizeof(type)
        case A2ExprOpType::U_SIZEOF:
            newOp->subType = A3ExprOpType::U_SIZEOF;
            newOp->typeOperand = lowerType(e->typeOperand.get());
            break;

        // len(slice) or len(array)
        case A2ExprOpType::U_LEN:
            newOp->subType = A3ExprOpType::U_LEN;
            if (e->operand0->exprType->objType == A2TypeType::ARRAY) {
                newOp->operand0 = mkLiteral(Literal((int64_t)e->operand0->exprType->arrLen), typePool[0].get(), e->location);
            } else {
                lower0 = true;
            }
            break;

        // make(&arr[st], ed-st)
        case A2ExprOpType::T_SLICE:
            return lowerExprOpSlice(e);

        // conditional operation can have preStats
        case A2ExprOpType::T_COND: case A2ExprOpType::B_LOGIC_AND: case A2ExprOpType::B_LOGIC_OR:
            return lowerExprOpCond(e);

        // reference operation
        case A2ExprOpType::U_REF:
            if (e->operand0->isLvalue) {
                newOp->subType = A3ExprOpType::U_REF;
                lower0 = true;
            } else {
                auto val = lowerExpr(e->operand0.get(), "");
                if (val->objType == A3ExprType::VAR_NAME) { // var_name has address
                    newOp->subType = A3ExprOpType::U_REF;
                    newOp->operand0 = std::move(val);
                } else { // make temporary variable
                    auto t = val->exprType;
                    std::string tName = setTempVar(t, std::move(val));
                    newOp->subType = A3ExprOpType::U_REF;
                    newOp->operand0 = refVar(tName, e->location);
                }
            }
            break;

        default:
            throw std::runtime_error(std::format("E2105 unsupported operation in ast3 {}", (int)e->subType)); // E2105
    }

    // set rest members of operation node
    if (lower0) newOp->operand0 = lowerExpr(e->operand0.get(), "");
    if (lower1) newOp->operand1 = lowerExpr(e->operand1.get(), "");
    auto t = lowerType(e->exprType);
    int idx = findType(t.get());
    if (idx == -1) {
        idx = typePool.size();
        typePool.push_back(std::move(t));
    }
    newOp->exprType = typePool[idx].get();
    if (newOp->subType == A3ExprOpType::B_INDEX) // check bounds
        checkArrayAccess(newOp->operand0->exprType, newOp->operand1.get(), nullptr, false, getLocString(e->location));
    if (newOp->subType == A3ExprOpType::B_DOT || newOp->subType == A3ExprOpType::B_ARROW) // set struct access position
        newOp->accessPos = e->accessPos;
    return newOp;
}

// lowering slicing operation
std::unique_ptr<A3Expr> A3Gen::lowerExprOpSlice(A2ExprOperation* e) {
    // 1. arr, st, ed expressions
    auto arrExpr = lowerExpr(e->operand0.get(), "");
    std::unique_ptr<A3Expr> startExpr;
    std::unique_ptr<A3Expr> endExpr;

    if (e->operand1) { // 1-1-1. cast startExpr to int
        startExpr = lowerExpr(e->operand1.get(), "");
        if (startExpr->exprType->objType != A3TypeType::PRIMITIVE || startExpr->exprType->name != "int") {
            if (startExpr->objType == A3ExprType::LITERAL) {
                startExpr->exprType = typePool[0].get(); // simple cast for literal
            } else {
                std::unique_ptr<A3ExprOperation> castOp = std::make_unique<A3ExprOperation>();
                castOp->objType = A3ExprType::OPERATION;
                castOp->location = e->location;
                castOp->subType = A3ExprOpType::B_CAST;
                castOp->operand0 = std::move(startExpr);
                castOp->typeOperand = typePool[0]->clone();
                castOp->exprType = typePool[0].get();
                startExpr = std::move(castOp);
            }
        }
    } else { // 1-2. nullptr -> literal 0
        startExpr = mkLiteral(Literal((int64_t)0), typePool[0].get(), e->location);
    }

    if (e->operand2) { // 1-1-2. cast endExpr to int
        endExpr = lowerExpr(e->operand2.get(), "");
        if (endExpr->exprType->objType != A3TypeType::PRIMITIVE || endExpr->exprType->name != "int") {
            if (endExpr->objType == A3ExprType::LITERAL) {
                endExpr->exprType = typePool[0].get(); // simple cast for literal
            } else {
                std::unique_ptr<A3ExprOperation> castOp = std::make_unique<A3ExprOperation>();
                castOp->objType = A3ExprType::OPERATION;
                castOp->location = e->location;
                castOp->subType = A3ExprOpType::B_CAST;
                castOp->operand0 = std::move(endExpr);
                castOp->typeOperand = typePool[0]->clone();
                castOp->exprType = typePool[0].get();
                endExpr = std::move(castOp);
            }
        }

    } else if (arrExpr->exprType->objType == A3TypeType::ARRAY) { // 1-3-1. array -> literal k
        endExpr = mkLiteral(Literal((int64_t)arrExpr->exprType->arrLen), typePool[0].get(), e->location);

    } else if (arrExpr->exprType->objType == A3TypeType::SLICE) { // 1-3-2. slice -> temp var
        if (arrExpr->objType != A3ExprType::VAR_NAME) { // make complex expr to var_name
            auto t = arrExpr->exprType;
            std::string tName = setTempVar(t, std::move(arrExpr));
            arrExpr = getTempVar(tName, e->location);
        }

        // clone arrExpr, end = len(arr)
        auto arrExprClone = std::make_unique<A3ExprName>();
        arrExprClone->objType = A3ExprType::VAR_NAME;
        arrExprClone->decl = ((A3ExprName*)arrExpr.get())->decl;
        arrExprClone->location = arrExpr->location;
        arrExprClone->exprType = arrExpr->exprType;

        auto lenOp = std::make_unique<A3ExprOperation>();
        lenOp->objType = A3ExprType::OPERATION;
        lenOp->subType = A3ExprOpType::U_LEN;
        lenOp->location = e->location;
        lenOp->operand0 = std::move(arrExprClone);
        lenOp->exprType = typePool[0].get(); // int
        endExpr = std::move(lenOp);

    } else { // invalid type
        throw std::runtime_error(std::format("E2106 invalid slicing target type at {}", getLocString(e->location))); // E2106
    }

    // 2-1. check bounds, 2-2. make startExpr var_name or literal, 2-3. make arrExpr l-value (var_name)
    checkArrayAccess(arrExpr->exprType, startExpr.get(), endExpr.get(), true, getLocString(e->location));
    if (startExpr->objType != A3ExprType::VAR_NAME && startExpr->objType != A3ExprType::LITERAL) {
        auto t = startExpr->exprType;
        std::string tName = setTempVar(t, std::move(startExpr));
        startExpr = getTempVar(tName, e->location);
    }
    if (arrExpr->objType != A3ExprType::VAR_NAME) {
        auto t = arrExpr->exprType;
        std::string tName = setTempVar(t, std::move(arrExpr));
        arrExpr = getTempVar(tName, e->location);
    }

    // 4-1. assemble types, register
    auto eleType = arrExpr->exprType->direct->clone();
    int eIdx = findType(eleType.get());
    if (eIdx == -1) { eIdx = typePool.size(); typePool.push_back(std::move(eleType)); }

    auto ptrType = std::make_unique<A3Type>();
    ptrType->objType = A3TypeType::POINTER;
    ptrType->name = "*";
    ptrType->typeSize = arch; ptrType->typeAlign = arch;
    ptrType->direct = typePool[eIdx]->clone();
    int pIdx = findType(ptrType.get());
    if (pIdx == -1) { pIdx = typePool.size(); typePool.push_back(std::move(ptrType)); }

    auto sliceType = std::make_unique<A3Type>();
    sliceType->objType = A3TypeType::SLICE;
    sliceType->name = "[]";
    sliceType->typeSize = arch * 2; sliceType->typeAlign = arch;
    sliceType->direct = typePool[eIdx]->clone();
    int sIdx = findType(sliceType.get());
    if (sIdx == -1) { sIdx = typePool.size(); typePool.push_back(std::move(sliceType)); }

    // 4-2. copy startExpr, assemble arr[st]
    std::unique_ptr<A3Expr> startExprClone;
    if (startExpr->objType == A3ExprType::VAR_NAME) {
        auto nameRef = std::make_unique<A3ExprName>();
        nameRef->objType = A3ExprType::VAR_NAME;
        nameRef->decl = ((A3ExprName*)startExpr.get())->decl;
        nameRef->location = nameRef->decl->location;
        nameRef->exprType = nameRef->decl->type.get();
        startExprClone = std::move(nameRef);
    } else if (startExpr->objType == A3ExprType::LITERAL) {
        startExprClone = mkLiteral(((A3ExprLiteral*)startExpr.get())->value, startExpr->exprType, startExpr->location);
    }

    auto idxOp = std::make_unique<A3ExprOperation>();
    idxOp->objType = A3ExprType::OPERATION;
    idxOp->subType = A3ExprOpType::B_INDEX;
    idxOp->location = e->location;
    idxOp->operand0 = std::move(arrExpr);
    idxOp->operand1 = std::move(startExprClone);
    idxOp->exprType = typePool[eIdx].get();

    // 4-3. assemble &arr[st], ed - st
    auto refOp = std::make_unique<A3ExprOperation>();
    refOp->objType = A3ExprType::OPERATION;
    refOp->subType = A3ExprOpType::U_REF;
    refOp->location = e->location;
    refOp->operand0 = std::move(idxOp);
    refOp->exprType = typePool[pIdx].get();

    auto subOp = std::make_unique<A3ExprOperation>();
    subOp->objType = A3ExprType::OPERATION;
    subOp->subType = A3ExprOpType::B_SUB;
    subOp->location = e->location;
    subOp->operand0 = std::move(endExpr);
    subOp->operand1 = std::move(startExpr);
    subOp->exprType = typePool[0].get(); // int

    auto makeOp = std::make_unique<A3ExprOperation>();
    makeOp->objType = A3ExprType::OPERATION;
    makeOp->subType = A3ExprOpType::B_MAKE;
    makeOp->location = e->location;
    makeOp->operand0 = std::move(refOp);
    makeOp->operand1 = std::move(subOp);
    makeOp->exprType = typePool[sIdx].get();
    return makeOp;
}

// lowering conditional operation
std::unique_ptr<A3Expr> A3Gen::lowerExprOpCond(A2ExprOperation* e) {
    if (e->subType == A2ExprOpType::T_COND) {
        // 1. lower condition, save stats
        std::unique_ptr<A3Expr> condExpr = lowerExpr(e->operand0.get(), "");
        int statPos = statBuf.size();
        std::vector<std::unique_ptr<A3Stat>> trueBuf;
        std::vector<std::unique_ptr<A3Stat>> falseBuf;
        std::unique_ptr<A3Expr> trueExpr = lowerExpr(e->operand1.get(), "");
        for (int i = statPos; i < statBuf.size(); i++) {
            trueBuf.push_back(std::move(statBuf[i]));
        }
        statBuf.resize(statPos);
        std::unique_ptr<A3Expr> falseExpr = lowerExpr(e->operand2.get(), "");
        for (int i = statPos; i < statBuf.size(); i++) {
            falseBuf.push_back(std::move(statBuf[i]));
        }
        statBuf.resize(statPos);

        // 2-1. return if both empty, 2-2. make result temp var
        if (trueBuf.size() == 0 && falseBuf.size() == 0) {
            std::unique_ptr<A3ExprOperation> opExpr = std::make_unique<A3ExprOperation>();
            opExpr->objType = A3ExprType::OPERATION;
            opExpr->subType = A3ExprOpType::T_COND;
            opExpr->location = e->location;
            opExpr->exprType = trueExpr->exprType;
            opExpr->operand0 = std::move(condExpr);
            opExpr->operand1 = std::move(trueExpr);
            opExpr->operand2 = std::move(falseExpr);
            return opExpr;
        }
        std::string resName = genTempVar(lowerType(e->exprType).get(), e->location);
        
        // 3-1. true branch
        auto trueScope = std::make_unique<A3StatScope>();
        trueScope->objType = A3StatType::SCOPE;
        trueScope->uid = uidCount++;
        trueScope->location = e->location;
        trueScope->body = std::move(trueBuf);
        trueScope->body.push_back(genAssignStat(getTempVar(resName, e->location), std::move(trueExpr)));

        // 3-2. false branch
        auto falseScope = std::make_unique<A3StatScope>();
        falseScope->objType = A3StatType::SCOPE;
        falseScope->uid = uidCount++;
        falseScope->location = e->location;
        falseScope->body = std::move(falseBuf);
        falseScope->body.push_back(genAssignStat(getTempVar(resName, e->location), std::move(falseExpr)));

        // 3-3. if-else
        auto ifStat = std::make_unique<A3StatIf>();
        ifStat->uid = uidCount++;
        ifStat->location = e->location;
        ifStat->cond = std::move(condExpr);
        ifStat->thenBody = std::move(trueScope);
        ifStat->elseBody = std::move(falseScope);

        // 3-4. return result
        statBuf.push_back(std::move(ifStat));
        return getTempVar(resName, e->location);

    } else {
        // 1. lower op0, op1 (with stats)
        std::unique_ptr<A3Expr> op0 = lowerExpr(e->operand0.get(), "");
        int statPos = statBuf.size();
        std::vector<std::unique_ptr<A3Stat>> op1Buf;
        std::unique_ptr<A3Expr> op1 = lowerExpr(e->operand1.get(), "");
        for (int i = statPos; i < statBuf.size(); i++) {
            op1Buf.push_back(std::move(statBuf[i]));
        }
        statBuf.resize(statPos);

        // 2. check op type
        auto basicRes = std::make_unique<A3ExprLiteral>();
        basicRes->objType = A3ExprType::LITERAL;
        basicRes->location = e->location;
        basicRes->exprType = typePool[12].get(); // bool

        auto opExpr = std::make_unique<A3ExprOperation>();
        opExpr->objType = A3ExprType::OPERATION;
        opExpr->location = e->location;
        opExpr->exprType = typePool[12].get(); // bool
        if (e->subType == A2ExprOpType::B_LOGIC_AND) {
            opExpr->subType = A3ExprOpType::B_LOGIC_AND;
            basicRes->value = Literal(false);
        } else if (e->subType == A2ExprOpType::B_LOGIC_OR) {
            opExpr->subType = A3ExprOpType::B_LOGIC_OR;
            basicRes->value = Literal(true);
            // op0 = (!op0)
            if (op0->objType == A3ExprType::OPERATION && ((A3ExprOperation*)(op0.get()))->subType == A3ExprOpType::U_LOGIC_NOT) {
                op0 = std::move(((A3ExprOperation*)(op0.get()))->operand0);
            } else {
                auto notOp = std::make_unique<A3ExprOperation>();
                notOp->objType = A3ExprType::OPERATION;
                notOp->location = e->location;
                notOp->exprType = typePool[12].get(); // bool
                notOp->subType = A3ExprOpType::U_LOGIC_NOT;
                notOp->operand0 = std::move(op0);
                op0 = std::move(notOp);
            }
        } else {
            throw std::runtime_error(std::format("E2107 invalid logic op type at {}", getLocString(e->location))); // E2107
        }

        // 3-1. return if body empty, 3-2. make result temp var
        if (op1Buf.size() == 0) {
            opExpr->operand0 = std::move(op0);
            opExpr->operand1 = std::move(op1);
            return opExpr;
        }
        std::string resName = setTempVar(typePool[12].get(), std::move(basicRes));
        
        // 4-1. make stat (if (op0) res = op1;)
        auto ifStat = std::make_unique<A3StatIf>();
        ifStat->uid = uidCount++;
        ifStat->location = e->location;
        ifStat->cond = std::move(op0);
        ifStat->thenBody = genAssignStat(getTempVar(resName, e->location), std::move(op1));
        ifStat->elseBody = nullptr;
        
        // 4-2. return result
        statBuf.push_back(std::move(ifStat));
        return getTempVar(resName, e->location);
    }
}

// lowering expression call
std::vector<std::unique_ptr<A3Expr>> A3Gen::lowerExprCall(A3Type* ftype, std::vector<std::unique_ptr<A2Expr>>& a2Args, bool isVaArg, bool isRetArray, std::string* retName) {
    std::vector<std::unique_ptr<A3Expr>> a3Args;
    int fixArgCount = ftype->indirect.size();
    if (isVaArg) fixArgCount--;
    if (isRetArray) fixArgCount--;

    // 1. convert fixed arguments
    for (int i = 0; i < fixArgCount; i++) {
        auto argExpr = lowerExpr(a2Args[i].get(), ""); // lower expression
        if (hasExprSideEffect(argExpr.get())) { // assign to temp if side-effect
            auto t = argExpr->exprType;
            auto tName = setTempVar(t, std::move(argExpr));
            a3Args.push_back(getTempVar(tName, a2Args[i]->location));
        } else { // no side-effect
            a3Args.push_back(std::move(argExpr));
        }
    }

    // 2-1. convert variadic arguments
    if (isVaArg) {
        int vaArgCount = a2Args.size() - fixArgCount;
        std::vector<std::unique_ptr<A3Expr>> varArgs;
        for (int i = fixArgCount; i < fixArgCount + vaArgCount; i++) {
            auto argExpr = lowerExpr(a2Args[i].get(), "");
            std::unique_ptr<A3Expr> ptrExpr; // ptr arg to be casted to void*

            switch (argExpr->exprType->objType) {
                case A3TypeType::POINTER: case A3TypeType::FUNCTION: // pass as is
                    ptrExpr = std::move(argExpr);
                    break;

                case A3TypeType::ARRAY: // gen temp var, memcpy, take address
                {
                    auto l = argExpr->location;
                    auto tName = genTempVar(argExpr->exprType, l);
                    auto memCpy = std::make_unique<A3StatMem>(); // memcpy(dst=temp, src=argExpr, size=typeSize)
                    memCpy->objType = A3StatType::MEMCPY;
                    memCpy->location = l;
                    memCpy->uid = uidCount++;
                    memCpy->dst = getTempVar(tName, l);
                    memCpy->size = createArraySizeExpr(argExpr->exprType, typePool[0].get(), l);
                    memCpy->sizeHint = argExpr->exprType->typeSize;
                    memCpy->src = std::move(argExpr);
                    statBuf.push_back(std::move(memCpy));
                    ptrExpr = refVar(tName, l); // take address of temp
                }
                break;

                default: // value type, set temp var, take address
                {
                    auto t = argExpr->exprType;
                    auto l = argExpr->location;
                    auto tName = setTempVar(t, std::move(argExpr));
                    ptrExpr = refVar(tName, l); // take address of temp
                }
                break;
            }

            // cast to void*
            auto castOp = std::make_unique<A3ExprOperation>();
            castOp->objType = A3ExprType::OPERATION;
            castOp->subType = A3ExprOpType::B_CAST;
            castOp->location = ptrExpr->location;
            castOp->operand0 = std::move(ptrExpr);
            castOp->typeOperand = typePool[14]->clone(); // void*
            castOp->exprType = typePool[14].get();
            
            std::unique_ptr<A3Expr> voidPtrExpr = std::move(castOp);
            varArgs.push_back(std::move(voidPtrExpr));
        }

        if (vaArgCount > 0) {
            // 2-2. create void* array
            auto arrType = std::make_unique<A3Type>();
            arrType->objType = A3TypeType::ARRAY;
            arrType->name = std::format("[{}]", vaArgCount);
            arrType->typeSize = typePool[14]->typeSize * vaArgCount;
            arrType->typeAlign = typePool[14]->typeAlign;
            arrType->direct = typePool[14]->clone();
            arrType->arrLen = vaArgCount;
            std::string arrName = genTempVar(arrType.get(), a2Args[0]->location);

            // 2-3. fill array
            for (int i = 0; i < vaArgCount; i++) {
                auto left = std::make_unique<A3ExprOperation>();
                left->objType = A3ExprType::OPERATION;
                left->subType = A3ExprOpType::B_INDEX;
                left->location = varArgs[i]->location;
                left->operand0 = getTempVar(arrName, varArgs[i]->location);
                left->operand1 = mkLiteral(Literal((int64_t)i), typePool[0].get(), varArgs[i]->location);
                left->exprType = typePool[14].get(); // void*
                statBuf.push_back(genAssignStat(std::move(left), std::move(varArgs[i]))); // assign
            }

            // 2-4. make slice
            auto makeOp = std::make_unique<A3ExprOperation>();
            makeOp->objType = A3ExprType::OPERATION;
            makeOp->subType = A3ExprOpType::B_MAKE;
            makeOp->location = varArgs[0]->location;

            // &arr[0]
            auto addrOp = std::make_unique<A3ExprOperation>();
            addrOp->objType = A3ExprType::OPERATION;
            addrOp->subType = A3ExprOpType::U_REF;
            addrOp->location = varArgs[0]->location;

            auto idxOp = std::make_unique<A3ExprOperation>();
            idxOp->objType = A3ExprType::OPERATION;
            idxOp->subType = A3ExprOpType::B_INDEX;
            idxOp->location = varArgs[0]->location;
            idxOp->operand0 = getTempVar(arrName, varArgs[0]->location);
            idxOp->operand1 = mkLiteral(Literal((int64_t)0), typePool[0].get(), varArgs[0]->location);
            idxOp->exprType = typePool[14].get(); // void*

            addrOp->operand0 = std::move(idxOp);
            
            // type of &arr[0] (void**)
            auto ptrType = std::make_unique<A3Type>();
            ptrType->objType = A3TypeType::POINTER;
            ptrType->name = "*";
            ptrType->typeSize = arch;
            ptrType->typeAlign = arch;
            ptrType->direct = typePool[14]->clone(); // void*

            // register type
            int pIdx = findType(ptrType.get());
            if (pIdx == -1) {
                pIdx = typePool.size();
                typePool.push_back(std::move(ptrType));
            }

            addrOp->exprType = typePool[pIdx].get();
            makeOp->operand0 = std::move(addrOp);
            makeOp->operand1 = mkLiteral(Literal((int64_t)vaArgCount), typePool[0].get(), varArgs[0]->location);
            a3Args.push_back(std::move(makeOp));
        }
    }

    // 3. convert ret array
    if (isRetArray) {
        if (*retName == "") { // decl ret var
            std::string ret = genTempVar(ftype->direct.get(), ftype->location);
            *retName = ret;
        }
        a3Args.push_back(getTempVar(*retName, ftype->location)); // add ret var to args
    }
    return a3Args;
}

// main lowering function for statements
std::vector<std::unique_ptr<A3Stat>> A3Gen::lowerStat(A2Stat* s) {
    statBuf.clear();
    std::vector<std::unique_ptr<A3Stat>> resBuf;
    switch (s->objType) {
        case A2StatType::RAW_C: case A2StatType::RAW_IR:
        {
            A2StatRaw* raw = (A2StatRaw*)s;
            auto stat = std::make_unique<A3StatRaw>();
            stat->objType = (raw->objType == A2StatType::RAW_C) ? A3StatType::RAW_C : A3StatType::RAW_IR;
            stat->location = raw->location;
            stat->uid = uidCount++;
            stat->code = raw->code;
            resBuf.push_back(std::move(stat));
        }
        break;

        case A2StatType::EXPR:
        {
            A2StatExpr* expr = (A2StatExpr*)s;
            auto stat = std::make_unique<A3StatExpr>();
            stat->objType = A3StatType::EXPR;
            stat->location = expr->location;
            stat->uid = uidCount++;
            stat->expr = lowerExpr(expr->expr.get(), "");
            for (auto& s : statBuf) { // move statBuf to resBuf
                resBuf.push_back(std::move(s));
            }
            resBuf.push_back(std::move(stat));
        }
        break;

        case A2StatType::DECL:
        {
            A2StatDecl* decl = (A2StatDecl*)s;
            auto stat = std::make_unique<A3StatDecl>();
            stat->objType = A3StatType::DECL;
            stat->location = decl->location;
            stat->uid = uidCount++;
            stat->decl = lowerDecl(decl->decl.get());
            for (auto& s : statBuf) { // move statBuf to resBuf
                resBuf.push_back(std::move(s));
            }
            resBuf.push_back(std::move(stat));
        }
        break;

        case A2StatType::ASSIGN:
        {
            A2StatAssign* assign = (A2StatAssign*)s;
            auto left = lowerExpr(assign->left.get(), "");
            std::string tgtName = "";
            if (left->objType == A3ExprType::VAR_NAME) {
                tgtName = ((A3ExprName*)left.get())->decl->name;
            }
            auto right = lowerExpr(assign->right.get(), tgtName);
            for (auto& s : statBuf) { // move statBuf to resBuf
                resBuf.push_back(std::move(s));
            }

            // check RVO optimization
            bool isOpt = false;
            if (left->exprType->objType == A3TypeType::ARRAY && left->objType == A3ExprType::VAR_NAME && right->objType == A3ExprType::VAR_NAME) {
                if (((A3ExprName*)left.get())->decl->uid == ((A3ExprName*)right.get())->decl->uid) isOpt = true;
            }

            // memcpy array
            if (left->exprType->objType == A3TypeType::ARRAY && !isOpt) {
                auto stat = std::make_unique<A3StatMem>();
                stat->objType = A3StatType::MEMCPY;
                stat->location = assign->location;
                stat->uid = uidCount++;
                stat->src = std::move(right);
                stat->dst = std::move(left);
                stat->size = createArraySizeExpr(left->exprType, typePool[0].get(), assign->location);
                stat->sizeHint = left->exprType->typeSize;
                resBuf.push_back(std::move(stat));

            // simple assign
            } else if (left->exprType->objType != A3TypeType::ARRAY) {
                auto stat = std::make_unique<A3StatAssign>();
                stat->objType = A3StatType::ASSIGN;
                stat->location = assign->location;
                stat->uid = uidCount++;
                stat->left = std::move(left);
                stat->right = std::move(right);
                resBuf.push_back(std::move(stat));
            }
        }
        break;

        case A2StatType::ASSIGN_ADD: case A2StatType::ASSIGN_SUB: case A2StatType::ASSIGN_MUL: case A2StatType::ASSIGN_DIV: case A2StatType::ASSIGN_MOD:
        {
            A2StatAssign* assign = (A2StatAssign*)s;
            auto left = lowerExpr(assign->left.get(), "");
            A3ExprOpType opType; // set operation type
            if (assign->objType == A2StatType::ASSIGN_ADD) {
                opType = A3ExprOpType::B_ADD;
            } else if (assign->objType == A2StatType::ASSIGN_SUB) {
                opType = A3ExprOpType::B_SUB;
            } else if (assign->objType == A2StatType::ASSIGN_MUL) {
                opType = A3ExprOpType::B_MUL;
            } else if (assign->objType == A2StatType::ASSIGN_DIV) {
                opType = A3ExprOpType::B_DIV;
            } else if (assign->objType == A2StatType::ASSIGN_MOD) {
                opType = A3ExprOpType::B_MOD;
            }
            if (left->exprType->objType == A3TypeType::POINTER && assign->objType == A2StatType::ASSIGN_ADD) {
                opType = A3ExprOpType::B_PTR_ADD;
            } else if (left->exprType->objType == A3TypeType::POINTER && assign->objType == A2StatType::ASSIGN_SUB) {
                opType = A3ExprOpType::B_PTR_SUB;
            }

            // make left var_use
            std::unique_ptr<A3Expr> left0;
            std::unique_ptr<A3Expr> left1;
            if (left->objType == A3ExprType::VAR_NAME) {
                A3ExprName* nm = (A3ExprName*)left.get();
                auto l0 = std::make_unique<A3ExprName>();
                l0->objType = A3ExprType::VAR_NAME;
                l0->decl = nm->decl;
                l0->location = nm->location;
                l0->exprType = nm->exprType;
                left0 = std::move(l0);
                auto l1 = std::make_unique<A3ExprName>();
                l1->objType = A3ExprType::VAR_NAME;
                l1->decl = nm->decl;
                l1->location = nm->location;
                l1->exprType = nm->exprType;
                left1 = std::move(l1);
            } else {
                auto refLeft = std::make_unique<A3ExprOperation>();
                refLeft->objType = A3ExprType::OPERATION;
                refLeft->location = assign->location;
                refLeft->subType = A3ExprOpType::U_REF;
                refLeft->operand0 = std::move(left);
                auto leftName = setTempVar(refLeft->exprType, std::move(refLeft));
                left0 = derefVar(leftName, assign->location);
                left1 = derefVar(leftName, assign->location);
            }

            // convert right, assemble stat
            auto right = lowerExpr(assign->right.get(), "");
            for (auto& s : statBuf) { // move statBuf to resBuf
                resBuf.push_back(std::move(s));
            }

            auto expr = std::make_unique<A3ExprOperation>();
            expr->objType = A3ExprType::OPERATION;
            expr->location = assign->location;
            expr->subType = opType;
            expr->operand0 = std::move(left0);
            expr->operand1 = std::move(right);

            auto stat = std::make_unique<A3StatAssign>();
            stat->objType = A3StatType::ASSIGN;
            stat->location = assign->location;
            stat->uid = uidCount++;
            stat->left = std::move(left1);
            stat->right = std::move(expr);
            resBuf.push_back(std::move(stat));
        }
        break;

        case A2StatType::RETURN: case A2StatType::BREAK: case A2StatType::CONTINUE:
            return lowerStatCtrl((A2StatCtrl*)s);

        case A2StatType::SCOPE:
            resBuf.push_back(lowerStatScope((A2StatScope*)s, nullptr, {}));
            break;

        case A2StatType::IF:
        {
            A2StatIf* ifStat = (A2StatIf*)s;
            auto ifRes = std::make_unique<A3StatIf>();
            ifRes->objType = A3StatType::IF;
            ifRes->location = ifStat->location;
            ifRes->uid = uidCount++;
            ifRes->cond = lowerExpr(ifStat->cond.get(), "");
            for (auto& s : statBuf) { // move statBuf to resBuf
                resBuf.push_back(std::move(s));
            }

            auto thenStats = lowerStat(ifStat->thenBody.get());
            if (thenStats.size() == 1) { // if then body is a single statement
                ifRes->thenBody = std::move(thenStats[0]);
            } else { // if then body is a scope
                auto thenScope = std::make_unique<A3StatScope>();
                thenScope->objType = A3StatType::SCOPE;
                thenScope->location = ifStat->location;
                thenScope->uid = uidCount++;
                thenScope->body = std::move(thenStats);
                ifRes->thenBody = std::move(thenScope);
            }

            if (ifStat->elseBody == nullptr || ifStat->elseBody->objType == A2StatType::NONE) {
                ifRes->elseBody = nullptr;
            } else {
                auto elseStats = lowerStat(ifStat->elseBody.get());
                if (elseStats.size() == 1) { // if else body is a single statement
                    ifRes->elseBody = std::move(elseStats[0]);
                } else { // if else body is a scope
                    auto elseScope = std::make_unique<A3StatScope>();
                    elseScope->objType = A3StatType::SCOPE;
                    elseScope->location = ifStat->location;
                    elseScope->uid = uidCount++;
                    elseScope->body = std::move(elseStats);
                    ifRes->elseBody = std::move(elseScope);
                }
            }
            resBuf.push_back(std::move(ifRes));
        }
        break;

        case A2StatType::LOOP:
            resBuf.push_back(lowerStatLoop((A2StatLoop*)s));
            break;

        case A2StatType::SWITCH:
        {
            A2StatSwitch* sw = (A2StatSwitch*)s;
            auto swRes = std::make_unique<A3StatSwitch>();
            swRes->objType = A3StatType::SWITCH;
            swRes->location = sw->location;
            swRes->uid = uidCount++;
            swRes->cond = lowerExpr(sw->cond.get(), "");
            for (auto& s : statBuf) { // move statBuf to resBuf
                resBuf.push_back(std::move(s));
            }
            for (int i = 0; i < sw->caseConds.size(); i++) {
                swRes->caseConds.push_back(sw->caseConds[i]); // fill caseConds
                swRes->caseFalls.push_back(sw->caseFalls[i]); // fill caseFalls
                std::vector<std::unique_ptr<A3Stat>> body;
                for (auto& v : sw->caseBodies[i]) {
                    auto u = lowerStat(v.get());
                    for (auto& t : u) {
                        body.push_back(std::move(t));
                    }
                }
                swRes->caseBodies.push_back(std::move(body)); // fill caseBodies
            }
            std::vector<std::unique_ptr<A3Stat>> body;
            for (auto& v : sw->defaultBody) {
                auto u = lowerStat(v.get());
                for (auto& t : u) {
                    body.push_back(std::move(t));
                }
            }
            swRes->defaultBody = std::move(body); // fill defaultBody
            resBuf.push_back(std::move(swRes));
        }
        break;

        default:
            throw std::runtime_error(std::format("E2201 unknown statement type at {}", getLocString(s->location))); // E2201
    }
    return resBuf;
}

// lower control flow (return, break, continue)
std::vector<std::unique_ptr<A3Stat>> A3Gen::lowerStatCtrl(A2StatCtrl* s) {
    A3StatCtrl* jmpLbl = nullptr;
    for (int i = scopes.size() - 1; i >= 0; i--) {
        if (scopes[i]->scopeLbl != nullptr) {
            jmpLbl = scopes[i]->scopeLbl;
            break;
        }
    }
    if (jmpLbl == nullptr || curFunc == nullptr) {
        throw std::runtime_error(std::format("E2202 cannot find control jump target at {}", getLocString(s->location))); // E2202
    }

    std::vector<std::unique_ptr<A3Stat>> resBuf;
    if (s->objType == A2StatType::RETURN || s->objType == A2StatType::BREAK || s->objType == A2StatType::CONTINUE) {
        if (s->objType == A2StatType::RETURN && !(curFunc->retType->objType == A3TypeType::PRIMITIVE && curFunc->retType->name == "void")) { // set return value, memcpy array
            auto retExpr = lowerExpr(s->body.get(), curFunc->retVar->name);
            for (auto& s : statBuf) { // move statBuf to resBuf
                resBuf.push_back(std::move(s));
            }

            // check RVO optimization
            bool isOpt = false;
            if (curFunc->retType->objType == A3TypeType::ARRAY && retExpr->objType == A3ExprType::VAR_NAME) {
                if (curFunc->retVar->uid == ((A3ExprName*)retExpr.get())->decl->uid) isOpt = true;
            }

            // create dst (curFunc retrun value)
            std::unique_ptr<A3ExprName> dst = std::make_unique<A3ExprName>();
            dst->objType = A3ExprType::VAR_NAME;
            dst->location = s->location;
            dst->decl = curFunc->retVar;
            dst->exprType = curFunc->retVar->type.get();

            // memcpy array
            if (curFunc->retType->objType == A3TypeType::ARRAY && !isOpt) {
                auto stat = std::make_unique<A3StatMem>();
                stat->objType = A3StatType::MEMCPY;
                stat->location = s->location;
                stat->uid = uidCount++;
                stat->src = std::move(retExpr);
                stat->dst = std::move(dst);
                stat->size = createArraySizeExpr(curFunc->retType.get(), typePool[0].get(), s->location);
                stat->sizeHint = curFunc->retType->typeSize;
                resBuf.push_back(std::move(stat));

            // simple assign
            } else if (curFunc->retType->objType != A3TypeType::ARRAY) {
                auto stat = std::make_unique<A3StatAssign>();
                stat->objType = A3StatType::ASSIGN;
                stat->location = s->location;
                stat->uid = uidCount++;
                stat->left = std::move(dst);
                stat->right = std::move(retExpr);
                resBuf.push_back(std::move(stat));
            }
        }

        // set status variable
        std::unique_ptr<A3ExprName> left = std::make_unique<A3ExprName>();
        left->objType = A3ExprType::VAR_NAME;
        left->location = s->location;
        left->decl = curFunc->stateVar;
        left->exprType = left->decl->type.get();
        auto right = mkLiteral(Literal(countJumps(s->objType)), typePool[0].get(), s->location);
        resBuf.push_back(genAssignStat(std::move(left), std::move(right)));

        // jump to scope label
        std::unique_ptr<A3StatCtrl> ctrl = std::make_unique<A3StatCtrl>();
        ctrl->objType = A3StatType::JUMP;
        ctrl->location = s->location;
        ctrl->uid = uidCount++;
        ctrl->label = jmpLbl;
        resBuf.push_back(std::move(ctrl));
    } else {
        throw std::runtime_error(std::format("E2203 unknown control flow type at {}", getLocString(s->location))); // E2203
    }
    return resBuf;
}

// lower scope
std::unique_ptr<A3StatScope> A3Gen::lowerStatScope(A2StatScope* s, A3StatWhile* w, std::vector<std::unique_ptr<A3Stat>> step) {
    int scopeType = 0; // normal scope
    if (w != nullptr) {
        scopeType = 1; // while scope
    } else if (s->defers.size() > 0) {
        scopeType = 2; // defer scope
    }

    // make scope, label, info
    auto scopeRes = std::make_unique<A3StatScope>();
    scopeRes->objType = A3StatType::SCOPE;
    scopeRes->location = s->location;
    scopeRes->uid = uidCount++;
    std::unique_ptr<A3StatCtrl> labelA = nullptr;
    if (scopeType != 0) {
        labelA = std::make_unique<A3StatCtrl>();
        labelA->objType = A3StatType::LABEL;
        labelA->location = s->location;
        labelA->uid = uidCount++;
    }
    auto info = std::make_unique<A3ScopeInfo>(scopeRes.get(), labelA.get(), w);
    scopes.push_back(std::move(info));

    // convert body
    for (auto& st : s->body) {
        auto converted = lowerStat(st.get());
        for (auto& c : converted) {
            scopeRes->body.push_back(std::move(c));
        }
    }

    // insert label, defer
    if (scopeType != 0) {
        scopeRes->body.push_back(std::move(labelA));
        for (auto& d : s->defers) {
            statBuf.clear();
            auto converted = lowerExpr(d.get(), "");
            for (auto& c : statBuf) {
                scopeRes->body.push_back(std::move(c));
            }
            auto c = std::make_unique<A3StatExpr>();
            c->objType = A3StatType::EXPR;
            c->location = d->location;
            c->uid = uidCount++;
            c->expr = std::move(converted);
            scopeRes->body.push_back(std::move(c));
        }
    }

    // find parent label
    A3StatCtrl* jmpLbl = nullptr;
    for (int i = scopes.size() - 1; i >= 0; i--) {
        if (scopes[i]->scopeLbl != nullptr) {
            jmpLbl = scopes[i]->scopeLbl;
            break;
        }
    }
    if (jmpLbl == nullptr || curFunc == nullptr) {
        throw std::runtime_error(std::format("E2204 cannot find control jump target at {}", getLocString(s->location))); // E2204
    }

    // jump logic for loop
    if (scopeType == 1) {
        auto checkState = getTempVar(curFunc->stateVar->name, s->location);
    
        // if (state == 0) {step; continue;}
        auto zeroVal = mkLiteral(Literal((int64_t)0), typePool[0].get(), s->location);
        auto isZero = std::make_unique<A3ExprOperation>();
        isZero->objType = A3ExprType::OPERATION;
        isZero->subType = A3ExprOpType::B_EQ;
        isZero->exprType = typePool[12].get();
        isZero->operand0 = std::move(checkState);
        isZero->operand1 = std::move(zeroVal);

        auto case0Block = std::make_unique<A3StatScope>();
        case0Block->objType = A3StatType::SCOPE;
        // step
        for (auto& st: step) case0Block->body.push_back(std::move(st));
        // continue
        auto contStat = std::make_unique<A3StatCtrl>();
        contStat->objType = A3StatType::CONTINUE;
        case0Block->body.push_back(std::move(contStat));

        // if (state == 1) {state--; break;}
        checkState = getTempVar(curFunc->stateVar->name, s->location);
        auto oneVal = mkLiteral(Literal((int64_t)1), typePool[0].get(), s->location);
        auto isOne = std::make_unique<A3ExprOperation>();
        isOne->objType = A3ExprType::OPERATION;
        isOne->subType = A3ExprOpType::B_EQ;
        isOne->exprType = typePool[12].get();
        isOne->operand0 = std::move(checkState);
        isOne->operand1 = std::move(oneVal);

        auto case1Block = std::make_unique<A3StatScope>();
        case1Block->objType = A3StatType::SCOPE;
        // state--
        auto lState = getTempVar(curFunc->stateVar->name, s->location);
        auto rState = getTempVar(curFunc->stateVar->name, s->location);
        auto oneLit = mkLiteral(Literal((int64_t)1), typePool[0].get(), s->location);
        auto subOp = std::make_unique<A3ExprOperation>();
        subOp->objType = A3ExprType::OPERATION;
        subOp->subType = A3ExprOpType::B_SUB;
        subOp->exprType = typePool[0].get();
        subOp->operand0 = std::move(rState);
        subOp->operand1 = std::move(oneLit);
        case1Block->body.push_back(genAssignStat(std::move(lState), std::move(subOp)));
        // break
        auto breakStat = std::make_unique<A3StatCtrl>();
        breakStat->objType = A3StatType::BREAK;
        case1Block->body.push_back(std::move(breakStat));

        // else {state--; jump parent}
        auto caseElseBlock = std::make_unique<A3StatScope>();
        caseElseBlock->objType = A3StatType::SCOPE;
        if (scopes.size() > 1) {
            A3StatCtrl* parentLabel = scopes[scopes.size() - 2]->scopeLbl;
            if (parentLabel) {
                // state--
                lState = getTempVar(curFunc->stateVar->name, s->location);
                rState = getTempVar(curFunc->stateVar->name, s->location);
                oneLit = mkLiteral(Literal((int64_t)1), typePool[0].get(), s->location);
                subOp = std::make_unique<A3ExprOperation>();
                subOp->objType = A3ExprType::OPERATION;
                subOp->subType = A3ExprOpType::B_SUB;
                subOp->exprType = typePool[0].get();
                subOp->operand0 = std::move(rState);
                subOp->operand1 = std::move(oneLit);
                caseElseBlock->body.push_back(genAssignStat(std::move(lState), std::move(subOp)));
            
                // jump parent
                auto jmpP = std::make_unique<A3StatCtrl>();
                jmpP->objType = A3StatType::JUMP;
                jmpP->label = parentLabel;
                caseElseBlock->body.push_back(std::move(jmpP));
            }
        }

        // assemble if, else if, else
        auto elseIfStat = std::make_unique<A3StatIf>();
        elseIfStat->objType = A3StatType::IF;
        elseIfStat->cond = std::move(isOne);
        elseIfStat->thenBody = std::move(case1Block);
        elseIfStat->elseBody = std::move(caseElseBlock);

        auto topIfStat = std::make_unique<A3StatIf>();
        topIfStat->objType = A3StatType::IF;
        topIfStat->cond = std::move(isZero);
        topIfStat->thenBody = std::move(case0Block);
        topIfStat->elseBody = std::move(elseIfStat);

        scopeRes->body.push_back(std::move(topIfStat));

    // jump logic for scope
    } else if (scopeType == 2) {
        // condition: state > 0
        auto stateVar = getTempVar(curFunc->stateVar->name, s->location);
        auto zero = mkLiteral(Literal((int64_t)0), typePool[0].get(), s->location);
        
        auto cond = std::make_unique<A3ExprOperation>();
        cond->objType = A3ExprType::OPERATION;
        cond->subType = A3ExprOpType::B_GT;
        cond->location = s->location;
        cond->exprType = typePool[12].get(); // bool
        cond->operand0 = std::move(stateVar);
        cond->operand1 = std::move(zero);

        // sody: state--; jump parent
        auto thenBlock = std::make_unique<A3StatScope>();
        thenBlock->objType = A3StatType::SCOPE;
        thenBlock->uid = uidCount++;
        
        // state-- (state = state - 1)
        auto lState = getTempVar(curFunc->stateVar->name, s->location);
        auto rState = getTempVar(curFunc->stateVar->name, s->location);
        auto one = mkLiteral(Literal((int64_t)1), typePool[0].get(), s->location);
        auto sub = std::make_unique<A3ExprOperation>();
        sub->objType = A3ExprType::OPERATION;
        sub->subType = A3ExprOpType::B_SUB;
        sub->exprType = typePool[0].get();
        sub->operand0 = std::move(rState);
        sub->operand1 = std::move(one);
        thenBlock->body.push_back(genAssignStat(std::move(lState), std::move(sub)));

        // jump parent
        auto jmp = std::make_unique<A3StatCtrl>();
        jmp->objType = A3StatType::JUMP;
        jmp->uid = uidCount++;
        jmp->label = jmpLbl;
        thenBlock->body.push_back(std::move(jmp));

        // if statement, insert into scope
        auto ifStat = std::make_unique<A3StatIf>();
        ifStat->objType = A3StatType::IF;
        ifStat->uid = uidCount++;
        ifStat->cond = std::move(cond);
        ifStat->thenBody = std::move(thenBlock);
        scopeRes->body.push_back(std::move(ifStat));
    }
    return scopeRes;
}

// lower loop
std::unique_ptr<A3StatWhile> A3Gen::lowerStatLoop(A2StatLoop* s) {
    auto cond = lowerExpr(s->cond.get(), "");
    std::vector<std::unique_ptr<A3Stat>> preCond;
    for (auto& s : statBuf) { // move statBuf to preCond
        preCond.push_back(std::move(s));
    }
    auto step = lowerStat(s->step.get()); // lower step statement
    auto whileStat = std::make_unique<A3StatWhile>();
    whileStat->objType = A3StatType::WHILE;
    whileStat->uid = uidCount++;

    // lower body
    std::unique_ptr<A3StatScope> bodyScope;
    if (s->body->objType == A2StatType::SCOPE) {
        bodyScope = std::move(lowerStatScope((A2StatScope*)s->body.get(), whileStat.get(), std::move(step)));
    } else {
        auto vScope = std::make_unique<A2StatScope>();
        vScope->uid = uidCount++;
        vScope->body.push_back(std::move(s->body));
        bodyScope = std::move(lowerStatScope(vScope.get(), whileStat.get(), std::move(step)));
    }

    // lower preCond
    if (preCond.size() > 0) {
        whileStat->cond = mkLiteral(Literal(true), typePool[12].get(), s->location);
        // if (!cond) break;
        auto notCond = std::make_unique<A3ExprOperation>();
        notCond->objType = A3ExprType::OPERATION;
        notCond->subType = A3ExprOpType::U_LOGIC_NOT;
        notCond->exprType = typePool[12].get();
        notCond->operand0 = std::move(cond);
        notCond->location = s->location;
        auto breakStat = std::make_unique<A3StatCtrl>();
        breakStat->objType = A3StatType::BREAK;
        breakStat->uid = uidCount++;
        breakStat->location = s->location;
        auto ifStat = std::make_unique<A3StatIf>();
        ifStat->objType = A3StatType::IF;
        ifStat->cond = std::move(notCond);
        ifStat->thenBody = std::move(breakStat);
        ifStat->elseBody = nullptr;
        bodyScope->body.insert(bodyScope->body.begin(), std::move(ifStat));
    } else {
        whileStat->cond = std::move(cond);
    }
    whileStat->body = std::move(bodyScope);
    return whileStat;
}

// main lowering function for declarations
std::unique_ptr<A3Decl> A3Gen::lowerDecl(A2Decl* d) {
    switch (d->objType) {
        case A2DeclType::RAW_C: case A2DeclType::RAW_IR:
        {
            auto rawDecl = (A2DeclRaw*)d;
            auto res = std::make_unique<A3DeclRaw>();
            res->objType = (d->objType == A2DeclType::RAW_C) ? A3DeclType::RAW_C : A3DeclType::RAW_IR;
            res->uid = uidCount++;
            res->code = rawDecl->code;
            res->location = rawDecl->location;
            return res;
        }

        case A2DeclType::VAR:
        {
            auto varDecl = (A2DeclVar*)d;
            auto res = std::make_unique<A3DeclVar>();
            res->objType = A3DeclType::VAR;
            res->uid = uidCount++;
            res->name = varDecl->name;
            res->type = lowerType(varDecl->type.get());
            res->isExported = varDecl->isExported;
            res->location = varDecl->location;
            res->init = lowerExpr(varDecl->init.get(), res->name);
            return res;
        }

        case A2DeclType::FUNC: ///// current working
        {
            auto funcDecl = (A2DeclFunc*)d;
            auto res = std::make_unique<A3DeclFunc>();
            res->objType = A3DeclType::FUNC;
            res->uid = uidCount++;
            res->name = funcDecl->name;
            res->isExported = funcDecl->isExported;
            res->location = funcDecl->location;
            res->type = lowerType(funcDecl->type.get());
            curFunc = res.get();

            res->body = std::make_unique<A3StatScope>();
            res->body->objType = A3StatType::SCOPE;
            res->body->uid = uidCount++;
            res->body->location = funcDecl->location;
            std::string stateName = setTempVar(typePool[0].get(), mkLiteral(Literal((int64_t)0), typePool[0].get(), funcDecl->location));
            curFunc->stateVar = findVar(stateName);
            std::string retName = "";
            curFunc->retVar = nullptr;
            if (res->type->direct->objType != A3TypeType::ARRAY && !(res->type->direct->objType == A3TypeType::PRIMITIVE && res->type->direct->name == "void")) {
                retName = genTempVar(res->type->direct.get(), funcDecl->location);
                curFunc->retVar = findVar(retName);
            }


            return res;
        }

        case A2DeclType::STRUCT:
        {
            auto structDecl = (A2DeclStruct*)d;
            auto res = std::make_unique<A3DeclStruct>();
            res->objType = A3DeclType::STRUCT;
            res->uid = uidCount++;
            res->name = structDecl->name;
            res->isExported = structDecl->isExported;
            res->location = structDecl->location;
            res->type = lowerType(structDecl->type.get());
            for (int i = 0; i < structDecl->memNames.size(); i++) {
                res->memNames.push_back(structDecl->memNames[i]);
                res->memTypes.push_back(lowerType(structDecl->memTypes[i].get()));
                res->memOffsets.push_back(structDecl->memOffsets[i]);
            }
            return res;
        }

        case A2DeclType::ENUM:
        {
            auto enumDecl = (A2DeclEnum*)d;
            auto res = std::make_unique<A3DeclEnum>();
            res->objType = A3DeclType::ENUM;
            res->uid = uidCount++;
            res->name = enumDecl->name;
            res->isExported = enumDecl->isExported;
            res->location = enumDecl->location;
            res->type = lowerType(enumDecl->type.get());
            for (int i = 0; i < enumDecl->memNames.size(); i++) {
                res->memNames.push_back(enumDecl->memNames[i]);
                res->memValues.push_back(enumDecl->memValues[i]);
            }
            return res;
        }

        default:
            throw std::runtime_error(std::format("E2301 invalid declaration type at {}", getLocString(d->location))); // E2301
    }
}