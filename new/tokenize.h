#ifndef TOKENIZE_H
#define TOKENIZE_H

#include "baseNode.h"

std::vector<Token> tokenize(const std::string& source, const std::string filename, const int source_id);

#endif // TOKENIZE_H