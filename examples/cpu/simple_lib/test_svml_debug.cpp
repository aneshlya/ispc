#include "ispc/ispc.h"
#include <iostream>
#include <vector>

int main() {
    if (!ispc::Initialize()) {
        std::cerr << "Failed to initialize ISPC\n";
        return 1;
    }

    std::cout << "Testing SVML configuration...\n";

    // Test the exact configuration that's failing
    std::vector<std::string> svml_args = {"ispc", "simple.ispc", "--target=sse4-i32x8", "-O3", "--math-lib=svml",
                                          "-o",   "debug_svml.o"};

    std::cout << "Command line: ";
    for (const auto &arg : svml_args) {
        std::cout << arg << " ";
    }
    std::cout << "\n";

    // Try direct compilation first
    std::cout << "Direct compilation: ";
    int direct_result = ispc::CompileFromArgs(svml_args);
    std::cout << "result=" << direct_result << "\n";

    // Try engine-based compilation
    std::cout << "Engine compilation: ";
    auto engine = ispc::ISPCEngine::CreateFromArgs(svml_args);
    if (!engine) {
        std::cout << "Engine creation failed\n";
    } else {
        int engine_result = engine->Execute();
        std::cout << "result=" << engine_result << "\n";
    }

    ispc::Shutdown();
    return 0;
}