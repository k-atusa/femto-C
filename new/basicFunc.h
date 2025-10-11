#ifndef BASIC_FUNC_H
#define BASIC_FUNC_H

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
struct LocNode {
    int source_id;
    int line;

    LocNode() : source_id(-1), line(-1) {}
    LocNode(int src_id, int ln) : source_id(src_id), line(ln) {}
};

// table for managing source ID
class SourceTable {
    public:
    std::vector<std::string> sources;
    std::vector<bool> isFinished;

    int addSource(const std::string& source, bool finished);
    const std::string& getSource(int id) const;
    int findSource(const std::string& source) const;
    bool getStatus(int id);
    void setStatus(int id, bool finished);
    std::string toString();
};

enum class ValueType {
    NONE,
    INT10,
    INT16,
    FLOAT,
    CHAR,
    STRING
};

// for saving literal values
struct ValueNode {
    ValueType type;
    int64_t int_value;
    double float_value;
    char char_value;
    std::string string_value;

    ValueNode() : type(ValueType::NONE), int_value(0), float_value(0.0), char_value(0), string_value("") {}
    ValueNode(int64_t val) : type(ValueType::INT10), int_value(val), float_value(0.0), char_value(0), string_value("") {}
    ValueNode(double val) : type(ValueType::FLOAT), int_value(val), float_value(val), char_value(0), string_value("") {}
    ValueNode(char val) : type(ValueType::CHAR), int_value(val), float_value(0.0), char_value(val), string_value("") {} // can use char as int
    ValueNode(const std::string& val) : type(ValueType::STRING), int_value(0), float_value(0.0), char_value(0), string_value(val) {}

    std::string toString() const;
};

enum class TypeNodeType {
    NONE,
    PRIMITIVE,
    POINTER,
    ARRAY,
    FUNCTION,
    STRUCT,
    ENUM,
    ABSTRACT, // abstract struct, for defining looping struct
    PRECOMPILE1 // struct or enum in source
};

// represents a type in the type system
class TypeNode {
    public:
    TypeNodeType type;
    std::string name;
    int size; // size in bytes
    int length; // array length
    int offset; // struct member offset
    int allign_req; // allign requirement
    std::unique_ptr<TypeNode> direct; // ptr & arr target, func return
    std::vector<std::unique_ptr<TypeNode>> indirects; // func params, struct members

    TypeNode() : type(TypeNodeType::NONE), name(""), size(0), length(-1), offset(-1), allign_req(1), direct(nullptr), indirects() {}
    TypeNode(TypeNodeType tp, const std::string& n, int s) : type(tp), name(n), size(s), length(-1), offset(-1), allign_req(s), direct(nullptr), indirects() {}

    bool isEqual(const TypeNode& other) const;
    std::unique_ptr<TypeNode> clone() const;
    std::string toString(int depth = 0, bool verbose = false);
};

// one table per source
class TypeTable {
    public:
    int source_id;
    std::vector<std::unique_ptr<TypeNode>> types;

    TypeTable() : source_id(0), types() {}
    TypeTable(int id) : source_id(id), types() {}

    bool addType(std::unique_ptr<TypeNode> type);
    int findType(const std::string& name);
    std::string toString(int depth = 0);
};

enum class NameNodeType {
    NONE,
    GLOBAL,
    LOCAL,
    FUNCTION,
    STRUCT, // not used, struct info is in TypeTable
    MEMBER,
    METHOD,
    ENUM, // not used, enum info is in TypeTable
    ITEM,
    MODULE
};

// represents a named entity in the program
class NameNode {
    public:
    NameNodeType type;
    std::string name;
    int tag_value; // source_id, struct member pos, enum value, va_arg 1, const 2, volatile 3
    std::unique_ptr<TypeNode> type_node; // type info for global, local, function, member, method, item

    NameNode() : type(NameNodeType::NONE), name(""), tag_value(-1), type_node(nullptr) {}
    NameNode(NameNodeType tp, const std::string& nm, int tag) : type(tp), name(nm), tag_value(tag), type_node(nullptr) {}
    NameNode(NameNodeType tp, const std::string& nm, int tag, std::unique_ptr<TypeNode> tn) : type(tp), name(nm), tag_value(tag), type_node(std::move(tn)) {}

    std::unique_ptr<NameNode> clone() { return std::make_unique<NameNode>(type, name, tag_value, type_node->clone()); }
    std::string toString() const {
        if (type_node == nullptr) {
            return "_ " + name + ", type: " + std::to_string(static_cast<int>(type)) + ", tag: " + std::to_string(tag_value);
        } else {
            return type_node->toString() + " " + name + ", type: " + std::to_string(static_cast<int>(type)) + ", tag: " + std::to_string(tag_value);
        }
    }
};

// one table per source & scope
class NameTable {
    public:
    int source_id; // 0 for self
    std::vector<std::unique_ptr<NameNode>> names;

    NameTable() : source_id(0), names() {}
    NameTable(int id) : source_id(id), names() {}

    bool addName(std::unique_ptr<NameNode> name);
    int findName(const std::string& name);
    std::string toString(int depth = 0);
};

#endif // BASIC_FUNC_H
