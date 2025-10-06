#ifndef SOURCE_IO_H
#define SOURCE_IO_H

#include <string>
#include <fstream>
#include <stdexcept>

std::string readFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("File open_r fail: " + filename);
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    return content;
}

void writeFile(const std::string& filename, const std::string& content) {
    std::ofstream file(filename);
    if (!file.is_open()) {
       throw std::runtime_error("File open_w fail: " + filename);
    }
    file << content;
    file.close();
}

#endif // SOURCE_IO_H