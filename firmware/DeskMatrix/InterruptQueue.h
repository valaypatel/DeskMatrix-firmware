#pragma once
#include <string>
#include <deque>

class InterruptQueue {
public:
    void push(const std::string& screenId) { queue_.push_back(screenId); }
    bool empty() const { return queue_.empty(); }
    size_t size() const { return queue_.size(); }

    bool popFront(std::string& out) {
        if (queue_.empty()) return false;
        out = queue_.front();
        queue_.pop_front();
        return true;
    }

private:
    std::deque<std::string> queue_;
};
