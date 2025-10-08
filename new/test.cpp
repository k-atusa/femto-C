#include "sourceIO.h"
#include <iostream>

int main() {
    std::cout << getWorkingDir("parser.cpp") << std::endl;
    std::cout << absPath("./lib.txt", "../test") << std::endl;
    std::cout << absPath("../test/main.txt", "./") << std::endl;
    return 0;
}