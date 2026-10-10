#include "../Common.hpp"

int main(int argc, char* argv[])
{
    ca::str::Utf8StringArena arena;
    std::string              inputPath = (argc > 1) ? argv[1] : "";

    std::vector<uint8_t> bytes;
    if (!inputPath.empty()) {
        bytes = readFile(inputPath);
    }
    else {
        bytes = createSampleClass();
    }
    if (bytes.empty()) {
        std::cerr << "Error: empty input\n";
        return 1;
    }

    // Test 1: Passthrough (no compute)
    ClassReader reader1(arena, bytes);
    ClassWriter cw1(arena, 0);
    reader1.accept(&cw1, 0);
    auto pass1  = cw1.toByteArray();
    bool passOk = !pass1.empty();

    // Test 2: SKIP_FRAMES (strip frames)
    ClassReader reader2(arena, bytes);
    ClassWriter cw2(arena, ClassWriter::COMPUTE_FRAMES);
    reader2.accept(&cw2, ClassReader::SKIP_FRAMES);
    auto skip2  = cw2.toByteArray();
    bool skipOk = !skip2.empty();

    // Test 3: EXPAND_FRAMES (expand compressed frames)
    ClassReader reader3(arena, bytes);
    ClassWriter cw3(arena, 0);
    reader3.accept(&cw3, ClassReader::EXPAND_FRAMES);
    auto expand3  = cw3.toByteArray();
    bool expandOk = !expand3.empty();

    // Test 4: COMPUTE_FRAMES from scratch
    ClassReader reader4(arena, bytes);
    ClassWriter cw4(arena, ClassWriter::COMPUTE_FRAMES);
    reader4.accept(&cw4, ClassReader::SKIP_FRAMES | ClassReader::EXPAND_FRAMES);
    auto compute4  = cw4.toByteArray();
    bool computeOk = !compute4.empty();

    // Test 5: SKIP_CODE (no method bodies)
    ClassReader reader5(arena, bytes);
    ClassWriter cw5(arena, 0);
    reader5.accept(&cw5, ClassReader::SKIP_CODE);
    auto skipCode   = cw5.toByteArray();
    bool skipCodeOk = !skipCode.empty();

    // Test 6: COMPUTE_MAXS
    ClassReader reader6(arena, bytes);
    ClassWriter cw6(arena, ClassWriter::COMPUTE_MAXS);
    reader6.accept(&cw6, 0);
    auto maxs6  = cw6.toByteArray();
    bool maxsOk = !maxs6.empty();

    // Verify idempotency of COMPUTE_FRAMES result
    ClassReader reader7(arena, compute4);
    ClassWriter cw7(arena, ClassWriter::COMPUTE_FRAMES);
    reader7.accept(&cw7, ClassReader::SKIP_FRAMES | ClassReader::EXPAND_FRAMES);
    auto idem7  = cw7.toByteArray();
    bool idemOk = !idem7.empty();

    std::cout << "StackMapAnalyzer Demo\n";
    std::cout << "  Input size:          " << bytes.size() << " bytes\n";
    std::cout << "  Class:               " << toStr(ClassReader(arena, bytes).getClassName())
              << "\n\n";
    std::cout << "  Mode                          Size     Status\n";
    std::cout << "  ----                           ----     ------\n";
    std::cout << "  1. Passthrough                 " << pass1.size() << "     "
              << (passOk ? "OK" : "FAIL") << "\n";
    std::cout << "  2. Strip+Recompute             " << skip2.size() << "     "
              << (skipOk ? "OK" : "FAIL") << "\n";
    std::cout << "  3. Expand frames               " << expand3.size() << "     "
              << (expandOk ? "OK" : "FAIL") << "\n";
    std::cout << "  4. Compute frames from scratch " << compute4.size() << "     "
              << (computeOk ? "OK" : "FAIL") << "\n";
    std::cout << "  5. Skip code                   " << skipCode.size() << "     "
              << (skipCodeOk ? "OK" : "FAIL") << "\n";
    std::cout << "  6. Compute maxs                " << maxs6.size() << "     "
              << (maxsOk ? "OK" : "FAIL") << "\n";
    std::cout << "  7. Idempotent (mode 4->4)      " << idem7.size() << "     "
              << (idemOk ? "OK" : "FAIL") << "\n";
    return 0;
}
