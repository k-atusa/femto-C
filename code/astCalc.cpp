#include <format>
#include "astGen.h"
#include "astCalc.h"

// helper functions
int ASTCalc::findSource(const std::string& path, std::vector<int>& tmpSizes, std::vector<int>& tmpAligns) {
    for (int i = 0; i < srcTrees.size(); i++) {
        if (srcTrees[i]->path == path && srcSizes[i] == tmpSizes && srcAligns[i] == tmpAligns) return i;
    }
    return -1;
}

bool ASTCalc::completeType(SrcFile& src, TypeNode& tgt) {
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
                throw std::runtime_error(std::format("E0801 cannot create array/slice of void type at {}", getLocString(tgt.location))); // E0801
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
                    throw std::runtime_error(std::format("E0802 type {} not found at {}", tgt.name, getLocString(tgt.location))); // E0802
                }
            }
            break;

        case TypeNodeType::FOREIGN:
            {
                IncludeNode* includeNode = static_cast<IncludeNode*>(src.findNodeByName(ASTNodeType::INCLUDE, tgt.includeName, false));
                if (includeNode == nullptr) {
                    throw std::runtime_error(std::format("E0803 include name {} not found at {}", tgt.name, getLocString(tgt.location))); // E0803
                }

                // find from ASTCalc first
                std::vector<int> tmpSizes;
                std::vector<int> tmpAligns;
                for (int i = 0; i < includeNode->args.size(); i++) {
                    tmpSizes.push_back(includeNode->args[i]->typeSize);
                    tmpAligns.push_back(includeNode->args[i]->typeAlign);
                }
                int index = findSource(includeNode->path, tmpSizes, tmpAligns);
                SrcFile* includeSrc = nullptr;
                if (index >= 0) {
                    includeSrc = srcTrees[index].get();
                }

                // find from ASTGen
                if (includeSrc == nullptr) {
                    index = astGen->findSource(includeNode->path);
                    if (index >= 0) {
                        includeSrc = astGen->srcFiles[index].get();
                    }
                }
                if (includeSrc == nullptr) {
                    throw std::runtime_error(std::format("E0804 included module {} not found at {}", includeNode->path, getLocString(tgt.location))); // E0804
                }

                // fetch foreign type size
                DeclStructNode* structNode = static_cast<DeclStructNode*>(includeSrc->findNodeByName(ASTNodeType::DECL_STRUCT, tgt.name, true));
                if (structNode != nullptr && structNode->structSize != -1) {
                    tgt.typeSize = structNode->structSize;
                    tgt.typeAlign = structNode->structAlign;
                    modified = true;
                }
                DeclEnumNode* enumNode = static_cast<DeclEnumNode*>(includeSrc->findNodeByName(ASTNodeType::DECL_ENUM, tgt.name, true));
                if (enumNode != nullptr) {
                    tgt.typeSize = enumNode->enumSize;
                    tgt.typeAlign = enumNode->enumSize;
                    modified = true;
                }
                if (structNode == nullptr && enumNode == nullptr) {
                    throw std::runtime_error(std::format("E0805 type {}.{} not found at {}", tgt.includeName, tgt.name, getLocString(tgt.location))); // E0805
                }
            }
            break;
    }
    return modified;
}

bool ASTCalc::completeStruct(SrcFile& src, DeclStructNode& tgt) {
    bool isModified = false;
    for (auto& mem : tgt.memTypes) {
        isModified = isModified | completeType(src, *mem);
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

// calculate all sizes
std::string ASTCalc::complete(std::unique_ptr<SrcFile> src, std::vector<int>& tmpSizes, std::vector<int>& tmpAligns) {
    // fill template infos
    prt.Log(std::format("start completing source {}", src->path), 2);
    if (tmpSizes.size() != tmpAligns.size() || (tmpSizes.size() != 0 ^ src->isTemplate)) {
        return std::format("E0806 invalid template args while completing {}", src->path); // E0806
    }
    std::vector<DeclTemplateNode*> tmpNodes;
    std::vector<IncludeNode*> incNodes;
    std::vector<DeclStructNode*> structNodes;
    for (auto& node : src->code->body) {
        if (node->objType == ASTNodeType::DECL_TEMPLATE) {
            tmpNodes.push_back(static_cast<DeclTemplateNode*>(node.get()));
        } else if (node->objType == ASTNodeType::INCLUDE) {
            incNodes.push_back(static_cast<IncludeNode*>(node.get()));
        } else if (node->objType == ASTNodeType::DECL_STRUCT) {
            structNodes.push_back(static_cast<DeclStructNode*>(node.get()));
        }
    }
    if (tmpNodes.size() != tmpSizes.size()) {
        return std::format("E0807 tmpArg required {} given {} while completing {}", tmpNodes.size(), tmpSizes.size(), src->path); // E0807
    }
    for (size_t i = 0; i < tmpNodes.size(); i++) {
        tmpNodes[i]->tmpSize = tmpSizes[i];
        tmpNodes[i]->tmpAlign = tmpAligns[i];
    }

    // complete typesize calculation
    bool isModified = true;
    while (isModified) {
        // complete type sizes of includes
        isModified = false;
        for (auto node : incNodes) {
            if (node == nullptr) continue;
            for (auto& arg : node->args) {
                isModified = isModified | completeType(*src, *arg);
            }
        }

        // import includes, templates
        for (int i = 0; i < incNodes.size(); i++) {
            if (incNodes[i] == nullptr) continue;
            bool canImport = true;
            for (auto& arg : incNodes[i]->args) {
                if (arg->typeSize == -1) {
                    canImport = false;
                    break;
                }
            }
            if (canImport) {
                std::vector<int> tmpSizes;
                std::vector<int> tmpAligns;
                for (auto& arg : incNodes[i]->args) {
                    tmpSizes.push_back(arg->typeSize);
                    tmpAligns.push_back(arg->typeAlign);
                }
                if (findSource(incNodes[i]->path, tmpSizes, tmpAligns) == -1) { // include only if not imported
                    std::unique_ptr<SrcFile> incArg = astGen->srcFiles[astGen->findSource(incNodes[i]->path)]->Clone();
                    std::string err = complete(std::move(incArg), tmpSizes, tmpAligns);
                    if (!err.empty()) {
                        return err;
                    }
                }
                incNodes[i] = nullptr;
                isModified = true;
            }
        }

        // complete struct sizes
        for (auto& structNode : structNodes) {
            if (structNode->structSize > 0) continue;
            isModified = isModified | completeStruct(*src, *structNode);
        }
    }
    prt.Log(std::format("pass4 finished for source {}", src->uniqueName), 2);

    // check includes, structs
    for (auto& incNode : incNodes) {
        if (incNode != nullptr) {
            return std::format("E0807 tmpArgs of include {} size undecidable at {}", incNode->name, getLocString(incNode->location)); // E0807
        }
    }
    for (auto& structNode : structNodes) {
        if (structNode->structSize <= 0) {
            return std::format("E0808 struct {} size undecidable at {}", structNode->name, getLocString(structNode->location)); // E0808
        }
    }

    // finish completing
    std::string uname = src->uniqueName;
    int count = 0;
    bool found = true;
    while (found) {
        found = false;
        for (auto& src : srcTrees) {
            if (src->uniqueName == uname) {
                found = true;
                break;
            }
        }
        if (found) {
            src->uniqueName = std::format("{}_{}", uname, count++);
        }
    }
    src->uniqueName = uname;
    srcTrees.push_back(std::move(src));
    srcSizes.push_back(tmpSizes);
    srcAligns.push_back(tmpAligns);
    prt.Log(std::format("finished completing source {} as {}", src->path, src->uniqueName), 3);
    return "";
}