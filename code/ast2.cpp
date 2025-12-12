#include "ast2.h"
#include <stdexcept>
#include "ast1.h"
#include "baseFunc.h"

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

// helper for type pool initialization
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
                throw std::runtime_error(std::format("E0901 cannot find name {} at {}", t->name, getLocString(t->location))); // E0901
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
                throw std::runtime_error(std::format("E0902 cannot convert name {} at {}", t->name, getLocString(t->location))); // E0902
            }
        }
        break;

        case A1TypeType::FOREIGN:
        {
            A1Decl* decl = mod->findDeclaration(t->incName, false);
            if (decl == nullptr || decl->objType != A1DeclType::INCLUDE) {
                throw std::runtime_error(std::format("E0903 cannot find include {} at {}", t->incName, getLocString(t->location))); // E0903
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
    std::unique_ptr<A2Expr> res;

    switch (e->objType) {
        case A1ExprType::LITERAL:
        {
            A1ExprLiteral* lit = static_cast<A1ExprLiteral*>(e);
            std::unique_ptr<A2ExprLiteral> newLit = std::make_unique<A2ExprLiteral>(lit->value);
            newLit->location = e->location;
            switch (lit->value.objType) { // infer type
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
                    throw std::runtime_error(std::format("E1001 invalid literal at {}", getLocString(lit->location))); // E1001
            }
            if (expectedType != nullptr) { // check convertability
                switch (expectedType->objType) {
                    case A2TypeType::PRIMITIVE: // int, float, bool
                        if (lit->value.objType == LiteralType::INT) {
                            if (!(expectedType->name == "int" || expectedType->name == "i8" || expectedType->name == "i16"
                                    || expectedType->name == "i32" || expectedType->name == "i64" || expectedType->name == "uint"
                                    || expectedType->name == "u8" || expectedType->name == "u16" || expectedType->name == "u32"
                                    || expectedType->name == "u64")) {
                                throw std::runtime_error(std::format("E1002 cannot convert int literal to {} at {}", expectedType->name, getLocString(lit->location))); // E1002
                            }
                        } else if (lit->value.objType == LiteralType::FLOAT) {
                            if (!(expectedType->name == "float" || expectedType->name == "f32" || expectedType->name == "f64")) {
                                throw std::runtime_error(std::format("E1003 cannot convert float literal to {} at {}", expectedType->name, getLocString(lit->location))); // E1003
                            }
                        } else if (lit->value.objType == LiteralType::BOOL) {
                            if (!(expectedType->name == "bool")) {
                                throw std::runtime_error(std::format("E1004 cannot convert bool literal to {} at {}", expectedType->name, getLocString(lit->location))); // E1004
                            }
                        } else {
                            throw std::runtime_error(std::format("E1005 cannot convert literal to {} at {}", expectedType->name, getLocString(lit->location))); // E1005
                        }
                        break;
                    case A2TypeType::POINTER: // nptr, string
                        if (lit->value.objType == LiteralType::STRING) {
                            if (!isTypeEqual(expectedType->direct.get(), typePool[6].get())) {
                                throw std::runtime_error(std::format("E1006 cannot convert string literal to {} at {}", expectedType->toString(), getLocString(lit->location))); // E1006
                            }
                        } else if (lit->value.objType != LiteralType::NPTR) {
                            throw std::runtime_error(std::format("E1007 cannot convert literal to {} at {}", expectedType->toString(), getLocString(lit->location))); // E1007
                        }
                        break;
                    case A2TypeType::ARRAY: // string
                        if (lit->value.objType == LiteralType::STRING) {
                            if (!isTypeEqual(expectedType->direct.get(), typePool[6].get())) {
                                throw std::runtime_error(std::format("E1008 cannot convert string literal to {} at {}", expectedType->toString(), getLocString(lit->location))); // E1008
                            }
                        } else {
                            throw std::runtime_error(std::format("E1009 cannot convert literal to {} at {}", expectedType->toString(), getLocString(lit->location))); // E1009
                        }
                        break;
                    case A2TypeType::SLICE: // string
                        if (lit->value.objType == LiteralType::STRING) {
                            if (!isTypeEqual(expectedType->direct.get(), typePool[6].get())) {
                                throw std::runtime_error(std::format("E1010 cannot convert string literal to {} at {}", expectedType->toString(), getLocString(lit->location))); // E1010
                            }
                        } else {
                            throw std::runtime_error(std::format("E1011 cannot convert literal to {} at {}", expectedType->toString(), getLocString(lit->location))); // E1011
                        }
                        break;
                    case A2TypeType::FUNCTION: // nptr
                        if (lit->value.objType != LiteralType::NPTR) {
                            throw std::runtime_error(std::format("E1012 cannot convert literal to {} at {}", expectedType->toString(), getLocString(lit->location))); // E1012
                        }
                        break;
                    default:
                        throw std::runtime_error(std::format("E1013 cannot convert literal at {}", getLocString(lit->location))); // E1013
                }
                newLit->exprType = expectedType;
            }
            res = std::move(newLit);
            break;
        }

        case A1ExprType::LITERAL_DATA:
        {
            if (expectedType == nullptr) {
                throw std::runtime_error(std::format("E1014 need type expection for literal data at {}", getLocString(e->location))); // E1014
            }
            A1ExprLiteralData* data = static_cast<A1ExprLiteralData*>(e);
            std::unique_ptr<A2ExprLiteralData> newData = std::make_unique<A2ExprLiteralData>();
            newData->location = e->location;
            if (expectedType->objType == A2TypeType::STRUCT) { // check struct elements
                A2Decl* decl = modules[findModule(expectedType->modUname)]->nameMap[expectedType->name];
                if (decl->objType != A2DeclType::STRUCT) {
                    throw std::runtime_error(std::format("E1015 {}.{} is not found at {}", expectedType->modUname, expectedType->name, getLocString(e->location))); // E1015
                }
                A2DeclStruct* sDecl = static_cast<A2DeclStruct*>(decl);
                if (sDecl->memTypes.size() != data->elements.size()) {
                    throw std::runtime_error(std::format("E1016 {}.{} has {} elements but {} was given at {}", expectedType->modUname, expectedType->name, sDecl->memTypes.size(), data->elements.size(), getLocString(e->location))); // E1016
                }
                for (size_t i = 0; i < sDecl->memTypes.size(); i++) {
                    newData->elements.push_back(convertExpr(data->elements[i].get(), mod, sDecl->memTypes[i].get()));
                }
            } else if (expectedType->objType == A2TypeType::SLICE || expectedType->objType == A2TypeType::ARRAY) { // check slice, array elements
                if (expectedType->objType == A2TypeType::ARRAY && expectedType->arrLen != data->elements.size()) {
                    throw std::runtime_error(std::format("E1017 expected {} elements but {} was given at {}", expectedType->arrLen, data->elements.size(), getLocString(e->location))); // E1017
                }
                for (size_t i = 0; i < data->elements.size(); i++) {
                    newData->elements.push_back(convertExpr(data->elements[i].get(), mod, expectedType->direct.get()));
                }
            } else {
                throw std::runtime_error(std::format("E1018 cannot convert literal data to {} at {}", expectedType->toString(), getLocString(e->location))); // E1018
            }
            newData->exprType = expectedType;
            res = std::move(newData);
            break;
        }

        case A1ExprType::NAME: // single name is global/local variable, function //////////
        {
            A1ExprName* nm = static_cast<A1ExprName*>(e);
            std::unique_ptr<A2ExprName> newName = std::make_unique<A2ExprName>();
            newName->location = e->location;
            A2DeclVar* vDecl = findVar(nm->name); // local var
            A1Decl* decl = mod->findDeclaration(nm->name, false); // global/function
            if (!vDecl && !decl) {
                throw std::runtime_error(std::format("E1019 {} is not found at {}", nm->name, getLocString(e->location))); // E1019
            }
            newName->decl = vDecl;
            newName->exprType = vDecl->type.get();
            if (expectedType && !isTypeEqual(expectedType, newName->exprType)) {
                throw std::runtime_error(std::format("E1020 cannot convert {} to {} at {}", nm->name, expectedType->toString(), getLocString(e->location))); // E1020
            }
            res = std::move(newName);
            break;
        }

        case A1ExprType::OPERATION: // op, inc.name, struct.method, inc.struct.method
        {
            A1ExprOperation* op = static_cast<A1ExprOperation*>(e);
            std::unique_ptr<A2ExprOperation> newOp = std::make_unique<A2ExprOperation>();
            newOp->subType = (A2ExprOpType)op->subType;
            
            if (op->operand0) newOp->operand0 = convertExpr(op->operand0.get(), mod, nullptr);
            if (op->operand1) newOp->operand1 = convertExpr(op->operand1.get(), mod, nullptr);
            if (op->operand2) newOp->operand2 = convertExpr(op->operand2.get(), mod, nullptr);
            if (op->typeOperand) newOp->typeOperand = convertType(op->typeOperand.get(), mod);
            
            if (newOp->operand0 && newOp->operand0->exprType) {
                newOp->exprType = newOp->operand0->exprType->Clone().release();
                if ((int)newOp->subType >= (int)A2ExprOpType::B_LT && (int)newOp->subType <= (int)A2ExprOpType::B_NE) {
                     newOp->exprType->objType = A2TypeType::PRIMITIVE;
                     newOp->exprType->name = "bool";
                }
                if (newOp->subType == A2ExprOpType::U_REF) {
                     A2Type* base = newOp->exprType;
                     newOp->exprType = new A2Type(A2TypeType::POINTER, "");
                     newOp->exprType->direct.reset(base);
                }
                if (newOp->subType == A2ExprOpType::U_DEREF) {
                     if (newOp->exprType->objType == A2TypeType::POINTER && newOp->exprType->direct) {
                         auto t = newOp->exprType->direct->Clone();
                         delete newOp->exprType;
                         newOp->exprType = t.release();
                         newOp->isLvalue = true;
                     }
                }
            }
            res = std::move(newOp);
            break;
        }

        case A1ExprType::FUNC_CALL: // struct.method, inc.struct.method
        {
            A1ExprFuncCall* call = static_cast<A1ExprFuncCall*>(e);
            std::unique_ptr<A2ExprFuncCall> newCall = std::make_unique<A2ExprFuncCall>();
            newCall->func = convertExpr(call->func.get(), mod, nullptr);
            for (auto& arg : call->args) {
                newCall->args.push_back(convertExpr(arg.get(), mod, nullptr));
            }
            
            if (newCall->func->exprType && newCall->func->exprType->objType == A2TypeType::FUNCTION) {
                 if (newCall->func->exprType->direct)
                    newCall->exprType = newCall->func->exprType->direct->Clone().release();
            }
            res = std::move(newCall);
            break;
        }
        default: throw std::runtime_error(std::format("E3007 unsupported expression {} at {}", (int)e->objType, getLocString(e->location)));
    }
    
    res->location = e->location;
    return res;
}
