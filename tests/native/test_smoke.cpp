// tests/native/test_smoke.cpp
#include "test_framework.h"

int main() {
    CHECK(1 == 1);
    CHECK_EQ(2 + 2, 4);
    TEST_SUMMARY();
}
