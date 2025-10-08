#ifndef PARSER_H
#define PARSER_H

#include "basicFunc.h"

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
};

// parser class, only one per program
class Parser {
    public:
    CompileMessage printer;
    int ArchSize = 8;
    SourceTable srcTable;
    std::vector<std::unique_ptr<SrcModule>> modTables; // one per source

    Parser() : printer(), srcTable(), modTables() {}

    std::string parseSrc(const std::string& src_path); // returns error message or empty if ok

    private:
    std::string findLocation(LocNode loc); // get location string from loc node
    int findModule(int id); // find module index in modTables, -1 if not found

    std::string pass1(const std::vector<Token>& tokens, SrcModule& curSrc, const std::string& curPath); // manage import, create type table
    std::unique_ptr<TypeNode> parseGenericType(const std::vector<Token>& tokens, int start, int end, SrcModule& curSrc); // parse any type, [can raise exception]
    std::unique_ptr<TypeNode> parseStructDef(const std::string name, const std::vector<Token>& tokens, int start, int end); // parse struct type, [can raise exception]
    std::unique_ptr<TypeNode> parseEnumDef(const std::string name, const std::vector<Token>& tokens, int start, int end); // parse enum type, [can raise exception]
    
    std::string pass2(const std::vector<Token>& tokens, SrcModule& curSrc); // create name table
    std::string pass3(const std::vector<Token>& tokens, SrcModule& curSrc); // create ast nodes
};

#endif // PARSER_H