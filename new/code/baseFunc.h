#ifndef BASE_FUNC_H
#define BASE_FUNC_H

#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <filesystem>

// compiler messsage
class CompileMessage {
    public:
    int level;

    CompileMessage() : level(3) {} // default level 3
    CompileMessage(int lvl) : level(lvl) {}

    void Log(const std::string& msg, int lvl) {
        if (lvl >= level) {
            std::cout << msg << std::endl;
        }
    }
};

// read text data, [can raise exception]
std::string readFile(const std::string& filename);

// write text data, [can raise exception]
void writeFile(const std::string& filename, const std::string& content);

// get file name from path, [can raise exception]
std::string getFileName(const std::string& path);

// get working directory from path, [can raise exception]
std::string getWorkingDir(const std::string& path);

// convert to absolute path, [can raise exception]
std::string absPath(const std::string& path, const std::string& baseDir);

// indicates code position
struct Location {
    int source_id;
    int line;

    Location(): source_id(-1), line(-1) {}
    Location(int src_id, int ln): source_id(src_id), line(ln) {}
};

// indicates literal value
enum class LiteralType {
    NONE,
    INT,
    FLOAT,
    CHAR,
    STRING
};

struct Literal {
    LiteralType node_type;
    int64_t int_value;
    double float_value;
    char char_value;
    std::string string_value;

    Literal(): node_type(LiteralType::NONE) {}
    Literal(int64_t v): node_type(LiteralType::INT), int_value(v), float_value(0.0), char_value(0), string_value("") {}
    Literal(double v): node_type(LiteralType::FLOAT), int_value(0), float_value(v), char_value(0), string_value("") {}
    Literal(char v): node_type(LiteralType::CHAR), int_value(0), float_value(0.0), char_value(v), string_value("") {}
    Literal(const std::string& v): node_type(LiteralType::STRING), int_value(0), float_value(0.0), char_value(0), string_value(v) {}

    std::string toString();
};

// indicates single type component
enum class TpInfoType {
    NONE,
    PRIMITIVE,
    POINTER,
    SLICE,
    ARRAY,
    FUNCTION,
    STRUCT, // only for decl node
    ENUM,
    MEMCHUNK, // for template, structs
    PRECOMPILE1 // struct or enum in source
};

class TpInfo {
    public:
    TpInfoType node_type;
    std::string name;
    int size; // size in bytes
    int length; // array length
    int offset; // struct member offset
    int allign_req; // allign requirement
    std::unique_ptr<TpInfo> direct; // ptr & arr target, func return
    std::vector<std::unique_ptr<TpInfo>> indirects; // func params, struct members

    TpInfo() : node_type(TpInfoType::NONE), name(""), size(0), length(-1), offset(-1), allign_req(1), direct(nullptr), indirects() {}
    TpInfo(TpInfoType tp, const std::string& n, int s) : node_type(tp), name(n), size(s), length(-1), offset(-1), allign_req(s), direct(nullptr), indirects() {}

    bool isEqual(const TpInfo& other) const;
    std::unique_ptr<TpInfo> clone() const;
    std::string toString();
};

#endif // BASE_FUNC_H