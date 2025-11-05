#include "baseFunc.h"

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

// Literal to string
std::string Literal::toString() {
    switch (node_type) {
        case LiteralType::INT:
            return std::to_string(int_value);
        case LiteralType::FLOAT:
            return std::to_string(float_value);
        case LiteralType::CHAR:
            return "'" + std::string(1, char_value) + "'";
        case LiteralType::STRING:
            return "\"" + string_value + "\"";
        default:
            return "";
    }
}

// TpInfo methods
bool TpInfo::isEqual(const TpInfo& other) const {
    if (node_type == TpInfoType::MEMCHUNK || other.node_type == TpInfoType::MEMCHUNK) {
        if (size == other.size && allign_req == other.allign_req) {
            return true;
        }
    }
    if (node_type != other.node_type || name != other.name || size != other.size) {
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
    for (size_t i = 0; i < indirects.size(); i++) {
        if (!indirects[i]->isEqual(*other.indirects[i])) {
            return false;
        }
    }
    return true;
}

std::unique_ptr<TpInfo> TpInfo::clone() const {
    auto newNode = std::make_unique<TpInfo>(node_type, name, size);
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

std::string TpInfo::toString() {
    switch (node_type) {
        case TpInfoType::PRIMITIVE: case TpInfoType::STRUCT: case TpInfoType::ENUM: case TpInfoType::MEMCHUNK:
            return name;
        case TpInfoType::POINTER:
            return direct ? direct->toString() + "*" : "invalid";
        case TpInfoType::SLICE:
            return direct ? direct->toString() + "[]" : "invalid";
        case TpInfoType::ARRAY:
            if (direct && direct->size > 0) {
                return direct->toString() + "[" + std::to_string(length) + "]";
            } else {
                return "invalid";
            }
        case TpInfoType::FUNCTION: {
            if (!direct) return "invalid";
            std::string result = direct->toString() + "(";
            for (size_t i = 0; i < indirects.size(); i++) {
                result += indirects[i]->toString();
                if (i < indirects.size() - 1) result += ",";
            }
            return result + ")";
        }
        default:
            return "invalid";
    }
}

// SrcFile methods
bool SrcFile::isEqual(const SrcFile& other) const {
    if (isTemplate) {
        return path == other.path && tmp_size == other.tmp_size && tmp_allign == other.tmp_allign;
    } else {
        return path == other.path;
    }
}

std::string SrcFile::toString() {
    std::string result = path;
    if (isTemplate) {
        result += "<";
        for (size_t i = 0; i < tmp_size.size(); i++) {
            result += std::to_string(tmp_size[i]);
            if (i < tmp_size.size() - 1) {
                result += ",";
            }
        }
        result += ">";
    }
    return result;
}

int SrcNmTable::findSrc(SrcFile& tgt) {
    if (tgt.isTemplate) {
        for (size_t i = 0; i < sources.size(); i++) {
            if (tgt.isEqual(*sources[i])) {
                return i;
            }
        }
    } else {
        auto it = lookup.find(tgt.path);
        if (it != lookup.end()) {
            return it->second;
        }
    }
    return -1;
}

void SrcNmTable::addSrc(std::unique_ptr<SrcFile> src) {
    if (!src->isTemplate) {
        lookup[src->path] = sources.size();
    }
    sources.push_back(std::move(src));
}