# femto-C

A modern C-like language compiler project that aims to improve upon C's syntax and reduce common bugs.

## Project Structure

- **compiler_test/** - Main compiler implementation (C++)
  - Tokenizer, AST (Abstract Syntax Tree) phases 1-3
  - Includes Korean documentation files (기록사항.txt, 문법안.txt)
- **compiler_standard/** - Standard compiler version
- **code_stdlib/** - Standard library code
- **code_test/** - Test code samples

## Language Features

femto-C is designed as an improved C for operating system development with:

- Modern, intuitive syntax while maintaining C compatibility
- Strong typing with explicit type declarations
- Module system based on file names
- Value-type arrays and slice views
- Hoisting for structs and functions
- `defer` statement for cleanup
- Automatic `break` in `switch` statements
- Template support

See `compiler_test/문법안.txt` (in Korean) for detailed language specification.

## Building the Compiler

The compiler is written in C++20. To compile the source files:

```bash
cd compiler_test
g++ -std=c++20 -c baseFunc.cpp tokenizer.cpp ast1.cpp ast1ext.cpp ast2.cpp ast3.cpp
```

Note: This is currently a library implementation without a main entry point.

## Development

The project uses:
- C++20 standard
- clang-format, clang-tidy for code style
- VSCode configuration included in `compiler_test/.vscode/`

## Documentation

- `compiler_test/문법안.txt` - Language grammar specification (Korean)
- `compiler_test/기록사항.txt` - Implementation notes (Korean)
- `code_test/test.txt` - Example code showing language features

## License

See repository for license information.
