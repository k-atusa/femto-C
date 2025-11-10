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

LongStatNode* ScopeNode::findDefineByName(const std::string& name) {
    for (auto& node : body) {
        if (node->type == ASTNodeType::DEFINE) {
            ShortStatNode* defNode = static_cast<ShortStatNode*>(node.get());
            LongStatNode* varNode = static_cast<LongStatNode*>(defNode->statExpr.get());
            if (varNode->varName && varNode->varName->text == name) {
                return varNode;
            }
        }
    }
    if (parent != nullptr && parent->type == ASTNodeType::SCOPE) {
        ScopeNode* parentScope = static_cast<ScopeNode*>(parent);
        return parentScope->findDefineByName(name);
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

std::unique_ptr<TypeNode> SrcFile::parseType(TokenProvider& tp) {
    // placeholder implementation
    // actual implementation would parse tokens to create a TypeNode
    return std::make_unique<TypeNode>();
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