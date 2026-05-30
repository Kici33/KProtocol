#ifdef NDEBUG
#error "kprotocol tests must compile with assert() enabled, even in Release builds."
#endif

#include <cassert>

int main() {
    assert(true);
    return 0;
}
