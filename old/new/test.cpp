#include <iostream>
#include "parser.h"

int main() {
    Parser p;
    p.printer.level = 1;
    try {
        std::cout << p.parseSrc(absPath("../test/main.txt", "./")) << std::endl;
        writeFile("./debug.txt", p.toString());
    } catch (const std::runtime_error& e) {
        std::cout << e.what() << std::endl;
    }
    std::cout << "test completed" << std::endl;
    return 0;
}