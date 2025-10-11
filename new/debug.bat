set option="bugprone-*,modernize-use-auto,modernize-use-emplace,modernize-loop-convert,performance-for-range-copy,performance-unnecessary-value-param"
clang-tidy basicFunc.cpp tokenize.cpp parser.cpp test.cpp --checks=option -- -std=c++23 -Wall
pause