#include "basicFunc.h"

// read text data, [can raise exception]
std::string readFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("E0001 File open_r fail: " + filename); // E0001
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    return content;
}

// write text data, [can raise exception]
void writeFile(const std::string& filename, const std::string& content) {
    std::ofstream file(filename);
    if (!file.is_open()) {
       throw std::runtime_error("E0002 File open_w fail: " + filename); // E0002
    }
    file << content;
    file.close();
}

// get file name from path, [can raise exception]
std::string getFileName(const std::string& path) {
    try {
        std::filesystem::path p = std::filesystem::path(path);
        return p.filename().string();
    } catch (const std::filesystem::filesystem_error& e) {
        throw std::runtime_error("E0003 Get f_name fail: " + path + ", " + e.what()); // E0003
    }
}

// get working directory from path, [can raise exception]
std::string getWorkingDir(const std::string& path) {
    try {
        std::filesystem::path p = std::filesystem::path(path);
        if (p.has_parent_path()) {
            return p.parent_path().string();
        } else {
            return ".";
        }
    } catch (const std::filesystem::filesystem_error& e) {
        throw std::runtime_error("E0004 Get w_dir fail: " + path + ", " + e.what()); // E0004
    }
}

// convert to absolute path, [can raise exception]
std::string absPath(const std::string& path, const std::string& baseDir) {
    try {
        std::string relative = path;
        for (size_t i = 0; i < relative.length(); ++i) {
            if (relative[i] == '\\') {
                relative[i] = '/';
            }
        }
        std::filesystem::path base = std::filesystem::canonical(std::filesystem::path(baseDir));
        for (;;) {
            if (relative.length() > 2 && relative[0] == '.' && relative[1] == '/') {
                relative = relative.substr(2);
            } else if (relative.length() > 3 && relative[0] == '.' && relative[1] == '.' && relative[2] == '/') {
                relative = relative.substr(3);
                if (base.has_parent_path()) {
                    base = base.parent_path();
                }
            } else {
                break;
            }
        }
        return std::filesystem::canonical(base / std::filesystem::path(relative)).string();
    } catch (const std::filesystem::filesystem_error& e) {
        throw std::runtime_error("E0005 Path resolve fail: <" + path + ", " + baseDir + ">, " + e.what()); // E0005
    }
}

// source table methods
int SourceTable::addSource(const std::string& source, bool finished) {
    sources.push_back(source);
    isFinished.push_back(finished);
    return sources.size() - 1;
}

const std::string& SourceTable::getSource(int id) const {
    if (id >= 0 && id < static_cast<int>(sources.size())) {
        return sources[id];
    }
    static const std::string empty = "";
    return empty;
}

int SourceTable::findSource(const std::string& source) const {
    for (size_t i = 0; i < sources.size(); ++i) {
        if (sources[i] == source) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool SourceTable::getStatus(int id) {
    if (id >= 0 && id < static_cast<int>(isFinished.size())) {
        return isFinished[id];
    }
    return false;
}

void SourceTable::setStatus(int id, bool finished) {
    if (id >= 0 && id < static_cast<int>(isFinished.size())) {
        isFinished[id] = finished;
    }
}

std::string SourceTable::toString() {
    std::string result;
    for (size_t i = 0; i < sources.size(); ++i) {
        result += "SrcID " + std::to_string(i) + ": " + sources[i] + "\n";
    }
    return result;
}

// value node to string
std::string ValueNode::toString() const {
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

// type node methods
bool TypeNode::isEqual(const TypeNode& other) const {
    if (type != other.type || name != other.name || size != other.size) {
        return false;
    }
    if ((direct == nullptr) != (other.direct == nullptr)) {
        return false;
    }
    if (direct && other.direct && !direct->isEqual(*other.direct)) {
        return false;
    }
    if (indirects.size() != other.indirects.size()) {
        return false;
    }
    for (size_t i = 0; i < indirects.size(); ++i) {
        if (!indirects[i]->isEqual(*other.indirects[i])) {
            return false;
        }
    }
    return true;
}

std::unique_ptr<TypeNode> TypeNode::clone() const {
    auto newNode = std::make_unique<TypeNode>(type, name, size);
    newNode->length = length;
    newNode->offset = offset;
    newNode->allign_req = allign_req;
    if (direct) {
        newNode->direct = direct->clone();
    }
    for (const auto& ind : indirects) {
        newNode->indirects.push_back(ind->clone());
    }
    return newNode;
}

std::string TypeNode::toString(int depth, bool verbose) {
    if (verbose) {
        std::string indent(depth * 2, ' ');
        std::string result;
        result += indent + "TypeNode type: " + std::to_string(static_cast<int>(type)) + "\n";
        result += indent + "name: " + name + "\n";
        result += indent + "size: " + std::to_string(size) + "\n";
        result += indent + "length: " + std::to_string(length) + "\n";
        result += indent + "offset: " + std::to_string(offset) + "\n";
        result += indent + "allign requirement: " + std::to_string(allign_req) + "\n";
        if (direct) {
            result += indent + "direct:\n" + direct->toString(depth + 1, true) + "\n";
        }
        for (const auto& ind : indirects) {
            result += indent + "indirect:\n" + ind->toString(depth + 1, true) + "\n";
        }
        return result;
    } else {
        switch (type) {
            case TypeNodeType::PRIMITIVE: case TypeNodeType::STRUCT: case TypeNodeType::ENUM: case TypeNodeType::ABSTRACT:
                return name;
            case TypeNodeType::POINTER:
                return direct ? direct->toString() + "*" : "invalid";
            case TypeNodeType::ARRAY:
                if (direct && direct->size > 0) {
                    return direct->toString() + "[" + std::to_string(length) + "]";
                } else {
                    return "invalid";
                }
            case TypeNodeType::FUNCTION: {
                if (!direct) return "invalid";
                std::string result = direct->toString() + "(";
                for (size_t i = 0; i < indirects.size(); ++i) {
                    result += indirects[i]->toString();
                    if (i < indirects.size() - 1) result += ",";
                }
                return result + ")";
            }
            default:
                return "invalid";
        }
    }
}

// type table methods, 
bool TypeTable::addType(std::unique_ptr<TypeNode> type) {
    for (const auto& existing : types) {
        if (existing->name == type->name) {
            return false; // already exists
        }
    }
    types.push_back(std::move(type));
    return true;
}

int TypeTable::findType(const std::string& name) {
    for (size_t i = 0; i < types.size(); ++i) {
        if (types[i]->name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::string TypeTable::toString(int depth) {
    std::string indent(depth * 2, ' ');
    std::string result;
    result += indent + "TypeTable id: " + std::to_string(source_id) + "\n";
    for (const auto& type : types) {
        result += type->toString(depth + 1, true);
    }
    return result + "\n";
}

// name table methods
bool NameTable::addName(std::unique_ptr<NameNode> name){
    for (const auto& existing : names) {
        if (existing->name == name->name) {
            return false; // already exists
        }
    }
    names.push_back(std::move(name));
    return true;
}

int NameTable::findName(const std::string& name) {
    for (size_t i = 0; i < names.size(); i++) {
        if (names[i]->name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::string NameTable::toString(int depth) {
    std::string indent(depth * 2, ' ');
    std::string result;
    result += indent + "NameTable id: " + std::to_string(source_id) + "\n";
    for (const auto& name : names) {
        result += indent + name->toString() + "\n";
    }
    return result + "\n";
}