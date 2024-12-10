#include "ClangFormat.h"
#include <iostream>

int main() {
    std::string source;
    // Read source code from stdin until EOF
    for (std::string line; std::getline(std::cin, line);) {
        source += line + "\n";
    }
    std::cout<< "Formated Code:" << std::endl;
    std::cout<<format(source);
}