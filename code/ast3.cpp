#include "ast3.h"
#include <stdexcept>

/*
lowering 주요 변환규칙
- A3의 배열은 값 타입처럼 표현되나 실제로는 포인터로 취급 (할당 같은 작업에서 포인터 memcpy 등 처리)
- 일정 크기 이상(bigCopyAlert)의 메모리 작업은 경고
- 구현 순서 : ast2의 선언만 위상 정렬로 추가 -> 함수 본문 채우기
- 일부 expr/stat은 preStats가 필요 : A3Gen의 버퍼를 사용하여 사전실행문을 채우고 최종적으로 통합

- literal_data는 미리 변수를 선언하고 초기화하는 stat으로 변환
- 문자열 슬라이스는 문자열 포인터를 받아 슬라이스 구조체를 생성
- 삼항연산자 안에서 prestat 발생 시 if-else로 변환
- 논리연산자 안에서 prestat 발생 시 단축평가 고려하여 if-else로 변환
- rvalue인 메서드 호출 (func().method() 등)은 임시변수로 변환

- 가변인자함수는 void*[] (void*들의 슬라이스)로 포장하여 전달
- 가변인자함수 인자로 값 타입이 들어간다면 임시변수 선언하고 해당 변수의 주소를 취해 넣을 것
- 함수 호출 시 인자에 함수 호출을 포함하는 side-effect 있는 인자는 순서에 맞춰서 미리 임시변수에 할당하고 넣을 것
- 함수 호출 시 반환값이 배열이라면 반환값을 받을 임시변수를 선언하고 호출 마지막 인자로 전달

- 배열 대입 시 memcpy로 복사
- 함수 본문에서, 인자로 들어오는 배열을 복사
- 함수 본문에서, 반환값이 배열이라면 호출 마지막 인자에 복사
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

// initialize type pool
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
                newType->indirect.push_back(newType->direct->clone());
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
            throw std::runtime_error(std::format("E2002 invalid type {} at {}", (int)t->objType, getLocString(t->location)));
    }
    return newType;
}

std::unique_ptr<A3Expr> A3Gen::lowerExpr(A2Expr* e) {
    if (!e) return nullptr;
    std::unique_ptr<A3Expr> res = nullptr;

    switch (e->objType) {
        case A2ExprType::LITERAL:
        {
            auto lit = (A2ExprLiteral*)e;
            auto r = std::make_unique<A3ExprLiteral>();
            r->objType = A3ExprType::LITERAL;
            r->value = lit->value;
            res = std::move(r);
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
            return lowerExprLitData((A2ExprLiteralData*)e);
        case A2ExprType::OPERATION:
            return lowerExprOp((A2ExprOperation*)e);
        case A2ExprType::FUNC_CALL:
            return lowerExprCall(e);
        case A2ExprType::FPTR_CALL:
            return lowerExprCall(e);
        default:
            throw std::runtime_error("E2103 invalid expression type");
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

std::unique_ptr<A3Expr> lowerExprLitData(A2ExprLiteralData* e) {

}

std::unique_ptr<A3Expr> lowerExprOp(A2ExprOperation* e) {

}

std::unique_ptr<A3Expr> lowerExprCall(A2Expr* e) {

}
