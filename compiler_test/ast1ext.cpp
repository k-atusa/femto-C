#include "ast1ext.h"

// helper functions
std::string A1Ext::getOrigin(A1Type* t, A1Module* m) {
    if (t->objType == A1TypeType::NAME) { // local name from module
        return m->uname;
    } else if (t->objType == A1TypeType::FOREIGN) { // foreign name from include
        A1DeclInclude* includeNode = static_cast<A1DeclInclude*>(m->findDeclaration(t->incName, A1DeclType::INCLUDE, false));
        if (includeNode == nullptr) {
            return "";
        }
        return includeNode->tgtUname;
    } else if (t->objType == A1TypeType::TEMPLATE) { // indirect incName
        if (t->incName.find("/") != std::string::npos) { // uname/incName
            int pos = t->incName.find("/");
            int idx = findModule(t->incName.substr(0, pos));
            if (idx != -1) { // from another module
                m = modules[idx].get();
            }
            A1DeclInclude* includeNode = static_cast<A1DeclInclude*>(m->findDeclaration(t->incName.substr(pos + 1), A1DeclType::INCLUDE, false));
            if (includeNode == nullptr) {
                return "";
            }
            return includeNode->tgtUname;
        } else { // uname
            return t->incName;
        }
    } else {
        return "";
    }
}

bool A1Ext::isTypeEqual(A1Type* A, A1Type* B, A1Module* modA, A1Module* modB) {
    if (A == nullptr ^ B == nullptr) return false;
    if (A == nullptr && B == nullptr) return true;
    if (A->typeSize != B->typeSize || A->typeAlign != B->typeAlign || A->name != B->name) return false;
    if (A->objType == A1TypeType::ARRAY) {
        if (A->objType != B->objType) return false;
        if (A->arrLen != B->arrLen) return false;
    } else if (A->objType == A1TypeType::PRIMITIVE || A->objType == A1TypeType::POINTER || A->objType == A1TypeType::SLICE || A->objType == A1TypeType::FUNCTION) {
        if (A->objType != B->objType) return false;
    } else {
        std::string originA = getOrigin(A, modA);
        std::string originB = getOrigin(B, modB);
        if (originA == "" || originB == "") return false; // both are before instantiated
        if (originA != originB) return false;
    }
    if (!isTypeEqual(A->direct.get(), B->direct.get(), modA, modB)) return false;
    for (int i = 0; i < A->indirect.size(); i++) {
        if (!isTypeEqual(A->indirect[i].get(), B->indirect[i].get(), modA, modB)) return false;
    }
    return true;
}

int A1Ext::findModule(const std::string& uname) {
    for (int i = 0; i < modules.size(); i++) {
        if (modules[i]->uname == uname) return i;
    }
    return -1;
}

int A1Ext::findModule(const std::string& path, std::vector<A1Type*> args, A1Module* caller) {
    for (int i = 0; i < modules.size(); i++) {
        if (modules[i]->path == path && modules[i]->tmpArgsCount == args.size()) {
            bool equal = true;
            for (int j = 0; j < modules[i]->tmpArgs.size(); j++) {
                if (!isTypeEqual(modules[i]->tmpArgs[j].get(), args[j], modules[i].get(), caller)) {
                    equal = false;
                    break;
                }
            }
            if (equal) return i;
        }
    }
    return -1;
}

bool A1Ext::completeType(A1Module& mod, A1Type& tgt) {
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
        // size of template type is already known
        case A1TypeType::ARRAY:
            if (tgt.direct->typeSize == 0) {
                throw std::runtime_error(std::format("E0801 cannot create array/slice of void type at {}", ast1->getLocString(tgt.location))); // E0801
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
                if (templateNode != nullptr) { // copy from template body
                    tgt.objType = templateNode->type->objType;
                    tgt.location = templateNode->type->location;
                    tgt.name = templateNode->type->name;
                    tgt.incName = templateNode->type->incName;
                    if (templateNode->type->direct != nullptr) {
                        tgt.direct = templateNode->type->direct->Clone();
                    }
                    for (auto& ind : templateNode->type->indirect) {
                        tgt.indirect.push_back(ind->Clone());
                    }
                    tgt.arrLen = templateNode->type->arrLen;
                    tgt.typeSize = templateNode->type->typeSize;
                    tgt.typeAlign = templateNode->type->typeAlign;
                    modified = true;
                }
                if (structNode == nullptr && enumNode == nullptr && templateNode == nullptr) {
                    throw std::runtime_error(std::format("E0802 type {} not found at {}", tgt.name, ast1->getLocString(tgt.location))); // E0802
                }
            }
            break;

        case A1TypeType::FOREIGN:
            {
                A1DeclInclude* includeNode = static_cast<A1DeclInclude*>(mod.findDeclaration(tgt.incName, A1DeclType::INCLUDE, false));
                if (includeNode == nullptr) {
                    throw std::runtime_error(std::format("E0803 include name {} not found at {}", tgt.name, ast1->getLocString(tgt.location))); // E0803
                }

                // find from A1Ext first
                int index = findModule(includeNode->tgtUname);
                A1Module* includeSrc = nullptr;
                if (index >= 0) {
                    includeSrc = modules[index].get();
                }

                // find from A1Gen
                if (includeSrc == nullptr) {
                    index = ast1->findModule(includeNode->tgtPath);
                    if (index >= 0) {
                        includeSrc = ast1->modules[index].get();
                    }
                }
                if (includeSrc == nullptr) {
                    throw std::runtime_error(std::format("E0804 included module {} not found at {}", includeNode->tgtUname, ast1->getLocString(tgt.location))); // E0804
                }

                // fetch foreign type size
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
                    throw std::runtime_error(std::format("E0805 type {}.{} not found at {}", includeNode->tgtUname, tgt.name, ast1->getLocString(tgt.location))); // E0805
                }
            }
            break;
        default: // no change
            break;
    }
    return modified;
}

bool A1Ext::completeStruct(A1Module& mod, A1DeclStruct& tgt) {
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
    prt.Log(std::format("calculated struct size {} at {}", tgt.name, ast1->getLocString(tgt.location)), 1);
    return true;
}

void A1Ext::standardizeType(A1Module& mod, A1Type& tgt) {
    if (tgt.direct) standardizeType(mod, *tgt.direct);
    for (auto& ind : tgt.indirect) {
        standardizeType(mod, *ind);
    }
    switch (tgt.objType) {
        // if type is template, it is already standardized
        case A1TypeType::NAME: // local name -> template uname
            tgt.objType = A1TypeType::TEMPLATE;
            tgt.incName = mod.uname;
            break;
        case A1TypeType::FOREIGN:
            tgt.objType = A1TypeType::TEMPLATE;
            tgt.incName = mod.uname + "/" + tgt.incName;
            break;
        default: // no change
            break;
    }
    if (tgt.typeSize < 0 || tgt.typeAlign < 0) {
        throw std::runtime_error(std::format("E0806 cannot standardize type {}", tgt.name)); // E0806
    }
}

// instanciate templates
std::string A1Ext::complete(std::unique_ptr<A1Module> mod, std::vector<std::unique_ptr<A1Type>> args) {
    // generate unique name
    std::string uname = mod->uname;
    int count = 0;
    bool found = true;
    while (found) {
        found = false;
        for (auto& s : modules) {
            if (s->uname == uname) {
                found = true;
                break;
            }
        }
        if (found) {
            uname = std::format("{}_{}", uname, count++);
        }
    }
    mod->uname = uname;

    // check template args, fill args
    if (mod->tmpArgsCount != args.size()) {
        return std::format("E0807 invalid template args while completing {}", mod->path); // E0807
    }
    int tmpIndex = 0;
    for (auto& node : mod->code->body) {
        if (node->objType == A1StatType::DECL) {
            A1StatDecl* dNode = static_cast<A1StatDecl*>(node.get());
            if (dNode->decl->objType == A1DeclType::TEMPLATE) {
                A1DeclTemplate* tmpNode = static_cast<A1DeclTemplate*>(dNode->decl.get());
                tmpNode->type = std::move(args[tmpIndex++]);
            }
        }
    }

    // get includes and structs
    std::vector<A1DeclInclude*> incNodes;
    std::vector<A1DeclStruct*> structNodes;
    for (auto& node : mod->code->body) {
        if (node->objType == A1StatType::DECL) {
            A1StatDecl* dNode = static_cast<A1StatDecl*>(node.get());
            if (dNode->decl->objType == A1DeclType::INCLUDE) {
                A1DeclInclude* incNode = static_cast<A1DeclInclude*>(dNode->decl.get());
                incNodes.push_back(incNode);
            } else if (dNode->decl->objType == A1DeclType::STRUCT) {
                A1DeclStruct* structNode = static_cast<A1DeclStruct*>(dNode->decl.get());
                structNodes.push_back(structNode);
            }
        }
    }

    // complete typesize calculation
    bool isModified = true;
    while (isModified) {
        // complete type sizes of includes
        isModified = false;
        for (auto node : incNodes) {
            if (node == nullptr) continue;
            for (auto& arg : node->argTypes) {
                try {
                    isModified = isModified | completeType(*mod, *arg);
                } catch (std::exception& e) {
                    return e.what();
                }
            }
        }

        // import includes, templates
        for (int i = 0; i < incNodes.size(); i++) {
            if (incNodes[i] == nullptr) continue;
            bool canImport = true;
            for (auto& arg : incNodes[i]->argTypes) {
                if (arg->typeSize == -1) {
                    canImport = false;
                    break;
                }
            }
            if (canImport) {
                std::vector<std::unique_ptr<A1Type>> tmps;
                std::vector<A1Type*> finds;
                for (auto& arg : incNodes[i]->argTypes) {
                    standardizeType(*mod, *arg);
                    tmps.push_back(arg->Clone());
                    finds.push_back(arg.get());
                }
                int idx = findModule(incNodes[i]->tgtPath, finds, mod.get());
                if (idx == -1) { // include only if not imported
                    idx = ast1->findModule(incNodes[i]->tgtPath);
                    if (idx == -1) {
                        return std::format("E0808 include {} not found at {}", incNodes[i]->name, ast1->getLocString(incNodes[i]->location)); // E0808
                    }
                    std::unique_ptr<A1Module> incMod = ast1->modules[idx]->Clone();
                    std::string err = complete(std::move(incMod), std::move(tmps));
                    if (!err.empty()) {
                        return err;
                    }
                    idx = modules.size() - 1;
                }
                incNodes[i]->tgtUname = modules[idx]->uname;
                incNodes[i] = nullptr;
                isModified = true;
            }
        }

        // complete struct sizes
        for (auto& structNode : structNodes) {
            if (structNode->structSize > 0) continue;
            try {
                isModified = isModified | completeStruct(*mod, *structNode);
            } catch (std::exception& e) {
                return e.what();
            }
        }
    }
    prt.Log(std::format("pass4 finished for source {}", mod->uname), 2);

    // check includes, structs
    for (auto& incNode : incNodes) {
        if (incNode != nullptr) {
            return std::format("E0809 tmpArgs of include {} size undecidable at {}", incNode->name, ast1->getLocString(incNode->location)); // E0809
        }
    }
    for (auto& structNode : structNodes) {
        if (structNode->structSize <= 0) {
            return std::format("E0810 struct {} size undecidable at {}", structNode->name, ast1->getLocString(structNode->location)); // E0810
        }
    }

    // finish completing
    prt.Log(std::format("finished completing source {} as {}", mod->path, mod->uname), 3);
    modules.push_back(std::move(mod));
    return "";
}
