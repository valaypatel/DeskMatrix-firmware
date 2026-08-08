// tests/native/test_interrupt_queue.cpp
#include "test_framework.h"
#include "InterruptQueue.h"

int main() {
    InterruptQueue q;
    CHECK(q.empty());

    q.push("scr_jira_full");
    q.push("scr_calendar_full");
    CHECK(!q.empty());
    CHECK_EQ(q.size(), (size_t)2);

    std::string first;
    CHECK(q.popFront(first));
    CHECK_EQ(first, "scr_jira_full");

    std::string second;
    CHECK(q.popFront(second));
    CHECK_EQ(second, "scr_calendar_full");

    CHECK(q.empty());
    std::string none;
    CHECK(!q.popFront(none)); // popping empty queue returns false, doesn't crash

    TEST_SUMMARY();
}
