#include "tokenize.h"
#include "baseNode.h"
#include "sourceIO.h"

int main() {
    std::string s = readFile("test_in.txt");
    std::vector<Token> t = tokenize(s, "test_in.txt", 0);
    std::string out;
    for (auto& token : t) {
        out += token.toString() + "\n";
    }
    writeFile("test_out.txt", out);
    return 0;
}