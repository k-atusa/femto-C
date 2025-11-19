#ifndef BASE_FUNC_H
#define BASE_FUNC_H

#include <string>
#include <vector>
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
    int srcLoc;
    int line;

    Location(): srcLoc(-1), line(-1) {}
    Location(int src, int ln): srcLoc(src), line(ln) {}
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
    LiteralType objType;
    int64_t intValue;
    double floatValue;
    char charValue;
    std::string stringValue;

    Literal(): objType(LiteralType::NONE) {}
    Literal(int64_t v): objType(LiteralType::INT), intValue(v), floatValue(0.0), charValue(0), stringValue("") {}
    Literal(double v): objType(LiteralType::FLOAT), intValue(0), floatValue(v), charValue(0), stringValue("") {}
    Literal(char v): objType(LiteralType::CHAR), intValue(v), floatValue(0.0), charValue(v), stringValue("") {}
    Literal(const std::string& v): objType(LiteralType::STRING), intValue(0), floatValue(0.0), charValue(0), stringValue(v) {}

    std::string toString();
};

#endif // BASE_FUNC_H