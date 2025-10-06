#ifndef BASE_NODE_H
#define BASE_NODE_H

#include <string>
#include <vector>
#include <memory>

struct LocNode {
    int source_id;
    int line;

    LocNode() : source_id(-1), line(-1) {}
    LocNode(int src_id, int ln) : source_id(src_id), line(ln) {}
};

class SourceTable {
    public:
    std::vector<std::string> sources;

    int addSource(const std::string& source) {
        sources.push_back(source);
        return sources.size() - 1;
    }

    const std::string& getSource(int id) const {
        if (id >= 0 && id < static_cast<int>(sources.size())) {
            return sources[id];
        }
        static const std::string empty = "";
        return empty;
    }

    int findSource(const std::string& source) const {
        for (size_t i = 0; i < sources.size(); ++i) {
            if (sources[i] == source) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    std::string toString() {
        std::string result;
        for (size_t i = 0; i < sources.size(); ++i) {
            result += "SrcID " + std::to_string(i) + ": " + sources[i] + "\n";
        }
        return result;
    }
};

enum class ValueType {
    NONE,
    INT10,
    INT16,
    FLOAT,
    CHAR,
    STRING
};

struct ValueNode {
    ValueType type;
    int int_value;
    double float_value;
    char char_value;
    std::string string_value;

    ValueNode() : type(ValueType::NONE) {}
    ValueNode(int val) : type(ValueType::INT10), int_value(val) {}
    ValueNode(double val) : type(ValueType::FLOAT), float_value(val) {}
    ValueNode(char val) : type(ValueType::CHAR), char_value(val) {}
    ValueNode(const std::string& val) : type(ValueType::STRING), string_value(val) {}

    std::string toString() const {
        switch (type) {
            case ValueType::INT10:
                return std::to_string(int_value);
            case ValueType::FLOAT:
                return std::to_string(float_value);
            case ValueType::CHAR:
                return "'" + std::string(1, char_value) + "'";
            case ValueType::STRING:
                return "\"" + string_value + "\"";
            default:
                return "";
        }
    }
};

enum class TypeNodeType {
    NONE,
    PRIMITIVE,
    POINTER,
    ARRAY,
    FUNCTION,
    STRUCT,
    ENUM
};

// represents a type in the type system
class TypeNode {
    public:
    TypeNodeType type;
    std::string name;
    int size; // size in bytes
    std::unique_ptr<TypeNode> direct; // ptr & arr target, func return
    std::vector<std::unique_ptr<TypeNode>> indirects; // func params, struct members

    TypeNode() : type(TypeNodeType::NONE), name(""), size(0), direct(nullptr), indirects() {}
    TypeNode(TypeNodeType tp, const std::string& n, int s) : type(tp), name(n), size(s), direct(nullptr), indirects() {}

    std::string toString(int depth = 0, bool verbose = false) {
        if (verbose) {
            std::string indent(depth * 2, ' ');
            std::string result;
            result += indent + "TypeNode type: " + std::to_string(static_cast<int>(type)) + "\n";
            result += indent + "name: " + name + "\n";
            result += indent + "size: " + std::to_string(size) + "\n";
            if (direct) {
                result += indent + "direct:\n" + direct->toString(depth + 1);
            }
            for (const auto& ind : indirects) {
                result += indent + "indirect:\n" + ind->toString(depth + 1);
            }
            return result + "\n";
        } else {
            switch (type) {
                case TypeNodeType::PRIMITIVE:
                    return name;
                case TypeNodeType::POINTER:
                    return direct ? direct->toString() + "*" : "invalid";
                case TypeNodeType::ARRAY:
                    if (direct && direct->size > 0) {
                        return direct->toString() + "[" + std::to_string(size / direct->size) + "]";
                    } else {
                        return "invalid";
                    }
                case TypeNodeType::FUNCTION: {
                    if (!direct) return "invalid";
                    std::string result = direct->toString() + "<";
                    for (size_t i = 0; i < indirects.size(); ++i) {
                        result += indirects[i]->toString();
                        if (i < indirects.size() - 1) result += ",";
                    }
                    return result + ">";
                }
                case TypeNodeType::STRUCT:
                    return "struct " + name;
                case TypeNodeType::ENUM:
                    return "enum " + name;
                default:
                    return "invalid";
            }
        }
    }
};

// one table per source
class TypeTable {
    public:
    int source_id; // 0 for self
    std::string source_name; // alias for the namespace
    std::vector<std::unique_ptr<TypeNode>> types;

    TypeTable() : source_id(0), source_name(""), types() {}
    TypeTable(int id, const std::string& name) : source_id(id), source_name(name), types() {}

    std::string toString(int depth = 0) {
        std::string indent(depth * 2, ' ');
        std::string result;
        result += indent + "TypeTable source: " + std::to_string(source_id) + " (" + source_name + ")\n";
        for (const auto& type : types) {
            result += type->toString(depth + 1, true);
        }
        return result + "\n";
    }
};

enum class NameNodeType {
    NONE,
    GLOBAL,
    LOCAL,
    PARAM,
    MEMBER,
    STRUCT,
    ENUM,
    FUNCTION
};

// represents a named entity in the program
class NameNode {
    public:
    NameNodeType type;
    std::string name;
    std::string alias;
    int name_id;

    NameNode() : type(NameNodeType::NONE), name(""), alias(""), name_id(-1) {}
    NameNode(NameNodeType tp, const std::string& nm, const std::string& al, int i) : type(tp), name(nm), alias(al), name_id(i) {}

    std::string toString() const {
        return "NameNode type: " + std::to_string(static_cast<int>(type)) + ", name: " + name + "<" + alias + ">, id: " + std::to_string(name_id);
    }
};

// one table per source & scope
class NameTable {
    public:
    int source_id; // 0 for self
    std::string source_name; // alias for the namespace
    std::vector<std::unique_ptr<NameNode>> names;

    NameTable() : source_id(0), source_name(""), names() {}
    NameTable(int id, const std::string& name) : source_id(id), source_name(name), names() {}

    std::string toString(int depth = 0) {
        std::string indent(depth * 2, ' ');
        std::string result;
        result += indent + "NameTable source: " + std::to_string(source_id) + " (" + source_name + ")\n";
        for (const auto& name : names) {
            result += indent + name->toString() + "\n";
        }
        return result + "\n";
    }
};

#endif // BASE_NODE_H