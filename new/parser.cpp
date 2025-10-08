#include "basicFunc.h"
#include "tokenize.h"
#include "parser.h"

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
    printer.Log("Tokenized source: " + src_path + " (len " + std::to_string(tokens.size()) + ")", 2);

    // step 3: pass 1 (imports, type table)
    printer.Log("Pass 1 start source: " + src_path, 2);
    std::string err = pass1(tokens, *curSrc, src_path);
    if (!err.empty()) {
        return err;
    }

    return ""; // Return empty string on success
}

// get location string from loc node
std::string Parser::findLocation(LocNode loc) {
    if (loc.source_id < 0 || loc.source_id >= srcTable.sources.size()) {
        return "unknown";
    }
    return getFileName(srcTable.getSource(loc.source_id)) + ":" + std::to_string(loc.line);
}

// find module index in modTables, -1 if not found
int Parser::findModule(int id) {
    for (int i = 0; i < modTables.size(); i++) {
        if (modTables[i]->source_id == id) {
            return i;
        }
    }
    return -1;
}

// manage import, create type table
std::string Parser::pass1(const std::vector<Token>& tokens, SrcModule& curSrc, const std::string& curPath) {
    int pos = 0;
    while (pos < tokens.size()) {
        const Token& tk = tokens[pos++];
        if (tk.type == TokenType::OP_HASH && pos + 2 < tokens.size()) { // #include "path" name
            const Token& tkn_order = tokens[pos++];
            if (tkn_order.type == TokenType::IDENTIFIER && tkn_order.text == "include") {
                const Token& tkn_path = tokens[pos++];
                const Token& tkn_name = tokens[pos++];
                if (tkn_path.type == TokenType::LIT_STRING && tkn_name.type == TokenType::IDENTIFIER) {
                    std::string import_path;
                    std::string import_name = tkn_name.text;
                    try {
                        import_path = absPath(tkn_path.value.string_value, curPath);
                    } catch (const std::runtime_error& e) {
                        return e.what();
                    }
                    int import_id = srcTable.findSource(import_path);
                    if (import_id == -1) { // not found, add source & link
                        std::string err = parseSrc(import_path);
                        if (!err.empty()) {
                            return err;
                        }
                        std::unique_ptr<NameNode> newImport = std::make_unique<NameNode>();
                        newImport->type = NameNodeType::MODULE;
                        newImport->name = import_path;
                        newImport->alias = import_name;
                        newImport->name_id = srcTable.findSource(import_path);
                        if (!curSrc.table_names.addName(std::move(newImport))) {
                            return "E0301 name " + import_name + "is double defined at " + findLocation(tk.location); // E0301
                        }
                        curSrc.table_names.names.push_back(std::move(newImport));
                        printer.Log("Imported source: " + import_path + " as " + import_name + " with ID " + std::to_string(newImport->name_id), 3);

                    } else if (srcTable.getStatus(import_id)) { // already finished, link
                        std::unique_ptr<NameNode> newImport = std::make_unique<NameNode>();
                        newImport->type = NameNodeType::MODULE;
                        newImport->name = import_path;
                        newImport->alias = import_name;
                        newImport->name_id = import_id;
                        if (!curSrc.table_names.addName(std::move(newImport))) {
                            return "E0302 name " + import_name + "is double defined at " + findLocation(tk.location); // E0302
                        }
                        curSrc.table_names.names.push_back(std::move(newImport));
                        printer.Log("Imported source: " + import_path + " as " + import_name + " with ID " + std::to_string(newImport->name_id), 3);

                    } else { // import cycle, error
                        return "E0303 Import cycle detected with source: " + import_path + " at " + findLocation(tk.location); // E0303
                    }
                }
            }

        } else if (tk.type == TokenType::KEY_STRUCT && pos + 2 < tokens.size()) { // struct {...}
            const Token& tkn_id = tokens[pos++];
            if (tkn_id.type != TokenType::IDENTIFIER) {
                return "E0304 Expected identifier after 'struct' at " + findLocation(tk.location); // E0304
            }
            int tkn_start = pos;
            int tkn_end = findScopeEnd(tokens, pos);
            if (tkn_end == -1) {
                return "E0305 Expected '{...}' after struct " + tkn_id.text + " at " + findLocation(tk.location); // E0305
            }
            try {
                std::unique_ptr<TypeNode> newType = parseStructDef(tkn_id.text, tokens, tkn_start + 1, tkn_end - 1);
                if (!curSrc.table_types.addType(std::move(newType))) {
                    return "E0306 Type " + tkn_id.text + " is double defined at " + findLocation(tk.location); // E0306
                }
                printer.Log("Defined struct type: " + tkn_id.text + " in source ID " + std::to_string(curSrc.source_id), 1);
            } catch (const std::runtime_error& e) {
                return e.what();
            }

        } else if (tk.type == TokenType::KEY_ENUM && pos + 2 < tokens.size()) { // enum {...}
            const Token& tkn_id = tokens[pos++];
            if (tkn_id.type != TokenType::IDENTIFIER) {
                return "E0307 Expected identifier after 'enum' at " + findLocation(tk.location); // E0307
            }
            int tkn_start = pos;
            int tkn_end = findScopeEnd(tokens, pos);
            if (tkn_end == -1) {
                return "E0308 Expected '{...}' after enum " + tkn_id.text + " at " + findLocation(tk.location); // E0308
            }
            try {
                std::unique_ptr<TypeNode> newType = parseEnumDef(tkn_id.text, tokens, tkn_start + 1, tkn_end - 1);
                if (!curSrc.table_types.addType(std::move(newType))) {
                    return "E0309 Type " + tkn_id.text + " is double defined at " + findLocation(tk.location); // E0309
                }
                printer.Log("Defined enum type: " + tkn_id.text + " in source ID " + std::to_string(curSrc.source_id), 1);
            } catch (const std::runtime_error& e) {
                return e.what();
            }
        }
    }

    // type size check /////

    // export curSrc type table
    for (auto& t : curSrc.table_types.types) {
        if (t->name[0] <= 'Z') {
            curSrc.export_types.addType(t->clone());
            printer.Log("Added public type: " + t->toString() + " from source ID " + std::to_string(curSrc.source_id), 1);
        }
    }
    return "";
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

// parse any type, [can raise exception]
std::unique_ptr<TypeNode> Parser::parseGenericType(const std::vector<Token>& tokens, int start, int end, SrcModule& curSrc) {
    if (start > end || start < 0 || end >= tokens.size()) {
        throw std::runtime_error("E0310 Invalid range for type parsing [" + std::to_string(start) + ":" + std::to_string(start) + "]"); // E0310
    }
    std::unique_ptr<TypeNode> newType;

    Token tkn = tokens[start++]; // first token should be primitive or module name
    switch (tkn.type) {
        case TokenType::KEY_I8: case TokenType::KEY_I16:  case TokenType::KEY_I32: case TokenType::KEY_I64:
        case TokenType::KEY_U8: case TokenType::KEY_U16:  case TokenType::KEY_U32: case TokenType::KEY_U64:
        case TokenType::KEY_F32: case TokenType::KEY_F64:  case TokenType::KEY_VOID:
            newType = TypeNode(TypeNodeType::PRIMITIVE, tkn.text, -1).clone();
            newType->size = (tkn.type == TokenType::KEY_I8 || tkn.type == TokenType::KEY_U8) ? 1 :
                            (tkn.type == TokenType::KEY_I16 || tkn.type == TokenType::KEY_U16) ? 2 :
                            (tkn.type == TokenType::KEY_I32 || tkn.type == TokenType::KEY_U32 || tkn.type == TokenType::KEY_F32) ? 4 :
                            (tkn.type == TokenType::KEY_I64 || tkn.type == TokenType::KEY_U64 || tkn.type == TokenType::KEY_F64) ? 8 : 0;
            break;
        case TokenType::IDENTIFIER: // could be module.name or just name (struct, enum)
            if (start + 1 <= end && tokens[start].type == TokenType::OP_DOT && tokens[start + 1].type == TokenType::IDENTIFIER) {
                Token tkn_name = tokens[start++];
                int modID = curSrc.table_names.findName(tkn.text);
                if (modID == -1) {
                    throw std::runtime_error("E0311 Unknown module name: " + tkn.text + " at " + findLocation(tkn.location)); // E0311
                }
                int modIdx = findModule(modID);
                if (modIdx == -1) {
                    throw std::runtime_error("E0312 Module not loaded: " + tkn.text + " at " + findLocation(tkn.location)); // E0312
                }
                int typeID = modTables[modIdx]->export_types.findType(tkn_name.text);
                if (typeID == -1) {
                    throw std::runtime_error("E0313 Unknown type name: " + tkn_name.text + " in module " + tkn.text + " at " + findLocation(tkn_name.location)); // E0313
                }
                newType = modTables[modIdx]->export_types.types[typeID]->clone();
                newType->name = tkn.text + "." + newType->name;
            } else {
                newType = TypeNode(TypeNodeType::PRECOMPILE1, tkn.text, -1).clone();
            }
            break;
        default:
            throw std::runtime_error("E0314 Invalid type start token: " + tkn.text + " at " + findLocation(tkn.location)); // E0314
    }

    // parse modifiers
    while (start <= end) {
        Token mod = tokens[start++];
        if (mod.type == TokenType::OP_MUL) { // pointer *
            std::unique_ptr<TypeNode> ptrType = std::make_unique<TypeNode>(TypeNodeType::POINTER, "pointer", ArchSize);
            ptrType->direct = std::move(newType);
            newType = std::move(ptrType);

        } else if (mod.type == TokenType::OP_LBRACKET) { // array [
            if (start <= end && tokens[start].type == TokenType::OP_RBRACKET) { // []
                start++;
                std::unique_ptr<TypeNode> arrType = std::make_unique<TypeNode>(TypeNodeType::POINTER, "pointer", ArchSize);
                arrType->direct = std::move(newType);
                newType = std::move(arrType);
            } else if (start + 1 <= end && (tokens[start].type == TokenType::LIT_INT10 || tokens[start].type == TokenType::LIT_INT16) && tokens[start + 1].type == TokenType::OP_RBRACKET) { // [num]
                int arrLen = tokens[start].value.int_value;
                if (arrLen <= 0) {
                    throw std::runtime_error("E0315 Invalid array len: " + std::to_string(arrLen) + " at " + findLocation(tokens[start].location)); // E0315
                }
                start += 2;
                std::unique_ptr<TypeNode> arrType = std::make_unique<TypeNode>(TypeNodeType::ARRAY, "array", newType->size * arrLen);
                arrType->direct = std::move(newType);
                newType = std::move(arrType);
            } else {
                throw std::runtime_error("E0316 Invalid array type syntax at " + findLocation(mod.location)); // E0316
            }

        } else if (mod.type == TokenType::OP_LITTER) { // function <
            std::unique_ptr<TypeNode> funcType = std::make_unique<TypeNode>(TypeNodeType::FUNCTION, "function", ArchSize);
            funcType->direct = std::move(newType);
            newType = std::move(funcType);
            int funcEnd = findScopeEnd(tokens, start - 1); // ...>
            if (funcEnd == -1) {
                throw std::runtime_error("E0317 function type not ended at " + findLocation(mod.location)); // E0317
            }
            int parmStart;
            for (parmStart = start; start < funcEnd; start++) {
                if (tokens[start].type == TokenType::OP_COMMA) {
                    newType->indirects.push_back(std::move(parseGenericType(tokens, parmStart, start - 1, curSrc)));
                    parmStart = start + 1;
                }
            }
            if (parmStart < funcEnd) {
                newType->indirects.push_back(std::move(parseGenericType(tokens, parmStart, funcEnd - 1, curSrc)));
            }

        } else {
            throw std::runtime_error("E0318 Invalid type modifier token: " + mod.text + " at " + findLocation(mod.location)); // E0318
        }
    }
    return newType;
}

// parse struct definition, [can raise exception]
std::unique_ptr<TypeNode> Parser::parseStructDef(const std::string name, const std::vector<Token>& tokens, int start, int end) {
    std::unique_ptr<TypeNode> newType = std::make_unique<TypeNode>(TypeNodeType::STRUCT, name, -1);
    int pos = start;
    while (pos <= end) {
        const Token& tkn = tokens[pos++];
        
    }
    return newType;
}

// parse enum definition, [can raise exception]
std::unique_ptr<TypeNode> Parser::parseEnumDef(const std::string name, const std::vector<Token>& tokens, int start, int end) {
    
}