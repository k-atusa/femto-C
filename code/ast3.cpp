#include "ast3.h"

/*
lowering 주요 변환규칙
- A3의 배열은 값 타입처럼 표현되나 실제로는 포인터로 취급 (할당 같은 작업에서 포인터 memcpy 등 처리)
- 일정 크기 이상(bigCopyAlert)의 메모리 작업은 경고
- 구현 순서 : ast2의 선언만 위상 정렬로 추가 -> 함수 본문 채우기
- 일부 expr/stat은 preStats가 필요 : A3Gen의 버퍼를 사용하여 사전실행문을 채우고 최종적으로 통합

- literal_data는 미리 변수를 선언하고 초기화하는 stat으로 변환 -ok
- 문자열 슬라이스는 문자열 포인터를 받아 슬라이스 구조체를 생성 -ok
- 삼항연산자 안에서 prestat 발생 시 if-else로 변환
- 논리연산자 안에서 prestat 발생 시 단축평가 고려하여 if-else로 변환
- rvalue에 & 취하는 경우 임시변수 할당

- 가변인자함수는 void*[] (void*들의 슬라이스)로 포장하여 전달 -ok
- 가변인자함수 인자로 값 타입이 들어간다면 임시변수 선언하고 복사 후 해당 변수의 주소를 취해 넣을 것 -ok
- 함수 호출 시 인자에 함수 호출을 포함하는 side-effect 있는 인자는 순서에 맞춰서 미리 임시변수에 할당하고 넣을 것 -ok
- 함수 호출 시 반환값이 배열이라면 반환값을 받을 임시변수를 선언하고 호출 마지막 인자로 전달 -ok

- 배열 대입 시 memcpy로 복사 (이름 = 배열반환함수/리터럴 이면 복사생략)
- 함수 본문에서, 인자로 들어오는 배열을 복사
- 함수 본문에서, 반환값이 배열이라면 호출 마지막 인자에 복사
- 함수 반환 리터럴 RVO
*/

// helper functions
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

std::unique_ptr<A3Expr> createArraySizeExpr(A3Type* t, A3Type* intType) {
    // len
    auto len = std::make_unique<A3ExprLiteral>();
    len->objType = A3ExprType::LITERAL;
    len->value = Literal(getArrayLen(t)); 
    len->exprType = intType;

    // sizeof
    auto sz = std::make_unique<A3ExprOperation>();
    sz->objType = A3ExprType::OPERATION;
    sz->subType = A3ExprOpType::U_SIZEOF;
    sz->typeOperand = getArrayDirect(t)->clone();
    sz->exprType = intType;

    // mul
    auto mul = std::make_unique<A3ExprOperation>();
    mul->objType = A3ExprType::OPERATION;
    mul->subType = A3ExprOpType::B_MUL;
    mul->operand0 = std::move(len);
    mul->operand1 = std::move(sz);
    mul->exprType = intType;
    return mul;
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
        prt.Log(std::format("W1901 large temporary variable ({} bytes) at {}", t->typeSize, getLocString(l)), 5); // W1901
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

// generate reference to variable
std::unique_ptr<A3ExprOperation> A3Gen::refVar(std::string name) {
    // take address of var
    auto addrOp = std::make_unique<A3ExprOperation>();
    addrOp->objType = A3ExprType::OPERATION;
    addrOp->subType = A3ExprOpType::U_REF;
    
    auto nameRef = std::make_unique<A3ExprName>();
    nameRef->objType = A3ExprType::VAR_NAME;
    nameRef->decl = findVar(name);
    nameRef->location = nameRef->decl->location;
    nameRef->exprType = nameRef->decl->type.get();
    addrOp->location = nameRef->location;
    addrOp->operand0 = std::move(nameRef);

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

// generate temporary variable assign, returns variable name
std::string A3Gen::setTempVar(A3Type* t, std::unique_ptr<A3Expr> v) {
    std::string tName = genTempVar(t, v->location);
    if (!isTypeEqual(t, v->exprType)) {
        throw std::runtime_error(std::format("E1902 tempVar type mismatch at {}", getLocString(v->location))); // E1902
    }

    // assign
    auto assign = std::make_unique<A3StatAssign>();
    assign->objType = A3StatType::ASSIGN;
    assign->location = v->location;
    assign->uid = uidCount++;
             
    auto left = std::make_unique<A3ExprName>();
    left->objType = A3ExprType::VAR_NAME;
    left->location = v->location;
    left->decl = findVar(tName);
    left->exprType = t;

    assign->left = std::move(left);
    assign->right = std::move(v);
    statBuf.push_back(std::move(assign));
    return tName;
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

                // add call preStat
                auto statExpr = std::make_unique<A3StatExpr>();
                statExpr->objType = A3StatType::EXPR;
                statExpr->location = e->location;
                statExpr->uid = uidCount++;
                statExpr->expr = std::move(resCall);
                statBuf.push_back(std::move(statExpr));

                // var_use of retName
                auto retUse = std::make_unique<A3ExprName>();
                retUse->objType = A3ExprType::VAR_NAME;
                retUse->location = e->location;
                retUse->decl = findVar(retName);
                retUse->exprType = retType;
                res = std::move(retUse);

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

                // add call preStat
                auto statExpr = std::make_unique<A3StatExpr>();
                statExpr->objType = A3StatType::EXPR;
                statExpr->location = e->location;
                statExpr->uid = uidCount++;
                statExpr->expr = std::move(resCall);
                statBuf.push_back(std::move(statExpr));

                // var_use of retName
                auto retUse = std::make_unique<A3ExprName>();
                retUse->objType = A3ExprType::VAR_NAME;
                retUse->location = e->location;
                retUse->decl = findVar(retName);
                retUse->exprType = retType;
                res = std::move(retUse);

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
        auto len = std::make_unique<A3ExprLiteral>();
        len->objType = A3ExprType::LITERAL;
        len->value = Literal((int64_t)std::get<std::string>(l->value.value).size());
        len->exprType = typePool[0].get();
        r->operand1 = std::move(len);
        return r;

    } else { // string array or string pointer
        auto r = std::make_unique<A3ExprLiteral>();
        r->objType = A3ExprType::LITERAL;
        r->value = l->value;
        r->location = l->location;
        auto t = lowerType(l->exprType);
        int idx = findType(t.get());
        if (idx == -1) {
            idx = typePool.size();
            typePool.push_back(std::move(t));
        }
        r->exprType = typePool[idx].get();
        return r;
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
        
        // dst: NameRef
        auto dst = std::make_unique<A3ExprName>();
        dst->objType = A3ExprType::VAR_NAME;
        dst->location = e->location;
        dst->decl = findVar(*setName);
        dst->exprType = typePtr; // temp var type is array
        memSet->dst = std::move(dst);

        // size: arrlen * sizeof(direct), sizeHint : typeSize
        memSet->size = createArraySizeExpr(typePtr, typePool[0].get());
        memSet->sizeHint = typePtr->typeSize;
        statBuf.push_back(std::move(memSet)); // push back declaration

        // 4-1. set elements
        for (size_t i = 0; i < e->elements.size(); i++) {
            if (isZeroLiteral(e->elements[i].get())) continue;

            // assign
            auto assign = std::make_unique<A3StatAssign>();
            assign->objType = A3StatType::ASSIGN;
            assign->location = e->elements[i]->location;
            assign->uid = uidCount++;

            // left: temp[i]
            auto idxOp = std::make_unique<A3ExprOperation>();
            idxOp->objType = A3ExprType::OPERATION;
            idxOp->subType = A3ExprOpType::B_INDEX;
            idxOp->location = e->location;
            
            auto arrRef = std::make_unique<A3ExprName>();
            arrRef->objType = A3ExprType::VAR_NAME;
            arrRef->decl = findVar(*setName);
            arrRef->exprType = typePtr;
            idxOp->operand0 = std::move(arrRef);

            auto idxLit = std::make_unique<A3ExprLiteral>();
            idxLit->objType = A3ExprType::LITERAL;
            idxLit->value = Literal((int64_t)i);
            idxLit->exprType = typePool[0].get(); // int
            idxOp->operand1 = std::move(idxLit);

            idxOp->exprType = typePtr->direct.get(); // Array element type
            assign->left = std::move(idxOp);
            assign->right = lowerExpr(e->elements[i].get(), "");
            
            statBuf.push_back(std::move(assign)); // push back assignment
        }

    } else if (typePtr->objType == A3TypeType::STRUCT) { // 3-2. struct initialization
        for (size_t i = 0; i < e->elements.size(); i++) { // 4-2. set elements
            auto assign = std::make_unique<A3StatAssign>();
            assign->objType = A3StatType::ASSIGN;
            assign->location = e->elements[i]->location;
            assign->uid = uidCount++;

            // left: temp.i
            auto dotOp = std::make_unique<A3ExprOperation>();
            dotOp->objType = A3ExprType::OPERATION;
            dotOp->subType = A3ExprOpType::B_DOT;
            dotOp->location = e->location;
            dotOp->accessPos = i;

            auto strRef = std::make_unique<A3ExprName>();
            strRef->objType = A3ExprType::VAR_NAME;
            strRef->decl = findVar(*setName);
            strRef->exprType = typePtr;
            dotOp->operand0 = std::move(strRef);

            // infer element type from source A2Expr
            auto elemType = lowerType(e->elements[i]->exprType);
            int eIdx = findType(elemType.get());
            if (eIdx == -1) {
                eIdx = typePool.size();
                typePool.push_back(std::move(elemType));
            }
            dotOp->exprType = typePool[eIdx].get();

            assign->left = std::move(dotOp);
            assign->right = lowerExpr(e->elements[i].get(), "");
            statBuf.push_back(std::move(assign)); // push back assignment
        }
    }

    // 5. return name_use
    auto res = std::make_unique<A3ExprName>();
    res->objType = A3ExprType::VAR_NAME;
    res->location = e->location;
    res->decl = findVar(*setName);
    res->exprType = typePtr;
    return res;
}

// lowering expression operation
std::unique_ptr<A3Expr> A3Gen::lowerExprOp(A2ExprOperation* e) {
    auto newOp = std::make_unique<A3ExprOperation>();
    newOp->objType = A3ExprType::OPERATION;
    newOp->location = e->location;

    switch (e->subType) {
        // operations having same logic
        case A2ExprOpType::B_DOT: newOp->subType = A3ExprOpType::B_DOT; break;
        case A2ExprOpType::B_ARROW: newOp->subType = A3ExprOpType::B_ARROW; break;
        case A2ExprOpType::B_INDEX: newOp->subType = A3ExprOpType::B_INDEX; break;
        
        case A2ExprOpType::U_PLUS: newOp->subType = A3ExprOpType::U_PLUS; break;
        case A2ExprOpType::U_MINUS: newOp->subType = A3ExprOpType::U_MINUS; break;
        case A2ExprOpType::U_BIT_NOT: newOp->subType = A3ExprOpType::U_BIT_NOT; break;
        case A2ExprOpType::U_DEREF: newOp->subType = A3ExprOpType::U_DEREF; break;

        case A2ExprOpType::B_MUL: newOp->subType = A3ExprOpType::B_MUL; break;
        case A2ExprOpType::B_DIV: newOp->subType = A3ExprOpType::B_DIV; break;
        case A2ExprOpType::B_MOD: newOp->subType = A3ExprOpType::B_MOD; break;
        
        case A2ExprOpType::B_SHL: newOp->subType = A3ExprOpType::B_SHL; break;
        case A2ExprOpType::B_SHR: newOp->subType = A3ExprOpType::B_SHR; break;
        
        case A2ExprOpType::B_LT: newOp->subType = A3ExprOpType::B_LT; break;
        case A2ExprOpType::B_LE: newOp->subType = A3ExprOpType::B_LE; break;
        case A2ExprOpType::B_GT: newOp->subType = A3ExprOpType::B_GT; break;
        case A2ExprOpType::B_GE: newOp->subType = A3ExprOpType::B_GE; break;
        case A2ExprOpType::B_EQ: newOp->subType = A3ExprOpType::B_EQ; break;
        case A2ExprOpType::B_NE: newOp->subType = A3ExprOpType::B_NE; break;
        
        case A2ExprOpType::B_BIT_AND: newOp->subType = A3ExprOpType::B_BIT_AND; break;
        case A2ExprOpType::B_BIT_XOR: newOp->subType = A3ExprOpType::B_BIT_XOR; break;
        case A2ExprOpType::B_BIT_OR: newOp->subType = A3ExprOpType::B_BIT_OR; break;

        // number operations + pointer arithmetics
        case A2ExprOpType::B_ADD:
            if (e->operand0->exprType->objType == A2TypeType::POINTER || e->operand1->exprType->objType == A2TypeType::POINTER) {
                newOp->subType = A3ExprOpType::B_PTR_ADD;
            } else {
                newOp->subType = A3ExprOpType::B_ADD;
            }
            break;
        case A2ExprOpType::B_SUB:
            if (e->operand0->exprType->objType == A2TypeType::POINTER || e->operand1->exprType->objType == A2TypeType::POINTER) {
                newOp->subType = A3ExprOpType::B_PTR_SUB;
            } else {
                newOp->subType = A3ExprOpType::B_SUB;
            }
            break;

        // cast type, register
        case A2ExprOpType::B_CAST:
        {
            newOp->subType = A3ExprOpType::B_CAST;
            auto t = lowerType(e->typeOperand.get());
            int idx = findType(t.get());
            if (idx == -1) {
                idx = typePool.size();
                typePool.push_back(std::move(t));
            }
            newOp->typeOperand = typePool[idx]->clone();
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
                auto lit = std::make_unique<A3ExprLiteral>();
                lit->objType = A3ExprType::LITERAL;
                lit->location = e->location;
                lit->value = Literal((int64_t)e->operand0->exprType->arrLen);
                lit->exprType = typePool[0].get();
                return lit;
            } else {
                newOp->operand0 = lowerExpr(e->operand0.get(), "");
            }
            break;

        // make(&arr[st], ed-st)
        case A2ExprOpType::T_SLICE:
        {
            
        }
        
        case A2ExprOpType::T_COND:
        case A2ExprOpType::B_LOGIC_AND:
        case A2ExprOpType::B_LOGIC_OR:
        case A2ExprOpType::U_LOGIC_NOT:
        case A2ExprOpType::U_REF:
             throw std::runtime_error(std::format("E9999 operation {} skipped due to preStats issues", (int)e->subType));

        default:
            throw std::runtime_error(std::format("E9999 unsupported operation in ast3 {}", (int)e->subType));
    }

    // set result type of operation node
    auto t = lowerType(e->exprType);
    int idx = findType(t.get());
    if (idx == -1) {
        idx = typePool.size();
        typePool.push_back(std::move(t));
    }
    newOp->exprType = typePool[idx].get();
    return newOp;
}

// lowering expression call
std::vector<std::unique_ptr<A3Expr>> A3Gen::lowerExprCall(A3Type* ftype, std::vector<std::unique_ptr<A2Expr>>& a2Args, bool isVaArg, bool isRetArray, std::string* retName) {
    std::vector<std::unique_ptr<A3Expr>> a3Args;
    int fixArgCount = ftype->indirect.size();
    if (isVaArg) fixArgCount--;
    if (isRetArray) fixArgCount--;

    // 1. convert fixed arguments
    for (int i = 0; i < fixArgCount; i++) {
        auto argExpr = lowerExpr(a2Args[i].get(), "");

        // check side-effect, assign to temp if not literal/name
        if (argExpr->objType != A3ExprType::LITERAL && argExpr->objType != A3ExprType::VAR_NAME) {
            auto tName = setTempVar(argExpr->exprType, std::move(argExpr));
            auto nameUse = std::make_unique<A3ExprName>();
            nameUse->objType = A3ExprType::VAR_NAME;
            nameUse->decl = findVar(tName);
            nameUse->location = nameUse->decl->location;
            nameUse->exprType = nameUse->decl->type.get();
            a3Args.push_back(std::move(nameUse));
        } else {
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
                    auto tName = genTempVar(argExpr->exprType, argExpr->location);
                    
                    // memcpy(dst=temp, src=argExpr, size=typeSize)
                    auto memCpy = std::make_unique<A3StatMem>();
                    memCpy->objType = A3StatType::MEMCPY;
                    memCpy->location = argExpr->location;
                    memCpy->uid = uidCount++;
                    
                    auto dst = std::make_unique<A3ExprName>();
                    dst->objType = A3ExprType::VAR_NAME;
                    dst->location = argExpr->location;
                    dst->decl = findVar(tName);
                    dst->exprType = dst->decl->type.get();
                    memCpy->dst = std::move(dst);

                    memCpy->size = createArraySizeExpr(argExpr->exprType, typePool[0].get());
                    memCpy->sizeHint = dst->decl->type->typeSize;
                    memCpy->src = std::move(argExpr); // src is the array expr
                    statBuf.push_back(std::move(memCpy));

                    ptrExpr = refVar(tName); // take address of temp
                }
                break;

                default: // value type, set temp var, take address
                {
                    auto tName = setTempVar(argExpr->exprType, std::move(argExpr));
                    ptrExpr = refVar(tName); // take address of temp
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
                auto assign = std::make_unique<A3StatAssign>();
                assign->objType = A3StatType::ASSIGN;
                assign->location = varArgs[i]->location;
                assign->uid = uidCount++;

                auto left = std::make_unique<A3ExprOperation>();
                left->objType = A3ExprType::OPERATION;
                left->subType = A3ExprOpType::B_INDEX;
                left->location = varArgs[i]->location;

                auto arrRef = std::make_unique<A3ExprName>();
                arrRef->objType = A3ExprType::VAR_NAME;
                arrRef->decl = findVar(arrName);
                arrRef->exprType = arrRef->decl->type.get();
                left->operand0 = std::move(arrRef);

                auto idxLit = std::make_unique<A3ExprLiteral>();
                idxLit->objType = A3ExprType::LITERAL;
                idxLit->value = Literal((int64_t)i);
                idxLit->exprType = typePool[0].get();
                left->operand1 = std::move(idxLit);
                left->exprType = typePool[14].get(); // void*

                assign->left = std::move(left);
                assign->right = std::move(varArgs[i]);
                statBuf.push_back(std::move(assign));
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

            auto arrRef = std::make_unique<A3ExprName>();
            arrRef->objType = A3ExprType::VAR_NAME;
            arrRef->decl = findVar(arrName);
            arrRef->location = varArgs[0]->location;
            arrRef->exprType = arrRef->decl->type.get();
            idxOp->operand0 = std::move(arrRef);

            auto idxLit = std::make_unique<A3ExprLiteral>();
            idxLit->objType = A3ExprType::LITERAL;
            idxLit->value = Literal((int64_t)0);
            idxLit->exprType = typePool[0].get();
            idxOp->operand1 = std::move(idxLit);
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

            auto lenLit = std::make_unique<A3ExprLiteral>();
            lenLit->objType = A3ExprType::LITERAL;
            lenLit->location = varArgs[0]->location;
            lenLit->value = Literal((int64_t)vaArgCount);
            lenLit->exprType = typePool[0].get();
            makeOp->operand1 = std::move(lenLit);

            a3Args.push_back(std::move(makeOp));
        }
    }

    // 3. convert ret array
    if (isRetArray) {
        // decl ret var
        if (*retName == "") {
            std::string ret = genTempVar(ftype->direct.get(), ftype->location);
            *retName = ret;
        }

        // add ret var to args
        auto outArg = std::make_unique<A3ExprName>();
        outArg->objType = A3ExprType::VAR_NAME;
        outArg->location = ftype->location; 
        outArg->decl = findVar(*retName);
        outArg->exprType = outArg->decl->type.get();
        a3Args.push_back(std::move(outArg));
    }
    return a3Args;
}
