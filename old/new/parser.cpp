#include "parser.h"

// debug string functions
std::string SrcModule::toString(const std::string& path) {
    std::string result;
    result += "Module: " + path + "\n";
    result += "module name table:\n" + table_names.toString() + "\n\n";
    result += "module type table:\n" + table_types.toString() + "\n\n";
    result += "export name table:\n" + export_names.toString() + "\n\n";
    result += "export type table:\n" + export_types.toString();
    return result;
}

std::string Parser::toString() {
    std::string result = "Program." + std::to_string(ArchSize) + ":\n" + srcTable.toString() + "\n\n\n\n";
    for (const auto& mod : modTables) {
        result += mod->toString(srcTable.getSource(mod->source_id)) + "\n\n\n\n";
    }
    return result.substr(0, result.length() - 4);
}

// returns error message or empty if ok
std::string Parser::parseSrc(const std::string& src_path) {
    // step 1: add source to table
    printer.Log("Parsing source: " + src_path, 4);
    int srcID = srcTable.findSource(src_path);
    if (srcID != -1) {
        return "E0201 Source already added: " + src_path; // E0201
    }
    srcID = srcTable.addSource(src_path, false); // finish later /////
    printer.Log("Added source: " + src_path + " with ID " + std::to_string(srcID), 1);

    // step 2: tokenize & make srcModule
    std::vector<Token> tokens;
    try {
        tokens = tokenize(readFile(src_path), getFileName(src_path), srcID);
    } catch (const std::runtime_error& e) {
        return e.what();
    }
    std::unique_ptr<SrcModule> curSrc = std::make_unique<SrcModule>(srcID); // create new src module, push&export later /////
    TokenProvider prov = TokenProvider(std::move(tokens));
    printer.Log("Tokenized source: " + src_path + " (len " + std::to_string(prov.tokens.size()) + ")", 2);

    // step 3: pass 1 (imports, type table)
    printer.Log("Pass 1 start source: " + src_path, 2);
    std::string err = pass1(prov, *curSrc, src_path);
    if (!err.empty()) {
        return err;
    }

    // step 4: pass 2 (name table)
    printer.Log("Pass 2 start source: " + src_path, 2);
    prov.pos = 0;
    err = pass2(prov, *curSrc);
    if (!err.empty()) {
        return err;
    }

    // step 5: export types & names
    printer.Log("Exporting types & names source: " + src_path, 2);
    for (auto& name : curSrc->table_names.names) {
        if (name->type == NameNodeType::GLOBAL || name->type == NameNodeType::FUNCTION) {
            if ('A' <= name->name[0] && name->name[0] <= 'Z') {
                curSrc->export_names.addName(name->clone());
                printer.Log("Exported name: " + name->name, 1);
            }
        } else if (name->type == NameNodeType::MEMBER || name->type == NameNodeType::METHOD || name->type == NameNodeType::ITEM) {
            std::string front = name->name.substr(0, name->name.find("."));
            std::string back = name->name.substr(name->name.find(".") + 1);
            if (('A' <= front[0] && front[0] <= 'Z') && ('A' <= back[0] && back[0] <= 'Z')) {
                curSrc->export_names.addName(name->clone());
                printer.Log("Exported name: " + name->name, 1);
            }
        }
    }
    for (auto& type : curSrc->table_types.types) {
        if ('A' <= type->name[0] && type->name[0] <= 'Z') {
            curSrc->export_types.addType(type->clone());
            printer.Log("Exported type: " + type->name, 1);
        }
    }

    modTables.push_back(std::move(curSrc)); // debug /////
    srcTable.setStatus(srcID, true); // debug /////
    printer.Log("Parsing completed source: " + src_path, 2);
    return ""; // Return empty string on success
}

// get location string from loc node
std::string Parser::findLocation(LocNode loc) {
    if (loc.source_id < 0 || loc.source_id >= srcTable.sources.size()) {
        return "unknown";
    }
    return getFileName(srcTable.getSource(loc.source_id)) + ":" + std::to_string(loc.line);
}

// find module position by source id, -1 if not found
int Parser::findModule(int id) {
    for (int i = 0; i < modTables.size(); i++) {
        if (modTables[i]->source_id == id) {
            return i;
        }
    }
    return -1;
}

// completes type definetion, returns if modified, [can raise exception]
bool comepleteTypes(TypeNode& node, SrcModule& curSrc) {
    bool modified = false; // complete lower nodes first
    if (node.direct) {
        if (comepleteTypes(*node.direct, curSrc)) {
            modified = true;
        }
    }
    for (auto& ind : node.indirects) {
        if (comepleteTypes(*ind, curSrc)) {
            modified = true;
        }
    }

    if (node.size < 0 && node.type == TypeNodeType::ARRAY) { // incomplete array
        if (node.direct->size >= 0) {
            node.size = node.length * node.direct->size;
            node.allign_req = node.direct->allign_req;
            modified = true;
        }

    } else if (node.size < 0 && node.type == TypeNodeType::STRUCT) { // incomplete struct
        // check if all indirects are complete
        int allign_req = 1;
        if (node.indirects.size() == 0) {
            return modified;
        }
        for (auto& ind : node.indirects) {
            if (ind->size < 0) {
                return modified;
            }
            allign_req = std::max(allign_req, ind->allign_req);
        }
        node.allign_req = std::min(allign_req, 8);
        modified = true;

        // allign struct members
        int offset = 0;
        for (auto& ind : node.indirects) {
            if (offset % ind->allign_req != 0) {
                offset += ind->allign_req - (offset % ind->allign_req);
            }
            ind->offset = offset;
            offset += ind->size;
        }
        if (offset % node.allign_req != 0) {
            offset += node.allign_req - (offset % node.allign_req);
        }
        node.size = offset;

    } else if (node.size < 0 && node.type == TypeNodeType::ABSTRACT) { // abstract struct
        int type_pos = curSrc.table_types.findType(node.name);
        if (type_pos >= 0 && curSrc.table_types.types[type_pos]->size >= 0) {
            node.size = curSrc.table_types.types[type_pos]->size;
            node.offset = curSrc.table_types.types[type_pos]->offset;
            node.allign_req = curSrc.table_types.types[type_pos]->allign_req;
            modified = true;
        }

    } else if (node.type == TypeNodeType::PRECOMPILE1) { // struct or enum
        int type_pos = curSrc.table_types.findType(node.name);
        if (type_pos >= 0) {
            if (curSrc.table_types.types[type_pos]->type == TypeNodeType::STRUCT) {
                node.type = TypeNodeType::ABSTRACT;
            } else {
                node.type = curSrc.table_types.types[type_pos]->type;
            }
            node.length = curSrc.table_types.types[type_pos]->length;
            modified = true;
            if (curSrc.table_types.types[type_pos]->size >= 0) {
                std::unique_ptr<TypeNode> copyTgt = curSrc.table_types.types[type_pos]->clone();
                node.size = copyTgt->size;
                node.offset = copyTgt->offset;
                node.allign_req = copyTgt->allign_req;
                node.direct = std::move(copyTgt->direct);
                node.indirects = std::move(copyTgt->indirects);
            }
        } else {
            throw std::runtime_error("E0331 type " + node.name + " is not defined"); // E0331
        }
    }
    return modified;
}

// manage import, create type table
std::string Parser::pass1(TokenProvider& prov, SrcModule& curSrc, const std::string& curPath) {
    while (prov.canPop(1)) {
        const Token& tk = prov.pop();
        if (tk.type == TokenType::OP_HASH && prov.canPop(3)) { // #include "path" name
            const Token& tkn_order = prov.pop();
            if (tkn_order.type == TokenType::IDENTIFIER && tkn_order.text == "include") {
                const Token& tkn_path = prov.pop();
                const Token& tkn_name = prov.pop();
                if (tkn_path.type == TokenType::LIT_STRING && tkn_name.type == TokenType::IDENTIFIER) {
                    std::string import_path;
                    std::string import_name = tkn_name.text;
                    try {
                        import_path = absPath(tkn_path.value.string_value, getWorkingDir(curPath));
                    } catch (const std::runtime_error& e) {
                        return e.what();
                    }
                    int source_id = srcTable.findSource(import_path);
                    if (source_id == -1) { // not found, add source & link
                        std::string err = parseSrc(import_path);
                        if (!err.empty()) {
                            return err;
                        }
                        std::unique_ptr<NameNode> newImport = std::make_unique<NameNode>();
                        newImport->type = NameNodeType::MODULE;
                        newImport->name = import_name;
                        newImport->tag_value = srcTable.findSource(import_path);
                        if (!curSrc.table_names.addName(std::move(newImport))) {
                            return "E0301 name " + import_name + "is double defined at " + findLocation(tkn_name.location); // E0301
                        }
                        printer.Log("Imported source: " + import_path + " as " + import_name, 3);

                    } else if (srcTable.getStatus(source_id)) { // already finished, link
                        std::unique_ptr<NameNode> newImport = std::make_unique<NameNode>();
                        newImport->type = NameNodeType::MODULE;
                        newImport->name = import_name;
                        newImport->tag_value = source_id;
                        if (!curSrc.table_names.addName(std::move(newImport))) {
                            return "E0302 name " + import_name + "is double defined at " + findLocation(tkn_name.location); // E0302
                        }
                        printer.Log("Imported source: " + import_path + " as " + import_name, 3);

                    } else { // import cycle, error
                        return "E0303 Import cycle detected with source: " + import_path + " at " + findLocation(tk.location); // E0303
                    }
                }
            }

        } else if (tk.type == TokenType::KEY_STRUCT && prov.canPop(3)) { // struct name {...}
            const Token& tkn_id = prov.pop();
            if (tkn_id.type != TokenType::IDENTIFIER) {
                return "E0304 Expected identifier after struct at " + findLocation(tkn_id.location); // E0304
            }
            if (prov.pop().type != TokenType::OP_LBRACE) {
                return "E0305 Expected { after struct name at " + findLocation(tkn_id.location); // E0305
            }
            try {
                std::unique_ptr<TypeNode> newType = parseStructDef(tkn_id.text, prov, curSrc);
                if (!curSrc.table_types.addType(std::move(newType))) {
                    return "E0306 Type " + tkn_id.text + " is double defined at " + findLocation(tkn_id.location); // E0306
                }
                printer.Log("Defined struct type: " + tkn_id.text + " in source ID " + std::to_string(curSrc.source_id), 1);
            } catch (const std::runtime_error& e) {
                return e.what();
            }

        } else if (tk.type == TokenType::KEY_ENUM && prov.canPop(3)) { // enum name {...}
            const Token& tkn_id = prov.pop();
            if (tkn_id.type != TokenType::IDENTIFIER) {
                return "E0307 Expected identifier after enum at " + findLocation(tkn_id.location); // E0307
            }
            if (prov.pop().type != TokenType::OP_LBRACE) {
                return "E0308 Expected { after enum name at " + findLocation(tkn_id.location); // E0308
            }
            try {
                std::unique_ptr<TypeNode> newType = parseEnumDef(tkn_id.text, prov, curSrc);
                if (!curSrc.table_types.addType(std::move(newType))) {
                    return "E0309 Type " + tkn_id.text + " is double defined at " + findLocation(tkn_id.location); // E0309
                }
                printer.Log("Defined enum type: " + tkn_id.text + " in source ID " + std::to_string(curSrc.source_id), 1);
            } catch (const std::runtime_error& e) {
                return e.what();
            }
        }
    }

    // type size check
    bool isModified = true;
    try {
        while (isModified) {
            isModified = false;
            for (auto& node : curSrc.table_types.types) {
                if (comepleteTypes(*node, curSrc)) {
                    isModified = true;
                }
            }
        }
    } catch (const std::runtime_error& e) {
        return e.what();
    }
    for (auto& node : curSrc.table_types.types) {
        if (node->size < 0) {
            return "E0310 Size of type " + node->name + " is not defined"; // E0310
        }
    }

    // struct member & enum item name-type match (name format struct.member or enum.item)
    for (auto& name_node : curSrc.table_names.names) {
        if (name_node->type == NameNodeType::MEMBER) {
            std::string struct_name = name_node->name.substr(0, name_node->name.find("."));
            int type_pos = curSrc.table_types.findType(struct_name);
            if (type_pos < 0) {
                return "E0332 Struct not exists while matching " + name_node->name; // E0332
            }
            if (curSrc.table_types.types[type_pos]->type != TypeNodeType::STRUCT) {
                return "E0333 Not struct type while matching " + name_node->name; // E0333
            }
            name_node->type_node = curSrc.table_types.types[type_pos]->indirects[name_node->tag_value]->clone();
        } else if (name_node->type == NameNodeType::ITEM) {
            std::string enum_name = name_node->name.substr(0, name_node->name.find("."));
            int type_pos = curSrc.table_types.findType(enum_name);
            if (type_pos < 0) {
                return "E0334 Enum not exists while matching " + name_node->name; // E0334
            }
            if (curSrc.table_types.types[type_pos]->type != TypeNodeType::ENUM) {
                return "E0335 Not enum type while matching " + name_node->name; // E0335
            }
            name_node->type_node = curSrc.table_types.types[type_pos]->clone();
        }
    }
    return "";
}

// parse any type, [can raise exception]
std::unique_ptr<TypeNode> Parser::parseGenericType(TokenProvider& prov, SrcModule& curSrc) {
    if (!prov.canPop(1)) {
        throw std::runtime_error("E0311 TokenProvider out of range"); // E0311
    }
    std::unique_ptr<TypeNode> newType;

    Token tkn = prov.pop(); // first token should be primitive or module name
    switch (tkn.type) {
        case TokenType::KEY_I8: case TokenType::KEY_I16:  case TokenType::KEY_I32: case TokenType::KEY_I64:
        case TokenType::KEY_U8: case TokenType::KEY_U16:  case TokenType::KEY_U32: case TokenType::KEY_U64:
        case TokenType::KEY_F32: case TokenType::KEY_F64:  case TokenType::KEY_VOID:
        {
            int s = (tkn.type == TokenType::KEY_I8 || tkn.type == TokenType::KEY_U8) ? 1 :
                            (tkn.type == TokenType::KEY_I16 || tkn.type == TokenType::KEY_U16) ? 2 :
                            (tkn.type == TokenType::KEY_I32 || tkn.type == TokenType::KEY_U32 || tkn.type == TokenType::KEY_F32) ? 4 :
                            (tkn.type == TokenType::KEY_I64 || tkn.type == TokenType::KEY_U64 || tkn.type == TokenType::KEY_F64) ? 8 : 0;
            newType = std::make_unique<TypeNode>(TypeNodeType::PRIMITIVE, tkn.text, s);
            if (s == 0) {
                newType->allign_req = 1;
            }
            break;
        }

        case TokenType::IDENTIFIER: // could be module.name or just name (struct, enum)
            if (prov.canPop(2) && prov.seek().type == TokenType::OP_DOT) { // module.
                prov.pop();
                Token tkn_name = prov.pop();
                if (tkn_name.type != TokenType::IDENTIFIER) {
                    throw std::runtime_error("E0312 expected identifier after . at " + findLocation(tkn.location)); // E0312
                }
                int name_pos = curSrc.table_names.findName(tkn.text);
                if (name_pos == -1 || curSrc.table_names.names[name_pos]->type != NameNodeType::MODULE) {
                    throw std::runtime_error("E0313 Unknown module name: " + tkn.text + " at " + findLocation(tkn.location)); // E0313
                }
                int source_id = curSrc.table_names.names[name_pos]->tag_value;
                int mod_pos = findModule(source_id);
                if (mod_pos == -1) {
                    throw std::runtime_error("E0314 Module not loaded: " + tkn.text + " at " + findLocation(tkn.location)); // E0314
                }
                int type_pos = modTables[mod_pos]->export_types.findType(tkn_name.text);
                if (type_pos == -1) {
                    throw std::runtime_error("E0315 Unknown type name: " + tkn_name.text + " in module " + tkn.text + " at " + findLocation(tkn_name.location)); // E0315
                }
                newType = modTables[mod_pos]->export_types.types[type_pos]->clone();
                newType->name = tkn.text + "." + newType->name;
            } else { // just name
                int type_pos = curSrc.table_types.findType(tkn.text);
                if (type_pos < 0) { // name that not declared yet
                    newType = std::make_unique<TypeNode>(TypeNodeType::PRECOMPILE1, tkn.text, -1);
                } else {
                    auto& copyTgt = curSrc.table_types.types[type_pos];
                    if (copyTgt->type == TypeNodeType::STRUCT) { // struct
                        newType = std::make_unique<TypeNode>(TypeNodeType::ABSTRACT, copyTgt->name, copyTgt->size);
                        newType->allign_req = copyTgt->allign_req;
                        newType->length = copyTgt->length;
                        newType->offset = copyTgt->offset;
                    } else { // enum
                        newType = copyTgt->clone();
                    }
                }
            }
            if (newType->type != TypeNodeType::ABSTRACT // struct in current module
                    && newType->type != TypeNodeType::STRUCT // struct in another module
                    && newType->type != TypeNodeType::ENUM // enum
                    && newType->type != TypeNodeType::PRECOMPILE1) { // name that not declared yet
                throw std::runtime_error("E0316 Expected struct or enum with name " + tkn.text + " at " + findLocation(tkn.location)); // E0316
            }
            break;

        default:
            throw std::runtime_error("E0317 Invalid type start token: " + tkn.text + " at " + findLocation(tkn.location)); // E0317
    }

    // parse modifiers
    while (prov.canPop(1)) {
        Token mod = prov.pop();
        if (mod.type == TokenType::OP_MUL) { // pointer *
            std::unique_ptr<TypeNode> ptrType = std::make_unique<TypeNode>(TypeNodeType::POINTER, "pointer", ArchSize);
            ptrType->direct = std::move(newType);
            newType = std::move(ptrType);

        } else if (mod.type == TokenType::OP_LBRACKET) { // array [
            if (prov.canPop(1) && prov.seek().type == TokenType::OP_RBRACKET) { // []
                prov.pop();
                std::unique_ptr<TypeNode> arrType = std::make_unique<TypeNode>(TypeNodeType::POINTER, "pointer", ArchSize);
                arrType->direct = std::move(newType);
                newType = std::move(arrType);
            } else if (prov.canPop(2) && (prov.seek().type == TokenType::LIT_INT10 || prov.seek().type == TokenType::LIT_INT16)) { // [num
                Token arrLen = prov.pop();
                if (prov.pop().type != TokenType::OP_RBRACKET) { // [num]
                    throw std::runtime_error("E0318 expected ] after array len at " + findLocation(arrLen.location)); // E0318
                }
                if (arrLen.value.int_value <= 0) {
                    throw std::runtime_error("E0319 Invalid array len: " + std::to_string(arrLen.value.int_value) + " at " + findLocation(arrLen.location)); // E0319
                }
                int s = newType->size >= 0 ? newType->size * arrLen.value.int_value : -1;
                std::unique_ptr<TypeNode> arrType = std::make_unique<TypeNode>(TypeNodeType::ARRAY, "array", s);
                arrType->length = arrLen.value.int_value;
                arrType->allign_req = newType->allign_req;
                arrType->direct = std::move(newType);
                newType = std::move(arrType);
            } else {
                throw std::runtime_error("E0320 Invalid array type syntax at " + findLocation(mod.location)); // E0320
            }

        } else if (mod.type == TokenType::OP_LPAREN) { // function (
            std::unique_ptr<TypeNode> funcType = std::make_unique<TypeNode>(TypeNodeType::FUNCTION, "function", ArchSize);
            funcType->direct = std::move(newType);
            newType = std::move(funcType);
            for (;;) {
                if (!prov.canPop(1)) {
                    throw std::runtime_error("E0321 Function type not complete at " + findLocation(mod.location)); // E0321
                }
                if (prov.seek().type == TokenType::OP_RPAREN) { // )
                    prov.pop();
                    break;
                } else if (prov.seek().type == TokenType::OP_COMMA) { // ,
                    prov.pop();
                } else { // type
                    newType->indirects.push_back(parseGenericType(prov, curSrc));
                }
            }

        } else {
            prov.rewind();
            break;
        }
    }
    return newType;
}

// parse struct definition, [can raise exception]
std::unique_ptr<TypeNode> Parser::parseStructDef(const std::string name, TokenProvider& prov, SrcModule& curSrc) {
    std::unique_ptr<TypeNode> newType = std::make_unique<TypeNode>(TypeNodeType::STRUCT, name, -1);
    int pos = 0;
    for (;;) {
        if (!prov.canPop(1)) {
            throw std::runtime_error("E0322 Struct " + name + " not completed"); // E0322
        }
        if (prov.seek().type == TokenType::OP_RBRACE) { // }
            prov.pop();
            break;
        }

        // type name;
        newType->indirects.push_back(parseGenericType(prov, curSrc));
        if (!prov.canPop(2)) {
            throw std::runtime_error("E0323 Struct " + name + " not completed"); // E0323
        }
        Token tkn_name = prov.pop();
        Token tkn_end = prov.pop();
        if (tkn_name.type != TokenType::IDENTIFIER || 
                (tkn_end.type != TokenType::OP_SEMICOLON && tkn_end.type != TokenType::OP_COMMA && tkn_end.type != TokenType::OP_RBRACE)) {
            throw std::runtime_error("E0324 Invalid struct member at " + findLocation(tkn_name.location)); // E0324
        }
        if (!curSrc.table_names.addName(std::make_unique<NameNode>(NameNodeType::MEMBER, name + "." + tkn_name.text, pos++))) { // add struct_type.member with member index
            throw std::runtime_error("E0325 name " + name + "." + tkn_name.text + "is double defined at " + findLocation(tkn_name.location)); // E0325
        }
        if (tkn_end.type == TokenType::OP_RBRACE) {
            break;
        }
    }
    return newType;
}

// parse enum definition, [can raise exception]
std::unique_ptr<TypeNode> Parser::parseEnumDef(const std::string enum_name, TokenProvider& prov, SrcModule& curSrc) {
    std::unique_ptr<TypeNode> newType = std::make_unique<TypeNode>(TypeNodeType::ENUM, enum_name, -1);
    std::vector<int64_t> values;
    int64_t previous = -1;
    int64_t min = 0;
    int64_t max = 0;
    for (;;) {
        if (!prov.canPop(1)) {
            throw std::runtime_error("E0326 Enum " + enum_name + " not completed"); // E0326
        }
        if (prov.seek().type == TokenType::OP_RBRACE) { // }
            prov.pop();
            break;
        }

        // name; or name = (+/-) literal;
        std::vector<Token> buffer;
        for (;;) {
            if (!prov.canPop(1)) {
                throw std::runtime_error("E0327 Enum " + enum_name + " not completed"); // E0327
            }
            Token tkn = prov.pop();
            buffer.push_back(tkn);
            if (tkn.type == TokenType::OP_SEMICOLON || tkn.type == TokenType::OP_COMMA) {
                break;
            } else if (tkn.type == TokenType::OP_RBRACE) {
                prov.rewind();
                break;
            }
        }
        std::string name;
        if (buffer.size() == 2 && buffer[0].type == TokenType::IDENTIFIER) { // name;
            name = buffer[0].text;
        } else if (buffer.size() == 4 && buffer[0].type == TokenType::IDENTIFIER && buffer[1].type == TokenType::OP_ASSIGN
                && (buffer[2].type == TokenType::LIT_INT10 || buffer[2].type == TokenType::LIT_INT16 || buffer[2].type == TokenType::LIT_CHAR)) { // name = literal;
            name = buffer[0].text;
            previous = buffer[2].value.int_value - 1;
        } else if (buffer.size() == 5 && buffer[0].type == TokenType::IDENTIFIER && buffer[1].type == TokenType::OP_ASSIGN
                && (buffer[2].type == TokenType::OP_PLUS || buffer[2].type == TokenType::OP_MINUS)
                && (buffer[3].type == TokenType::LIT_INT10 || buffer[3].type == TokenType::LIT_INT16 || buffer[3].type == TokenType::LIT_CHAR)) { // name = +/- literal;
            name = buffer[0].text;
            previous = (buffer[2].type == TokenType::OP_MINUS) ? (-buffer[3].value.int_value) - 1 : buffer[3].value.int_value - 1;
        } else {
            throw std::runtime_error("E0328 Invalid enum item at " + findLocation(buffer[0].location)); // E0328
        }

        // add enum_type.item with int value
        if (!curSrc.table_names.addName(std::make_unique<NameNode>(NameNodeType::ITEM, enum_name + "." + name, ++previous))) {
            throw std::runtime_error("E0329 name " + enum_name + "." + name + "is double defined at " + findLocation(buffer[0].location)); // E0329
        }
        for (auto i : values) {
            if (i == previous) {
                throw std::runtime_error("E0330 conflicting value " + std::to_string(previous) + " with name " + enum_name + "." + name + " at " + findLocation(buffer[0].location)); // E0330
            }
        }
        if (previous < min) min = previous;
        if (previous > max) max = previous;
        values.push_back(previous);
    }

    // find smallest type to fit enum
    if (-128 <= min && max <= 127) {
        newType->size = 1;
    } else if (-32768 <= min && max <= 32767) {
        newType->size = 2;
    } else if (-2147483648 <= min && max <= 2147483647) {
        newType->size = 4;
    } else {
        newType->size = 8;
    }
    newType->allign_req = newType->size;
    return newType;
}

// find scope end
int findScopeEnd(const std::vector<Token>& tokens, int start) {
    int depth = 0;
    TokenType Ltype;
    TokenType Rtype;
    if (tokens[start].type == TokenType::OP_LBRACE) {
        Ltype = TokenType::OP_LBRACE;
        Rtype = TokenType::OP_RBRACE;
    } else if (tokens[start].type == TokenType::OP_LPAREN) {
        Ltype = TokenType::OP_LPAREN;
        Rtype = TokenType::OP_RPAREN;
    } else if (tokens[start].type == TokenType::OP_LBRACKET) {
        Ltype = TokenType::OP_LBRACKET;
        Rtype = TokenType::OP_RBRACKET;
    } else if (tokens[start].type == TokenType::OP_LITTER) {
        Ltype = TokenType::OP_LITTER;
        Rtype = TokenType::OP_GREATER;
    } else {
        return -1; // not a scope start
    }
    for (int i = start; i < tokens.size(); i++) {
        if (tokens[i].type == Ltype) {
            depth++;
        } else if (tokens[i].type == Rtype) {
            depth--;
            if (depth == 0) {
                return i;
            }
        }
    }
    return -1; // not found
}

// hoists global variables & functions
std::string Parser::pass2(TokenProvider& prov, SrcModule& curSrc) {
    while (prov.canPop(1)) {
        Token tkn = prov.seek();
        switch (tkn.type) {
            case TokenType::KEY_I8: case TokenType::KEY_I16:  case TokenType::KEY_I32: case TokenType::KEY_I64:
            case TokenType::KEY_U8: case TokenType::KEY_U16:  case TokenType::KEY_U32: case TokenType::KEY_U64:
            case TokenType::KEY_F32: case TokenType::KEY_F64:  case TokenType::KEY_VOID: // type start
            {
                std::unique_ptr<TypeNode> front_type = parseGenericType(prov, curSrc);
                std::string err = "";
                if (prov.match({TokenType::IDENTIFIER, TokenType::OP_SEMICOLON})) { // T id;
                    err = parseGlobalDef(prov, curSrc, std::move(front_type), 0);
                } else if (prov.match({TokenType::IDENTIFIER, TokenType::OP_ASSIGN})) { // T id =
                    err = parseGlobalDef(prov, curSrc, std::move(front_type), 0);
                } else if (prov.match({TokenType::IDENTIFIER, TokenType::OP_LPAREN})) { // T id(
                    err = parseFunctionDef(prov, curSrc, std::move(front_type), 0);
                } else if (prov.match({TokenType::IDENTIFIER, TokenType::OP_DOT, TokenType::IDENTIFIER, TokenType::OP_LPAREN})) { // T struct_name.id(
                    err = parseFunctionDef(prov, curSrc, std::move(front_type), 0);
                }
                if (err != "") {
                    return err;
                }
            }
            break;

            case TokenType::IDENTIFIER: // module name or struct/enum
            {
                int name_pos = curSrc.table_names.findName(tkn.text);
                if (curSrc.table_names.names[name_pos]->type == NameNodeType::MODULE
                        || curSrc.table_names.names[name_pos]->type == NameNodeType::STRUCT
                        || curSrc.table_names.names[name_pos]->type == NameNodeType::ENUM) { // type start
                    std::unique_ptr<TypeNode> front_type = parseGenericType(prov, curSrc);
                    std::string err = "";
                    if (prov.match({TokenType::IDENTIFIER, TokenType::OP_SEMICOLON})) { // T id;
                        err = parseGlobalDef(prov, curSrc, std::move(front_type), 0);
                    } else if (prov.match({TokenType::IDENTIFIER, TokenType::OP_ASSIGN})) { // T id =
                        err = parseGlobalDef(prov, curSrc, std::move(front_type), 0);
                    } else if (prov.match({TokenType::IDENTIFIER, TokenType::OP_LPAREN})) { // T id(
                        err = parseFunctionDef(prov, curSrc, std::move(front_type), 0);
                    } else if (prov.match({TokenType::IDENTIFIER, TokenType::OP_DOT, TokenType::IDENTIFIER, TokenType::OP_LPAREN})) { // T struct_name.id(
                        err = parseFunctionDef(prov, curSrc, std::move(front_type), 0);
                    }
                    if (err != "") {
                        return err;
                    }
                } else {
                    prov.pop();
                }
            }
            break;

            case TokenType::OP_HASH: // va_arg, const, volatile
            {
                prov.pop();
                Token tkn_order = prov.pop();
                std::string err = "";
                if (tkn_order.type == TokenType::IDENTIFIER && tkn_order.text == "va_arg") { // #va_arg
                    std::unique_ptr<TypeNode> front_type = parseGenericType(prov, curSrc);
                    err = parseFunctionDef(prov, curSrc, std::move(front_type), 1);
                } else if (tkn_order.type == TokenType::IDENTIFIER && tkn_order.text == "const") { // #const
                    std::unique_ptr<TypeNode> front_type = parseGenericType(prov, curSrc);
                    err = parseGlobalDef(prov, curSrc, std::move(front_type), 2);
                } else if (tkn_order.type == TokenType::IDENTIFIER && tkn_order.text == "volatile") { // #volatile
                    std::unique_ptr<TypeNode> front_type = parseGenericType(prov, curSrc);
                    err = parseGlobalDef(prov, curSrc, std::move(front_type), 3);
                } else if (tkn_order.type == TokenType::IDENTIFIER && tkn_order.text == "include") { // #include _string_ module_name
                    prov.pos += 2;
                }
            }
            break;

            case TokenType::KEY_STRUCT: case TokenType::KEY_ENUM: // jump struct or enum
            {
                while (prov.pos < prov.tokens.size() && prov.tokens[prov.pos].type != TokenType::OP_LBRACE) {
                    prov.pos++;
                }
                if (prov.pos >= prov.tokens.size()) {
                    return "E0401 Scope not completed at " + findLocation(tkn.location); // E0401
                }
                int end = findScopeEnd(prov.tokens, prov.pos);
                if (end == -1) {
                    return "E0402 Scope not completed at " + findLocation(tkn.location); // E0402
                }
                prov.pos = end + 1;
            }
            break;

            default:
                prov.pos++;
        }
    }
    return "";
}

// parse global variable definition
std::string Parser::parseGlobalDef(TokenProvider& prov, SrcModule& curSrc, std::unique_ptr<TypeNode> front_type, int tag) {
    Token tkn_name = prov.pop();
    if (tkn_name.type != TokenType::IDENTIFIER) {
        return "E0403 Invalid global variable at " + findLocation(tkn_name.location); // E0403
    }
    if (!curSrc.table_names.addName(std::make_unique<NameNode>(NameNodeType::GLOBAL, tkn_name.text, tag, std::move(front_type)))) {
        return "E0404 name " + tkn_name.text + "is double defined at " + findLocation(tkn_name.location); // E0404
    }
    while (prov.pos < prov.tokens.size() && prov.tokens[prov.pos].type != TokenType::OP_SEMICOLON) {
        prov.pos++;
    }
    if (prov.pos >= prov.tokens.size()) {
        return "E0401 Expression not completed at " + findLocation(tkn_name.location); // E0401
    }
    printer.Log("Parsed global variable: " + tkn_name.text, 1);
    prov.pos++;
    return "";
}

// parse function definition
std::string Parser::parseFunctionDef(TokenProvider& prov, SrcModule& curSrc, std::unique_ptr<TypeNode> front_type, int tag) {
    // get function name
    Token tkn_temp = prov.pop();
    std::string struct_name = "";
    std::string func_name = "";
    bool isMethod = false;
    if (tkn_temp.type != TokenType::IDENTIFIER) {
        return "E0405 Expected identifier at " + findLocation(tkn_temp.location); // E0405
    }
    int type_pos = curSrc.table_types.findType(tkn_temp.text);
    if (type_pos >= 0) { // method
        if (curSrc.table_types.types[type_pos]->type != TypeNodeType::STRUCT) {
            return "E0406 Expected struct name at " + findLocation(tkn_temp.location); // E0406
        }
        if (prov.pop().type != TokenType::OP_DOT) {
            return "E0407 Expected . at " + findLocation(tkn_temp.location); // E0407
        }
        Token tkn_name = prov.pop();
        if (tkn_name.type != TokenType::IDENTIFIER) {
            return "E0408 Expected identifier at " + findLocation(tkn_name.location); // E0408
        }
        struct_name = tkn_temp.text;
        func_name = struct_name + "." + tkn_name.text;
        isMethod = true;
    } else { // function
        func_name = tkn_temp.text;
    }

    // get function parameters
    std::string func_method = isMethod ? "method" : "function";
    std::unique_ptr<TypeNode> func_type = std::make_unique<TypeNode>(TypeNodeType::FUNCTION, func_method, ArchSize);
    func_type->direct = std::move(front_type);
    if (prov.pop().type != TokenType::OP_LPAREN) {
        return "E0409 Expected ( at " + findLocation(tkn_temp.location); // E0409
    }
    while (prov.canPop(1)) {
        try {
            tkn_temp = prov.seek();
            if (tkn_temp.type == TokenType::OP_RPAREN) {
                prov.pop();
                break;
            }
            std::unique_ptr<TypeNode> arg_type = parseGenericType(prov, curSrc);
            tkn_temp = prov.pop();
            if (tkn_temp.type != TokenType::IDENTIFIER) {
                return "E0410 Expected identifier at " + findLocation(tkn_temp.location); // E0410
            }
            func_type->indirects.push_back(std::move(arg_type));
            if (!prov.canPop(1)) {
                return "E0411 Function parameter not completed at " + findLocation(tkn_temp.location); // E0411
            }
            if (prov.seek().type == TokenType::OP_COMMA) {
                prov.pop();
            } else if (prov.seek().type != TokenType::OP_RPAREN) {
                return "E0412 Expected , at " + findLocation(tkn_temp.location); // E0412
            }
        } catch (const std::runtime_error& e) {
            return e.what();
        }
    }

    // jump scope
    if (!prov.canPop(2) || prov.seek().type != TokenType::OP_LBRACE) {
        return "E0413 Scope not completed at " + findLocation(tkn_temp.location); // E0413
    }
    int end = findScopeEnd(prov.tokens, prov.pos);
    if (end == -1) {
        return "E0414 Scope not completed at " + findLocation(tkn_temp.location); // E0414
    }
    prov.pos = end + 1;

    // check argument types, add name
    if (tag == 1) {
        if (func_type->indirects.size() < 2) {
            return "E0415 Parameters of va_arg function should end with (void**, int) at " + findLocation(tkn_temp.location); // E0415
        }
        std::string arg1 = func_type->indirects[func_type->indirects.size() - 2]->toString();
        std::string arg2 = func_type->indirects[func_type->indirects.size() - 1]->toString();
        if (arg1 != "void**" || (arg2 != "i8" && arg2 != "i16" && arg2 != "i32" && arg2 != "i64" && arg2 != "u8" && arg2 != "u16" && arg2 != "u32" && arg2 != "u64")) {
            return "E0416 Parameters of va_arg function should end with (void**, int) at " + findLocation(tkn_temp.location); // E0416
        }
    }
    if (isMethod) {
        if (func_type->indirects.size() < 1) {
            return "E0417 Parameters of method should start with struct* at " + findLocation(tkn_temp.location); // E0417
        }
        std::string arg1 = func_type->indirects[0]->toString();
        if (arg1 != struct_name + "*") {
            return "E0418 Parameters of method should start with struct* at " + findLocation(tkn_temp.location); // E0418
        }
    }
    NameNodeType func_method_tp = isMethod ? NameNodeType::METHOD : NameNodeType::FUNCTION;
    if (!curSrc.table_names.addName(std::make_unique<NameNode>(func_method_tp, func_name, tag, std::move(func_type)))) {
        return "E0419 name " + func_name + "is double defined at " + findLocation(tkn_temp.location); // E0419
    }
    printer.Log("Parsed function: " + func_name, 1);
    return "";
}