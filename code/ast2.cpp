#include "ast2.h"

// helper functions
bool isTypeEqual(A2Type* a, A2Type* b) {
    if (a == nullptr ^ b == nullptr) return false;
    if (a == nullptr && b == nullptr) return true;
    if (a->typeSize != b->typeSize || a->typeAlign != b->typeAlign) return false;
    if (a->objType == A2TypeType::ARRAY) {
        if (a->arrLen != b->arrLen) return false;
    }
    if (a->objType == A2TypeType::ARRAY || a->objType == A2TypeType::POINTER || a->objType == A2TypeType::SLICE
            || a->objType == A2TypeType::FUNCTION || a->objType == A2TypeType::PRIMITIVE) {
        if (a->objType != b->objType || a->name != b->name) return false;
    }
    if (a->objType == A2TypeType::STRUCT || a->objType == A2TypeType::ENUM) {
        if (a->objType != b->objType || a->name != b->name || a->modUname != b->modUname) return false;
    }
    if (!isTypeEqual(a->direct.get(), b->direct.get())) return false;
    for (size_t i = 0; i < a->indirect.size(); i++) {
        if (!isTypeEqual(a->indirect[i].get(), b->indirect[i].get())) return false;
    }
    return true;
}

std::string funcArgCheck(A2Type* func, bool isVaArg, std::vector<A2Type*> args, std::string loc) {
    int count0 = func->indirect.size();
    int count1 = args.size();
    if (isVaArg) count0--;
    if (count0 > count1) return std::format("E0901 need {} arguments but {} was given at {}", count0, count1, loc); // E0901
    for (size_t i = 0; i < count0; i++) {
        if (!isTypeEqual(func->indirect[i].get(), args[i])) return std::format("E0902 arg[{}] need {} but {} was given at {}", i, func->indirect[i]->toString(), args[i]->toString(), loc); // E0902
    }
    return "";
}

bool isSint(A2Type* t) {
    if (t->objType != A2TypeType::PRIMITIVE) return false;
    if (t->name != "int" && t->name != "i8" && t->name != "i16" && t->name != "i32" && t->name != "i64") return false;
    return true;
}

bool isUint(A2Type* t) {
    if (t->objType != A2TypeType::PRIMITIVE) return false;
    if (t->name != "uint" && t->name != "u8" && t->name != "u16" && t->name != "u32" && t->name != "u64") return false;
    return true;
}

bool isFloat(A2Type* t) {
    if (t->objType != A2TypeType::PRIMITIVE) return false;
    if (t->name != "float" && t->name != "f32" && t->name != "f64") return false;
    return true;
}

bool isBool(A2Type* t) {
    if (t->objType != A2TypeType::PRIMITIVE) return false;
    if (t->name != "bool") return false;
    return true;
}

std::unique_ptr<A2Type> getPrimitiveType(const std::string& name, int size, int align) {
    auto type = std::make_unique<A2Type>(A2TypeType::PRIMITIVE, name);
    type->typeSize = size;
    type->typeAlign = align;
    return type;
}

void A2Gen::initTypePool() {
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
    auto voidPtrType = std::make_unique<A2Type>(A2TypeType::POINTER, "*");
    voidPtrType->typeSize = arch; 
    voidPtrType->typeAlign = arch;
    voidPtrType->direct = getPrimitiveType("void", 0, 1);
    typePool.push_back(std::move(voidPtrType)); // typePool[14]

    // Add u8[] type
    auto u8ArrayType = std::make_unique<A2Type>(A2TypeType::SLICE, "u8");
    u8ArrayType->typeSize = arch * 2;
    u8ArrayType->typeAlign = arch;
    u8ArrayType->direct = getPrimitiveType("u8", 1, 1);
    typePool.push_back(std::move(u8ArrayType)); // typePool[15]
}

int A2Gen::nameCheck(const std::string& name, A1Module* mod, Location loc) { // 0 incName, 1 structName, 2 enumName, 3 varName, 4 funcName
    if (findVar(name) != nullptr) return 3;
    A1Decl* decl = mod->findDeclaration(name, false);
    if (decl != nullptr) {
        if (decl->objType == A1DeclType::INCLUDE) return 0;
        if (decl->objType == A1DeclType::STRUCT) return 1;
        if (decl->objType == A1DeclType::ENUM) return 2;
        if (decl->objType == A1DeclType::VAR) return 3;
        if (decl->objType == A1DeclType::FUNC) return 4;
    }
    throw std::runtime_error(std::format("E0903 undefined name {} at {}", name, getLocString(loc))); // E0903
}

// main conversion of type
std::unique_ptr<A2Type> A2Gen::convertType(A1Type* t, A1Module* mod) {
    std::unique_ptr<A2Type> newType = std::make_unique<A2Type>();
    newType->location = t->location;
    newType->name = t->name;
    newType->typeSize = t->typeSize;
    newType->typeAlign = t->typeAlign;

    switch (t->objType) {
        case A1TypeType::NONE: case A1TypeType::AUTO:
            return newType;

        case A1TypeType::PRIMITIVE:
            newType->objType = A2TypeType::PRIMITIVE;
            return newType;

        case A1TypeType::POINTER:
            newType->objType = A2TypeType::POINTER;
            newType->direct = convertType(t->direct.get(), mod);
            return newType;

        case A1TypeType::ARRAY:
            newType->objType = A2TypeType::ARRAY;
            newType->direct = convertType(t->direct.get(), mod);
            newType->arrLen = t->arrLen;
            return newType;

        case A1TypeType::SLICE:
            newType->objType = A2TypeType::SLICE;
            newType->direct = convertType(t->direct.get(), mod);
            return newType;

        case A1TypeType::FUNCTION:
            newType->objType = A2TypeType::FUNCTION;
            newType->direct = convertType(t->direct.get(), mod);
            for (auto& ind : t->indirect) {
                newType->indirect.push_back(convertType(ind.get(), mod));
            }
            return newType;

        case A1TypeType::NAME:
        {
            A1Decl* decl = mod->findDeclaration(t->name, false);
            if (decl == nullptr) {
                throw std::runtime_error(std::format("E1001 undefined name {} at {}", t->name, getLocString(t->location))); // E1001
            }
            if (decl->objType == A1DeclType::STRUCT) {
                newType->objType = A2TypeType::STRUCT;
                newType->modUname = mod->uname;
                return newType;
            } else if (decl->objType == A1DeclType::ENUM) {
                newType->objType = A2TypeType::ENUM;
                newType->modUname = mod->uname;
                return newType;
            } else if (decl->objType == A1DeclType::TEMPLATE) {
                return convertType(decl->type.get(), mod);
            } else {
                throw std::runtime_error(std::format("E1002 cannot convert name {} at {}", t->name, getLocString(t->location))); // E1002
            }
        }
        break;

        case A1TypeType::FOREIGN:
        {
            A1Decl* decl = mod->findDeclaration(t->incName, false);
            if (decl == nullptr || decl->objType != A1DeclType::INCLUDE) {
                throw std::runtime_error(std::format("E1003 undefined include {} at {}", t->incName, getLocString(t->location))); // E1003
            }
            t->objType = A1TypeType::NAME;
            newType = convertType(t, ast1->modules[ast1->findModule(static_cast<A1DeclInclude*>(decl)->tgtUname)].get());
            t->objType = A1TypeType::FOREIGN;
        }
        break;

        case A1TypeType::TEMPLATE:
            if (t->incName.contains("/")) {
                int pos = t->incName.find("/");
                std::string modNm = t->incName.substr(0, pos);
                std::string incNm = t->incName.substr(pos + 1);
                t->objType = A1TypeType::FOREIGN;
                t->incName = incNm;
                newType = convertType(t, ast1->modules[ast1->findModule(modNm)].get());
                t->objType = A1TypeType::TEMPLATE;
                t->incName = modNm + "/" + incNm;
            } else {
                t->objType = A1TypeType::NAME;
                newType = convertType(t, ast1->modules[ast1->findModule(t->incName)].get());
                t->objType = A1TypeType::TEMPLATE;
            }
            break;
    }
    return newType;
}

// main conversion of expr
std::unique_ptr<A2Expr> A2Gen::convertExpr(A1Expr* e, A1Module* mod, A2Type* expectedType) {
    if (e == nullptr) return nullptr;
    switch (e->objType) {
        case A1ExprType::LITERAL:
            return convertLiteralExpr(static_cast<A1ExprLiteral*>(e), expectedType);

        case A1ExprType::LITERAL_DATA:
        {
            if (expectedType == nullptr) {
                throw std::runtime_error(std::format("E1114 need type expection for literal data at {}", getLocString(e->location))); // E1114
            }
            A1ExprLiteralData* data = static_cast<A1ExprLiteralData*>(e);
            std::unique_ptr<A2ExprLiteralData> newData = std::make_unique<A2ExprLiteralData>();
            newData->location = e->location;

            if (expectedType->objType == A2TypeType::STRUCT) { // check struct elements
                A2Decl* decl = modules[findModule(expectedType->modUname)]->nameMap[expectedType->name];
                if (decl == nullptr || decl->objType != A2DeclType::STRUCT) {
                    throw std::runtime_error(std::format("E1115 undefined struct {}.{} at {}", expectedType->modUname, expectedType->name, getLocString(e->location))); // E1115
                }
                A2DeclStruct* sDecl = static_cast<A2DeclStruct*>(decl);
                if (sDecl->memTypes.size() != data->elements.size()) {
                    throw std::runtime_error(std::format("E1116 {}.{} has {} members but {} was given at {}", expectedType->modUname, expectedType->name, sDecl->memTypes.size(), data->elements.size(), getLocString(e->location))); // E1116
                }
                for (size_t i = 0; i < sDecl->memTypes.size(); i++) {
                    newData->elements.push_back(convertExpr(data->elements[i].get(), mod, sDecl->memTypes[i].get()));
                }

            } else if (expectedType->objType == A2TypeType::SLICE || expectedType->objType == A2TypeType::ARRAY) { // check slice, array elements
                if (expectedType->objType == A2TypeType::ARRAY && expectedType->arrLen != data->elements.size()) {
                    throw std::runtime_error(std::format("E1117 expected {} elements but {} was given at {}", expectedType->arrLen, data->elements.size(), getLocString(e->location))); // E1117
                }
                for (size_t i = 0; i < data->elements.size(); i++) {
                    newData->elements.push_back(convertExpr(data->elements[i].get(), mod, expectedType->direct.get()));
                }

            } else {
                throw std::runtime_error(std::format("E1118 cannot convert literal data to {} at {}", expectedType->toString(), getLocString(e->location))); // E1118
            }
            newData->exprType = expectedType;
            return std::move(newData);
        }

        case A1ExprType::NAME: // single name is variable or function
        {
            std::unique_ptr<A2Expr> res;
            A1ExprName* nm = static_cast<A1ExprName*>(e);
            std::unique_ptr<A2ExprName> newName = std::make_unique<A2ExprName>();
            newName->location = e->location;
            A2DeclVar* vDecl = findVar(nm->name);

            if (vDecl == nullptr) { // function
                A2Decl* decl = curModule->nameMap[nm->name];
                if (decl == nullptr || decl->objType != A2DeclType::FUNC) {
                    throw std::runtime_error(std::format("E1119 {} is not found at {}", nm->name, getLocString(e->location))); // E1119
                }
                A2DeclFunc* fDecl = static_cast<A2DeclFunc*>(decl);
                newName->exprType = fDecl->type.get();
                newName->decl = fDecl;
                res = std::move(newName);

            } else { // global/local var
                newName->exprType = vDecl->type.get();
                newName->decl = vDecl;
                if (!vDecl->isConst && !vDecl->isDefine) newName->isLvalue = true;
                res = std::move(newName);
            }

            if (expectedType && !isTypeEqual(expectedType, newName->exprType)) { // check type
                throw std::runtime_error(std::format("E1120 expected type {}, but {} at {}", expectedType->toString(), newName->exprType->toString(), getLocString(e->location))); // E1120
            }
            return res;
        }

        case A1ExprType::OPERATION: // op, namespace access, member access
        {
            std::unique_ptr<A2Expr> res;
            A1ExprOperation* op = static_cast<A1ExprOperation*>(e);
            if (op->subType == A1ExprOpType::B_DOT) {
                res = convertDotExpr(op, mod); // dot op, right side is always name
            } else {
                res = convertOpExpr(op, mod); // normal op
            }
            if (expectedType && !isTypeEqual(expectedType, res->exprType)) { // check type
                throw std::runtime_error(std::format("E1121 expected type {}, but {} at {}", expectedType->toString(), res->exprType->toString(), getLocString(e->location))); // E1121
            }
            return res;
        }

        case A1ExprType::FUNC_CALL: // static func, func ptr, method call
        {
            std::unique_ptr<A2Expr> res;
            A1ExprFuncCall* fcall = static_cast<A1ExprFuncCall*>(e);
            res = convertFuncCallExpr(fcall, mod);
            if (expectedType && !isTypeEqual(expectedType, res->exprType)) { // check type
                throw std::runtime_error(std::format("E1122 expected type {}, but {} at {}", expectedType->toString(), res->exprType->toString(), getLocString(e->location))); // E1122
            }
            return res;
        }

        default:
            throw std::runtime_error(std::format("E1123 unsupported expression {} at {}", (int)e->objType, getLocString(e->location))); // E1123
    }
}

// convert literal expr
std::unique_ptr<A2ExprLiteral> A2Gen::convertLiteralExpr(A1ExprLiteral* lit, A2Type* expectedType) {
    std::unique_ptr<A2ExprLiteral> newLit = std::make_unique<A2ExprLiteral>(lit->value);
    newLit->location = lit->location;

    // infer type
    switch (lit->value.objType) {
        case LiteralType::INT:
            newLit->exprType = typePool[0].get();
            break;
        case LiteralType::FLOAT:
            newLit->exprType = typePool[11].get();
            break;
        case LiteralType::STRING:
            newLit->exprType = typePool[15].get();
            break;
        case LiteralType::BOOL:
            newLit->exprType = typePool[12].get();
            break;
        case LiteralType::NPTR:
            newLit->exprType = typePool[14].get();
            break;
        default:
            throw std::runtime_error(std::format("E1101 invalid literal at {}", getLocString(lit->location))); // E1101
    }

    // check convertability
    if (expectedType != nullptr) {
        switch (expectedType->objType) {
            case A2TypeType::PRIMITIVE: // int, float, bool
                if (lit->value.objType == LiteralType::INT) {
                    if (!(isSint(expectedType) || isUint(expectedType))) {
                        throw std::runtime_error(std::format("E1102 cannot convert int literal to {} at {}", expectedType->toString(), getLocString(lit->location))); // E1102
                    }
                } else if (lit->value.objType == LiteralType::FLOAT) {
                    if (!(isFloat(expectedType))) {
                        throw std::runtime_error(std::format("E1103 cannot convert float literal to {} at {}", expectedType->toString(), getLocString(lit->location))); // E1103
                    }
                } else if (lit->value.objType == LiteralType::BOOL) {
                    if (!(isBool(expectedType))) {
                        throw std::runtime_error(std::format("E1104 cannot convert bool literal to {} at {}", expectedType->toString(), getLocString(lit->location))); // E1104
                    }
                } else {
                    throw std::runtime_error(std::format("E1105 cannot convert literal to {} at {}", expectedType->toString(), getLocString(lit->location))); // E1105
                }
                break;

            case A2TypeType::POINTER: // nptr, string
                if (lit->value.objType == LiteralType::STRING) {
                    if (!isTypeEqual(expectedType->direct.get(), typePool[6].get())) {
                        throw std::runtime_error(std::format("E1106 cannot convert string literal to {} at {}", expectedType->toString(), getLocString(lit->location))); // E1106
                    }
                } else if (lit->value.objType != LiteralType::NPTR) {
                    throw std::runtime_error(std::format("E1107 cannot convert literal to {} at {}", expectedType->toString(), getLocString(lit->location))); // E1107
                }
                break;

            case A2TypeType::ARRAY: // string
                if (lit->value.objType == LiteralType::STRING) {
                    if (!isTypeEqual(expectedType->direct.get(), typePool[6].get())) {
                        throw std::runtime_error(std::format("E1108 cannot convert string literal to {} at {}", expectedType->toString(), getLocString(lit->location))); // E1108
                    }
                } else {
                    throw std::runtime_error(std::format("E1109 cannot convert literal to {} at {}", expectedType->toString(), getLocString(lit->location))); // E1109
                }
                break;

            case A2TypeType::SLICE: // string
                if (lit->value.objType == LiteralType::STRING) {
                    if (!isTypeEqual(expectedType->direct.get(), typePool[6].get())) {
                        throw std::runtime_error(std::format("E1110 cannot convert string literal to {} at {}", expectedType->toString(), getLocString(lit->location))); // E1110
                    }
                } else {
                    throw std::runtime_error(std::format("E1111 cannot convert literal to {} at {}", expectedType->toString(), getLocString(lit->location))); // E1111
                }
                break;

            case A2TypeType::FUNCTION: // nptr
                if (lit->value.objType != LiteralType::NPTR) {
                    throw std::runtime_error(std::format("E1112 cannot convert literal to {} at {}", expectedType->toString(), getLocString(lit->location))); // E1112
                }
                break;

            default:
                throw std::runtime_error(std::format("E1113 cannot convert literal at {}", getLocString(lit->location))); // E1113
        }
        newLit->exprType = expectedType;
    }
    return newLit;
}

// convert dot operation expr
std::unique_ptr<A2Expr> A2Gen::convertDotExpr(A1ExprOperation* op, A1Module* mod) {
    std::unique_ptr<A2Expr> lhs;
    std::string rname = static_cast<A1ExprName*>(op->operand1.get())->name;

    // step 1. resolve lhs to A2Expr
    if (op->operand0->objType == A1ExprType::NAME) { // 1-1. left is name
        std::string lname = static_cast<A1ExprName*>(op->operand0.get())->name;
        int nc = nameCheck(lname, mod, op->location);
        switch (nc) {
            case 0: // 1-1-1. inc.structName, inc.enumName, inc.varName, inc.funcName
            {
                A2Decl* decl = modules[findModule(lname)]->nameMap[rname];
                if (decl == nullptr || rname[0] < 'A' || rname[0] > 'Z') {
                    throw std::runtime_error(std::format("E1124 {}.{} is not found at {}", lname, rname, getLocString(op->location))); // E1124
                }
                std::unique_ptr<A2ExprName> newName = std::make_unique<A2ExprName>();
                newName->location = op->location;
                newName->decl = decl;
                newName->exprType = decl->type.get();
                switch (decl->objType) {
                case A2DeclType::VAR:
                    newName->objType = A2ExprType::VAR_NAME;
                    if (!static_cast<A2DeclVar*>(decl)->isConst && !static_cast<A2DeclVar*>(decl)->isDefine) newName->isLvalue = true;
                    break;
                case A2DeclType::FUNC:
                    newName->objType = A2ExprType::FUNC_NAME;
                    break;
                case A2DeclType::STRUCT:
                    newName->objType = A2ExprType::STRUCT_NAME;
                    break;
                case A2DeclType::ENUM:
                    newName->objType = A2ExprType::ENUM_NAME;
                    break;
                }
                return newName;
            }
            
            case 1: // 1-1-2. structName.x
            {
                std::unique_ptr<A2ExprName> newName = std::make_unique<A2ExprName>();
                newName->location = op->location;
                newName->decl = modules[findModule(mod->uname)]->nameMap[lname];
                newName->exprType = newName->decl->type.get();
                newName->objType = A2ExprType::STRUCT_NAME;
                lhs = std::move(newName);
            }
            break;

            case 2: // 1-1-3. enumName.x
            {
                std::unique_ptr<A2ExprName> newName = std::make_unique<A2ExprName>();
                newName->location = op->location;
                newName->decl = modules[findModule(mod->uname)]->nameMap[lname];
                newName->exprType = newName->decl->type.get();
                newName->objType = A2ExprType::ENUM_NAME;
                lhs = std::move(newName);
            }
            break;

            default: // 1-1-4. varName.x, funcName.x
                lhs = convertExpr(op->operand0.get(), mod, nullptr);
            break;
        }
    } else { // 1-2. left is expr
        lhs = convertExpr(op->operand0.get(), mod, nullptr);
    }

    // step 2. resolve op_dot based on lhs type
    switch (lhs->objType) {
        case A2ExprType::STRUCT_NAME: // 2-1. structName.method
        {
            // check visibility
            if (rname[0] < 'A' || rname[0] > 'Z') {
                if (lhs->exprType->modUname != curFunc->modUname || lhs->exprType->name != curFunc->structNm) {
                    throw std::runtime_error(std::format("E1127 {} is private at {}", rname, getLocString(op->location))); // E1127
                }
            }

            // find method
            A2Decl* sDecl = static_cast<A2ExprName*>(lhs.get())->decl;
            A2Module* targetMod = modules[findModule(sDecl->modUname)].get();
            if (targetMod->nameMap.count(sDecl->name + "." + rname) == 0) {
                throw std::runtime_error(std::format("E1125 {}.{} is not found at {}", sDecl->name, rname, getLocString(op->location))); // E1125
            }
            A2DeclFunc* fDecl = static_cast<A2DeclFunc*>(targetMod->nameMap[sDecl->name + "." + rname]);
            
            std::unique_ptr<A2ExprName> newName = std::make_unique<A2ExprName>();
            newName->location = op->location;
            newName->decl = fDecl;
            newName->exprType = fDecl->type.get();
            newName->objType = A2ExprType::FUNC_NAME;
            return newName;
        }

        case A2ExprType::ENUM_NAME: // 2-2. enumName.member
        {
            // check visibility
            if (rname[0] < 'A' || rname[0] > 'Z') {
                if (lhs->exprType->modUname != curFunc->modUname) {
                    throw std::runtime_error(std::format("E1127 {} is private at {}", rname, getLocString(op->location))); // E1127
                }
            }

            // find member
            A2DeclEnum* eDecl = static_cast<A2DeclEnum*>(static_cast<A2ExprName*>(lhs.get())->decl);
            auto it = std::find(eDecl->memNames.begin(), eDecl->memNames.end(), rname);
            if (it == eDecl->memNames.end()) {
                throw std::runtime_error(std::format("E1126 {}.{} is not found at {}", eDecl->name, rname, getLocString(op->location))); // E1126
            }

            std::unique_ptr<A2ExprLiteral> newLiteral = std::make_unique<A2ExprLiteral>();
            newLiteral->location = op->location;
            newLiteral->value = Literal(eDecl->memValues[std::distance(eDecl->memNames.begin(), it)]);
            newLiteral->exprType = eDecl->type.get();
            return newLiteral;
        }

        default: // 2-3. inst.member
        {
            // check valid instance type
            A2Type* structType = nullptr;
            A2ExprOpType opType = A2ExprOpType::NONE;
            if (lhs->exprType->objType == A2TypeType::STRUCT) {
                structType = lhs->exprType;
                opType = A2ExprOpType::B_DOT;
            } else if (lhs->exprType->objType == A2TypeType::POINTER && lhs->exprType->direct->objType == A2TypeType::STRUCT) {
                structType = lhs->exprType->direct.get();
                opType = A2ExprOpType::B_ARROW;
            } else {
                throw std::runtime_error(std::format("E1127 invalid access .{} at {}", rname, getLocString(op->location))); // E1127
            }

            // check visibility
            if (rname[0] < 'A' || rname[0] > 'Z') {
                if (structType->modUname != curFunc->modUname || structType->name != curFunc->structNm) {
                    throw std::runtime_error(std::format("E1127 {} is private at {}", rname, getLocString(op->location))); // E1127
                }
            }

            // resolve member
            A2DeclStruct* sDecl = static_cast<A2DeclStruct*>(modules[findModule(structType->modUname)]->nameMap[structType->name]);
            if (sDecl == nullptr || sDecl->objType != A2DeclType::STRUCT) {
                throw std::runtime_error(std::format("E1129 struct {} not found at {}", structType->name, getLocString(op->location))); // E1129
            }
            auto it = std::find(sDecl->memNames.begin(), sDecl->memNames.end(), rname);
            if (it == sDecl->memNames.end()) {
                throw std::runtime_error(std::format("E1130 member {} not found in {} at {}", rname, structType->name, getLocString(op->location))); // E1130
            }
            int index = std::distance(sDecl->memNames.begin(), it);

            std::unique_ptr<A2ExprOperation> newOp = std::make_unique<A2ExprOperation>(opType);
            newOp->location = op->location;
            newOp->operand0 = std::move(lhs);
            newOp->accessPos = index;
            newOp->exprType = sDecl->memTypes[index].get();
            if (opType == A2ExprOpType::B_ARROW) {
                newOp->isLvalue = true;
            } else if (newOp->operand0->isLvalue) {
                newOp->isLvalue = true;
            }
            return newOp;
        }
    }
}

// convert extra operation expr
std::unique_ptr<A2Expr> A2Gen::convertOpExpr(A1ExprOperation* op, A1Module* mod) {
    std::unique_ptr<A2ExprOperation> newOp = std::make_unique<A2ExprOperation>();
    newOp->location = op->location;
    switch (op->subType) {
        case A1ExprOpType::B_INDEX: // op0[op1]
        {
            newOp->subType = A2ExprOpType::B_INDEX;
            newOp->operand0 = convertExpr(op->operand0.get(), mod, nullptr);
            newOp->operand1 = convertExpr(op->operand1.get(), mod, nullptr);

            // check op0 type
            A2Type* t0 = newOp->operand0->exprType;
            if (t0->objType == A2TypeType::ARRAY || t0->objType == A2TypeType::SLICE) {
                newOp->exprType = t0->direct.get();
                if (t0->objType == A2TypeType::ARRAY && newOp->operand0->isLvalue) newOp->isLvalue = true; // array element lvalue if array is lvalue
                if (t0->objType == A2TypeType::SLICE) newOp->isLvalue = true; // slice element always lvalue
            } else if (t0->objType == A2TypeType::POINTER) {
                newOp->exprType = t0->direct.get();
                newOp->isLvalue = true;
            } else {
                throw std::runtime_error(std::format("E1132 cannot index type {} at {}", t0->toString(), getLocString(op->location))); // E1132
            }

            // check op1 type
            if (!isSint(newOp->operand1->exprType) && !isUint(newOp->operand1->exprType)) {
                 throw std::runtime_error(std::format("E1131 index must be integer at {}", getLocString(op->location))); // E1131
            }
            return std::move(newOp);
        }

        case A1ExprOpType::U_PLUS: case A1ExprOpType::U_MINUS:
        {
            newOp->subType = (op->subType == A1ExprOpType::U_PLUS) ? A2ExprOpType::U_PLUS : A2ExprOpType::U_MINUS;
            newOp->operand0 = convertExpr(op->operand0.get(), mod, nullptr);
            A2Type* t0 = newOp->operand0->exprType;
            if (!isSint(t0) && !isUint(t0) && !isFloat(t0)) {
                throw std::runtime_error(std::format("E1133 invalid type {} for unary op at {}", t0->toString(), getLocString(op->location))); // E1133
            }
            newOp->exprType = t0;
            return std::move(newOp);
        }

        case A1ExprOpType::U_BIT_NOT:
        {
            newOp->subType = A2ExprOpType::U_BIT_NOT;
            newOp->operand0 = convertExpr(op->operand0.get(), mod, nullptr);
            A2Type* t0 = newOp->operand0->exprType;
            if (!isSint(t0) && !isUint(t0)) {
                throw std::runtime_error(std::format("E1134 invalid type {} for bit-not at {}", t0->toString(), getLocString(op->location))); // E1134
            }
            newOp->exprType = t0;
            return std::move(newOp);
        }

        case A1ExprOpType::U_LOGIC_NOT:
        {
            newOp->subType = A2ExprOpType::U_LOGIC_NOT;
            newOp->operand0 = convertExpr(op->operand0.get(), mod, typePool[12].get()); // expect bool
            newOp->exprType = typePool[12].get();
            return std::move(newOp);
        }

        case A1ExprOpType::U_REF:
        {
            newOp->subType = A2ExprOpType::U_REF;
            newOp->operand0 = convertExpr(op->operand0.get(), mod, nullptr);
            if (!newOp->operand0->isLvalue) {
                throw std::runtime_error(std::format("E1136 cannot take address of r-value at {}", getLocString(op->location))); // E1136
            }
            
            auto ptrType = std::make_unique<A2Type>(A2TypeType::POINTER, "*");
            ptrType->typeSize = arch;
            ptrType->typeAlign = arch;
            ptrType->direct = newOp->operand0->exprType->Clone();
            int idx = findType(ptrType.get()); // findType needs A2Type*
            if (idx == -1) {
                typePool.push_back(std::move(ptrType));
                newOp->exprType = typePool.back().get();
            } else {
                newOp->exprType = typePool[idx].get();
            }
            return std::move(newOp);
        }

        case A1ExprOpType::U_DEREF:
        {
            newOp->subType = A2ExprOpType::U_DEREF;
            newOp->operand0 = convertExpr(op->operand0.get(), mod, nullptr);
            A2Type* t0 = newOp->operand0->exprType;
            if (t0->objType != A2TypeType::POINTER) {
                throw std::runtime_error(std::format("E1137 cannot dereference non-pointer type {} at {}", t0->toString(), getLocString(op->location))); // E1137
            }
            newOp->exprType = t0->direct.get();
            if (newOp->exprType->name == "void") {
                throw std::runtime_error(std::format("E1138 cannot dereference void* at {}", getLocString(op->location))); // E1138
            }
            newOp->isLvalue = true;
            return std::move(newOp);
        }

        case A1ExprOpType::B_MUL: case A1ExprOpType::B_DIV: case A1ExprOpType::B_MOD:
        case A1ExprOpType::B_ADD: case A1ExprOpType::B_SUB:
        {
            switch(op->subType) {
                case A1ExprOpType::B_MUL: newOp->subType = A2ExprOpType::B_MUL; break;
                case A1ExprOpType::B_DIV: newOp->subType = A2ExprOpType::B_DIV; break;
                case A1ExprOpType::B_MOD: newOp->subType = A2ExprOpType::B_MOD; break;
                case A1ExprOpType::B_ADD: newOp->subType = A2ExprOpType::B_ADD; break;
                case A1ExprOpType::B_SUB: newOp->subType = A2ExprOpType::B_SUB; break;
                default: break;
            }
            newOp->operand0 = convertExpr(op->operand0.get(), mod, nullptr);
            
            // get expected type for operand1
            A2Type* expected1 = newOp->operand0->exprType;
            if (newOp->subType == A2ExprOpType::B_ADD || newOp->subType == A2ExprOpType::B_SUB) {
                if (expected1->objType == A2TypeType::POINTER) {
                    expected1 = nullptr; // ptr +/- int
                } else if (op->operand1->objType != A1ExprType::LITERAL) {
                    expected1 = nullptr; // Don't enforce type for non-literals to allow Int + Ptr
                }
            }
            newOp->operand1 = convertExpr(op->operand1.get(), mod, expected1);
            
            // check type if pointer arithmetic
            if ((newOp->subType == A2ExprOpType::B_ADD || newOp->subType == A2ExprOpType::B_SUB) && 
                    newOp->operand0->exprType->objType == A2TypeType::POINTER) {
                if (!isSint(newOp->operand1->exprType) && !isUint(newOp->operand1->exprType)) {
                     throw std::runtime_error(std::format("E1140 invalid type {} for pointer arithmetic at {}", newOp->operand1->exprType->toString(), getLocString(op->location))); // E1140
                }
                newOp->exprType = newOp->operand0->exprType;
                return std::move(newOp);
            } else if (newOp->subType == A2ExprOpType::B_ADD && newOp->operand1->exprType->objType == A2TypeType::POINTER) {
                if (isSint(newOp->operand0->exprType) || isUint(newOp->operand0->exprType)) {
                    std::swap(newOp->operand0, newOp->operand1); // int + ptr -> ptr + int
                    newOp->exprType = newOp->operand0->exprType;
                    return std::move(newOp);
                }
            }

            // check type if normal arithmetic
            if (!isTypeEqual(newOp->operand0->exprType, newOp->operand1->exprType)) {
                throw std::runtime_error(std::format("E1139 type mismatch {} and {} at {}", newOp->operand0->exprType->toString(), newOp->operand1->exprType->toString(), getLocString(op->location))); // E1139
            }
            A2Type* t0 = newOp->operand0->exprType;
            if (!isSint(t0) && !isUint(t0) && !isFloat(t0)) {
                throw std::runtime_error(std::format("E1140 invalid type {} for arithmetic op at {}", t0->toString(), getLocString(op->location))); // E1140
            }
            if (newOp->subType == A2ExprOpType::B_MOD && isFloat(t0)) {
                throw std::runtime_error(std::format("E1141 cannot use modulo with float at {}", getLocString(op->location))); // E1141
            }
            newOp->exprType = t0;
            return std::move(newOp);
        }

        case A1ExprOpType::B_SHL: case A1ExprOpType::B_SHR:
        case A1ExprOpType::B_BIT_AND: case A1ExprOpType::B_BIT_XOR: case A1ExprOpType::B_BIT_OR:
        {
            switch(op->subType) {
                case A1ExprOpType::B_SHL: newOp->subType = A2ExprOpType::B_SHL; break;
                case A1ExprOpType::B_SHR: newOp->subType = A2ExprOpType::B_SHR; break;
                case A1ExprOpType::B_BIT_AND: newOp->subType = A2ExprOpType::B_BIT_AND; break;
                case A1ExprOpType::B_BIT_XOR: newOp->subType = A2ExprOpType::B_BIT_XOR; break;
                case A1ExprOpType::B_BIT_OR: newOp->subType = A2ExprOpType::B_BIT_OR; break;
                default: break;
            }
            newOp->operand0 = convertExpr(op->operand0.get(), mod, nullptr);
            newOp->operand1 = convertExpr(op->operand1.get(), mod, newOp->operand0->exprType); // expect same type
            
            // check type
            A2Type* t0 = newOp->operand0->exprType;
            if (!isSint(t0) && !isUint(t0)) {
                throw std::runtime_error(std::format("E1143 invalid type {} for bitwise op at {}", t0->toString(), getLocString(op->location))); // E1143
            }
            newOp->exprType = t0;
            return std::move(newOp);
        }

        case A1ExprOpType::B_LT: case A1ExprOpType::B_LE: case A1ExprOpType::B_GT: case A1ExprOpType::B_GE:
        {
            switch(op->subType) {
                case A1ExprOpType::B_LT: newOp->subType = A2ExprOpType::B_LT; break;
                case A1ExprOpType::B_LE: newOp->subType = A2ExprOpType::B_LE; break;
                case A1ExprOpType::B_GT: newOp->subType = A2ExprOpType::B_GT; break;
                case A1ExprOpType::B_GE: newOp->subType = A2ExprOpType::B_GE; break;
                default: break;
            }
            newOp->operand0 = convertExpr(op->operand0.get(), mod, nullptr);
            newOp->operand1 = convertExpr(op->operand1.get(), mod, newOp->operand0->exprType); // expect same type
            
            // check type
            A2Type* t0 = newOp->operand0->exprType;
            if (!isSint(t0) && !isUint(t0) && !isFloat(t0)) {
                throw std::runtime_error(std::format("E1145 invalid type {} for comparison at {}", t0->toString(), getLocString(op->location))); // E1145
            }
            newOp->exprType = typePool[12].get(); // bool
            return std::move(newOp);
        }

        case A1ExprOpType::B_EQ: case A1ExprOpType::B_NE:
        {
            newOp->subType = (op->subType == A1ExprOpType::B_EQ) ? A2ExprOpType::B_EQ : A2ExprOpType::B_NE;
            newOp->operand0 = convertExpr(op->operand0.get(), mod, nullptr);
            newOp->operand1 = convertExpr(op->operand1.get(), mod, newOp->operand0->exprType); // expect same type
            
            // check type
            A2Type* t0 = newOp->operand0->exprType;
            if (t0->objType != A2TypeType::PRIMITIVE && t0->objType != A2TypeType::POINTER
                    && t0->objType != A2TypeType::FUNCTION && t0->objType != A2TypeType::ENUM) {
                throw std::runtime_error(std::format("E1145 invalid type {} for comparison at {}", t0->toString(), getLocString(op->location))); // E1145
            }
            newOp->exprType = typePool[12].get(); // bool
            return std::move(newOp);
        }

        case A1ExprOpType::B_LOGIC_AND: case A1ExprOpType::B_LOGIC_OR:
        {
            newOp->subType = (op->subType == A1ExprOpType::B_LOGIC_AND) ? A2ExprOpType::B_LOGIC_AND : A2ExprOpType::B_LOGIC_OR;
            newOp->operand0 = convertExpr(op->operand0.get(), mod, typePool[12].get());
            newOp->operand1 = convertExpr(op->operand1.get(), mod, typePool[12].get()); // expect bool for both
            newOp->exprType = typePool[12].get(); // bool
            return std::move(newOp);
        }
        
        case A1ExprOpType::U_SIZEOF: // sizeof(type), sizeof(expr)
        {
            newOp->subType = A2ExprOpType::U_SIZEOF;
            if (op->typeOperand != nullptr) {
                newOp->typeOperand = convertType(op->typeOperand.get(), mod);
            } else {
                newOp->typeOperand = convertExpr(op->operand0.get(), mod, nullptr)->exprType->Clone();
            }
            newOp->exprType = typePool[0].get(); // int
            return std::move(newOp);
        }

        case A1ExprOpType::U_LEN: // len(arr), len(slice)
        {
            newOp->subType = A2ExprOpType::U_LEN;
            newOp->operand0 = convertExpr(op->operand0.get(), mod, nullptr);
            A2Type* t0 = newOp->operand0->exprType;
            if (t0->objType != A2TypeType::ARRAY && t0->objType != A2TypeType::SLICE) {
                throw std::runtime_error(std::format("E1149 len() requires array or slice at {}", getLocString(op->location))); // E1149
            }
            newOp->exprType = typePool[0].get(); // int
            return std::move(newOp);
        }

        case A1ExprOpType::B_CAST: // cast<type>(expr)
        {
            newOp->subType = A2ExprOpType::B_CAST;
            if (op->typeOperand == nullptr) throw std::runtime_error("E1150 cast without type info at " + getLocString(op->location)); // E1150
            newOp->typeOperand = convertType(op->typeOperand.get(), mod);
            newOp->operand0 = convertExpr(op->operand0.get(), mod, nullptr);
            
            // check typecast validity
            A2Type* from = newOp->operand0->exprType;
            A2Type* to = newOp->typeOperand.get();
            bool valid = false;
            if ((isSint(from) || isUint(from) || isFloat(from)) && (isSint(to) || isUint(to) || isFloat(to))) valid = true; // num <-> num
            else if (from->objType == A2TypeType::POINTER && to->objType == A2TypeType::POINTER) valid = true; // ptr <-> ptr
            else if ((isSint(from) || isUint(from)) && to->objType == A2TypeType::POINTER) valid = true; // int -> ptr
            else if (from->objType == A2TypeType::POINTER && (isSint(to) || isUint(to))) valid = true; // ptr -> int
            if (!valid) {
                throw std::runtime_error(std::format("E1151 cannot cast {} to {} at {}", from->toString(), to->toString(), getLocString(op->location))); // E1151
            }
            newOp->exprType = to;
            return std::move(newOp);
        }

        case A1ExprOpType::B_MAKE: // make(ptr, int)
        {
            newOp->subType = A2ExprOpType::B_MAKE;
            newOp->operand0 = convertExpr(op->operand0.get(), mod, nullptr);
            newOp->operand1 = convertExpr(op->operand1.get(), mod, nullptr);

            // check op0 type
            A2Type* t0 = newOp->operand0->exprType;
            if (t0->objType != A2TypeType::POINTER) {
                throw std::runtime_error(std::format("E1153 make() requires pointer as arg[0] at {}", getLocString(op->location))); // E1153
            }
            if (t0->direct->name == "void" && t0->direct->objType == A2TypeType::PRIMITIVE) { // void* check
                 throw std::runtime_error(std::format("E1155 cannot make slice from void* at {}", getLocString(op->location))); // E1155
            }
            
            // check op1 type
            if (!isSint(newOp->operand1->exprType) && !isUint(newOp->operand1->exprType)) {
                 throw std::runtime_error(std::format("E1152 make() requires integer as arg[1] at {}", getLocString(op->location))); // E1152
            }

            auto sliceType = std::make_unique<A2Type>(A2TypeType::SLICE, "[]");
            sliceType->direct = t0->direct->Clone();
            sliceType->typeSize = arch * 2;
            sliceType->typeAlign = arch;
            int idx = findType(sliceType.get());
            if (idx == -1) {
                typePool.push_back(std::move(sliceType));
                newOp->exprType = typePool.back().get();
            } else {
                newOp->exprType = typePool[idx].get();
            }
            return std::move(newOp);
        }

        default:
            throw std::runtime_error(std::format("E1156 unknown op {} at {}", (int)op->subType, getLocString(op->location))); // E1156
    }
}

// convert function call expression
std::unique_ptr<A2Expr> A2Gen::convertFuncCallExpr(A1ExprFuncCall* fcall, A1Module* mod) {
    // step 1. check if method call
    bool isMethod = false;
    std::unique_ptr<A2Expr> instanceExpr = nullptr;
    std::string methodName;
    A2DeclFunc* methodDecl = nullptr;
    if (fcall->func->objType == A1ExprType::OPERATION) { // check if fcall is op_dot
        A1ExprOperation* op = static_cast<A1ExprOperation*>(fcall->func.get());
        if (op->subType == A1ExprOpType::B_DOT) { // possible method call
            bool isInstance = true;
            if (op->operand0->objType == A1ExprType::NAME) { // check if lhs is instance
                std::string lname = static_cast<A1ExprName*>(op->operand0.get())->name;
                int nc = nameCheck(lname, mod, op->location);
                if (nc < 3) isInstance = false; // inc, struct, enum (not instance)
            }
            
            if (isInstance) { // possible method call, convert lhs
                auto lhs = convertExpr(op->operand0.get(), mod, nullptr);
                A2Type* t = lhs->exprType;
                A2Type* structType = nullptr;
                if (t->objType == A2TypeType::STRUCT) {
                    structType = t;
                } else if (t->objType == A2TypeType::POINTER && t->direct->objType == A2TypeType::STRUCT) {
                    structType = t->direct.get();
                }
                
                if (structType) { // possible method call
                    std::string rname = static_cast<A1ExprName*>(op->operand1.get())->name;
                    int targetModIdx = findModule(structType->modUname);
                    if (targetModIdx != -1) { // find structName.method
                        std::string funcName = structType->name + "." + rname;
                        A2Module* tMod = modules[targetModIdx].get();
                        if (tMod->nameMap.count(funcName)) {
                            A2Decl* decl = tMod->nameMap[funcName];
                            if (decl->objType == A2DeclType::FUNC) { // it is a method
                                isMethod = true;
                                instanceExpr = std::move(lhs);
                                methodName = funcName;
                                methodDecl = static_cast<A2DeclFunc*>(decl);

                                // check visibility
                                if (rname[0] < 'A' || rname[0] > 'Z') {
                                    if (structType->modUname != curFunc->modUname || structType->name != curFunc->structNm) {
                                        throw std::runtime_error(std::format("E1127 {} is private at {}", rname, getLocString(op->location))); // E1127
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (isMethod) { // case 1. method call
        std::unique_ptr<A2ExprFuncCall> newCall = std::make_unique<A2ExprFuncCall>();
        newCall->location = fcall->location;
        newCall->func = methodDecl;
            
        // handle this argument
        if (instanceExpr->exprType->objType == A2TypeType::STRUCT) {
            if (!instanceExpr->isLvalue) {
                throw std::runtime_error(std::format("E1157 cannot call method on rvalue struct at {}", getLocString(fcall->location))); // E1157
            }
            auto refOp = std::make_unique<A2ExprOperation>(A2ExprOpType::U_REF);
            refOp->location = instanceExpr->location;
            refOp->operand0 = std::move(instanceExpr);

            auto ptrType = std::make_unique<A2Type>(A2TypeType::POINTER, "*");
            ptrType->typeSize = arch;
            ptrType->typeAlign = arch;
            ptrType->direct = refOp->operand0->exprType->Clone();
            int idx = findType(ptrType.get());
            if (idx == -1) {
                typePool.push_back(std::move(ptrType));
                refOp->exprType = typePool.back().get();
            } else {
                refOp->exprType = typePool[idx].get();
            }
            newCall->args.push_back(std::move(refOp));
        } else {
            newCall->args.push_back(std::move(instanceExpr));
        }
            
        // process other args
        for (size_t i = 0; i < fcall->args.size(); i++) {
            A2Type* expected = (i + 1 < methodDecl->paramTypes.size()) ? methodDecl->paramTypes[i + 1].get() : nullptr;
            newCall->args.push_back(convertExpr(fcall->args[i].get(), mod, expected));
        }
            
        // check args type
        std::vector<A2Type*> argTypes;
        for (auto& arg : newCall->args) argTypes.push_back(arg->exprType);
        std::string err = funcArgCheck(methodDecl->type.get(), methodDecl->isVaArg, argTypes, getLocString(fcall->location));
        if (!err.empty()) throw std::runtime_error(err);
        newCall->exprType = methodDecl->retType.get();
        return std::move(newCall);

    } else { // case 2. normal call
        auto funcExpr = convertExpr(fcall->func.get(), mod, nullptr);
        if (funcExpr->objType == A2ExprType::FUNC_NAME) { // case 2-1. static func call
            A2ExprName* nm = static_cast<A2ExprName*>(funcExpr.get());
            A2DeclFunc* fDecl = static_cast<A2DeclFunc*>(nm->decl);
            std::unique_ptr<A2ExprFuncCall> newCall = std::make_unique<A2ExprFuncCall>();
            newCall->location = fcall->location;
            newCall->func = fDecl;

            // process args
            for (size_t i = 0; i < fcall->args.size(); i++) {
                A2Type* expected = (i < fDecl->paramTypes.size()) ? fDecl->paramTypes[i].get() : nullptr;
                newCall->args.push_back(convertExpr(fcall->args[i].get(), mod, expected));
            }

            // check args type
            std::vector<A2Type*> argTypes;
            for (auto& arg : newCall->args) argTypes.push_back(arg->exprType);
            std::string err = funcArgCheck(fDecl->type.get(), fDecl->isVaArg, argTypes, getLocString(fcall->location));
            if (!err.empty()) throw std::runtime_error(err);
            newCall->exprType = fDecl->retType.get();
            return std::move(newCall);

        } else if (funcExpr->exprType->objType == A2TypeType::FUNCTION) { // case 2-2. fptr call
            A2Type* fType = funcExpr->exprType;
            std::unique_ptr<A2ExprFptrCall> newCall = std::make_unique<A2ExprFptrCall>();
            newCall->location = fcall->location;
            newCall->fptr = std::move(funcExpr);

            // process args
            for (size_t i = 0; i < fcall->args.size(); i++) {
                A2Type* expected = (i < fType->indirect.size()) ? fType->indirect[i].get() : nullptr;
                newCall->args.push_back(convertExpr(fcall->args[i].get(), mod, expected));
            }

            // check args type
            std::vector<A2Type*> argTypes;
            for (auto& arg : newCall->args) argTypes.push_back(arg->exprType);
            std::string err = funcArgCheck(fType->direct.get(), false, argTypes, getLocString(fcall->location));
            if (!err.empty()) throw std::runtime_error(err);
            newCall->exprType = fType->direct.get();
            return std::move(newCall);

        } else {
            throw std::runtime_error(std::format("E1160 not callable type {} at {}", funcExpr->exprType->toString(), getLocString(fcall->location))); // E1160
        }
    }
}