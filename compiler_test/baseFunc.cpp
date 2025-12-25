#include "baseFunc.h"

// read text data, [can raise exception]
std::string readFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("E0001 File open_r fail: " + filename); // E0001
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
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
    switch (objType) {
        case LiteralType::NPTR:
            return "NULL";
        case LiteralType::BOOL:
            return std::get<int64_t>(value) ? "true" : "false";
        case LiteralType::INT:
            return std::to_string(std::get<int64_t>(value));
        case LiteralType::FLOAT:
            return std::to_string(std::get<double>(value));
        case LiteralType::STRING:
            return std::get<std::string>(value);
        default:
            return "";
    }
}

// convert unicode point to bytes
std::vector<char> uniToByte(int uni) {
    std::vector<char> bytes;
    if (uni <= 0x7F) {
        bytes.push_back(uni);
    } else if (uni <= 0x7FF) {
        bytes.push_back(0xC0 | (uni >> 6));
        bytes.push_back(0x80 | (uni & 0x3F));
    } else if (uni <= 0xFFFF) {
        bytes.push_back(0xE0 | (uni >> 12));
        bytes.push_back(0x80 | ((uni >> 6) & 0x3F));
        bytes.push_back(0x80 | (uni & 0x3F));
    } else if (uni <= 0x10FFFF) {
        bytes.push_back(0xF0 | (uni >> 18));
        bytes.push_back(0x80 | ((uni >> 12) & 0x3F));
        bytes.push_back(0x80 | ((uni >> 6) & 0x3F));
        bytes.push_back(0x80 | (uni & 0x3F));
    }
    return bytes;   
}

// convert bytes to unicode point
int byteToUni(std::vector<char> bytes) {
    int uni = -1;
    if (bytes.size() == 1) {
        uni = bytes[0];
    } else if (bytes.size() == 2) {
        uni = (bytes[0] & 0x1F) << 6 | (bytes[1] & 0x3F);
    } else if (bytes.size() == 3) {
        uni = (bytes[0] & 0x0F) << 12 | (bytes[1] & 0x3F) << 6 | (bytes[2] & 0x3F);
    } else if (bytes.size() == 4) {
        uni = (bytes[0] & 0x07) << 18 | (bytes[1] & 0x3F) << 12 | (bytes[2] & 0x3F) << 6 | (bytes[3] & 0x3F);
    }
    return uni;
}