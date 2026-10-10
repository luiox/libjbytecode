#include <cstdio>
#include <vector>
#include <cstdint>
#include "jbytecode/core/readers.hpp"
#include "test_utils.hpp"

using namespace jbytecode::core;

static std::vector<uint8_t> readFile2(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) return {};
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> d(sz);
    fread(d.data(), 1, sz, f);
    fclose(f);
    return d;
}

int main()
{
    auto d = readFile2("../../test_resource/generated/SimpleAdd.class");
    if (d.empty()) {
        printf("CANNOT READ FILE\n");
        return 1;
    }
    printf("File size: %zu\n", d.size());
    printf("Magic: %02X %02X %02X %02X\n", d[0], d[1], d[2], d[3]);
    printf("Version: %d.%d\n", (d[6] << 8) | d[7], (d[4] << 8) | d[5]);

    int cpc = (d[8] << 8) | d[9];
    printf("CP count: %d\n", cpc);
    printf("Header offset guess: %d\n", 10 + (cpc > 1 ? (d[12] << 8) | d[13] : 0));

    // Check CP entries
    int off = 10;
    for (int idx = 1; idx < cpc;) {
        int tag = d[off];
        printf("CP[%d]: tag=%d off=%d", idx, tag, off);
        if (tag == 1) {   // Utf8
            int len = (d[off + 1] << 8) | d[off + 2];
            printf(" len=%d data=\"", len);
            for (int i = 0; i < len && i < 40; i++) {
                if (d[off + 3 + i] >= 32 && d[off + 3 + i] < 127)
                    printf("%c", d[off + 3 + i]);
                else
                    printf("\\x%02X", d[off + 3 + i]);
            }
            printf("\"");
            off += 3 + len;
            if (tag == 5 || tag == 6)
                idx += 2;
            else
                idx++;
        }
        else if (tag == 7) {   // Class
            int ni = (d[off + 1] << 8) | d[off + 2];
            printf(" name_index=%d", ni);
            off += 3;
            idx++;
        }
        else {
            printf("\n");
            break;
        }
        printf("\n");
    }

    ca::str::Utf8StringArena arena;
    ClassReader              cr(arena, d);
    printf("\nClassName: \"%s\"\n", cr.getClassName().to_std_string().c_str());

    return 0;
}
