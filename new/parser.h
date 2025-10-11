#ifndef PARSER_H
#define PARSER_H

#include "tokenize.h"

// represents a source module
class SrcModule {
    public:
    int source_id;
    TypeTable export_types; // struct & enum exports
    NameTable export_names; // function & global exports
    TypeTable table_types; // local types
    NameTable table_names; // local names
    // add ast nodes /////

    SrcModule(int id) : source_id(id), export_types(id), export_names(id), table_types(id), table_names(id) {}
    
    std::string toString(const std::string& path);
};

// parser class, only one per program
class Parser {
    public:
    CompileMessage printer;
    int ArchSize;
    SourceTable srcTable;
    std::vector<std::unique_ptr<SrcModule>> modTables; // one per source

    Parser() : printer(), ArchSize(8), srcTable(), modTables() {}
    Parser(int asz) : printer(), ArchSize(asz), srcTable(), modTables() {}

    std::string toString();
    std::string parseSrc(const std::string& src_path); // returns error message or empty if ok

    private:
    std::string findLocation(LocNode loc); // get location string from loc node
    int findModule(int id); // find module index in modTables, -1 if not found

    std::string pass1(TokenProvider& prov, SrcModule& curSrc, const std::string& curPath); // manage import, create type table
    std::unique_ptr<TypeNode> parseGenericType(TokenProvider& prov, SrcModule& curSrc); // parse any type, [can raise exception]
    std::unique_ptr<TypeNode> parseStructDef(const std::string name, TokenProvider& prov, SrcModule& curSrc); // parse struct type, [can raise exception]
    std::unique_ptr<TypeNode> parseEnumDef(const std::string name, TokenProvider& prov, SrcModule& curSrc); // parse enum type, [can raise exception]
    
    std::string pass2(TokenProvider& prov, SrcModule& curSrc); // create name table
    std::string parseGlobalDef(TokenProvider& prov, SrcModule& curSrc, std::unique_ptr<TypeNode> front_type, int tag); // parse global variable definition
    std::string parseFunctionDef(TokenProvider& prov, SrcModule& curSrc, std::unique_ptr<TypeNode> front_type, int tag); // parse function definition

    std::string pass3(TokenProvider& prov, SrcModule& curSrc); // create ast nodes
};

#endif // PARSER_H