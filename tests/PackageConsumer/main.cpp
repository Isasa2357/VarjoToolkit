#include <VarjoToolkit/Core/VarjoSession.hpp>
#include <VarjoToolkit/Version.hpp>

#include <Varjo.h>

#include <iostream>
#include <memory>
#include <string>

int main()
{
    const std::string version = VARJOTOOLKIT_VERSION_STRING;
    if (version.empty()) {
        std::cerr << "VarjoToolkit version string is empty\n";
        return 1;
    }

    VarjoSession nullSession(std::shared_ptr<varjo_Session>{});
    if (nullSession.valid()) {
        std::cerr << "Null VarjoSession should be invalid\n";
        return 2;
    }

    std::cout << "VarjoToolkit package consumer test passed. version=" << version << "\n";
    return 0;
}
