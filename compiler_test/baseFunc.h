#ifndef BASE_FUNC_H
#define BASE_FUNC_H

#include <string>
#include <vector>
#include <variant>
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
    NPTR,
    BOOL,
    INT,
    FLOAT,
    STRING
};

struct Literal {
    using LiteralVariant = std::variant<int64_t, double, std::string>;

    LiteralType objType;
    LiteralVariant value;

    Literal(): objType(LiteralType::NONE), value(LiteralVariant()) {}
    Literal(void* n): objType(LiteralType::NPTR), value((int64_t)0) {}
    Literal(bool b): objType(LiteralType::BOOL), value(b ? (int64_t)1 : (int64_t)0) {}
    Literal(int64_t i): objType(LiteralType::INT), value(i) {}
    Literal(double d): objType(LiteralType::FLOAT), value(d) {}
    Literal(const std::string& s): objType(LiteralType::STRING), value(s) {}

    std::string toString();
};

std::vector<char> uniToByte(int uni); // convert unicode point to bytes
int byteToUni(std::vector<char> bytes); // convert bytes to unicode point

#endif // BASE_FUNC_H