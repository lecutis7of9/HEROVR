#pragma once
// Version 0.0.2 hotfix: a recursion guard for the settings getters. A getter that is entered again on
// the same thread before it returned (a getter reaching itself through a migration or a default) sees
// reentered() and answers its default instead of recursing until the stack runs out.
namespace spidy {
class ReentryGuard {
public:
    explicit ReentryGuard(int& depth) : depth_(depth) { ++depth_; }
    ~ReentryGuard() { --depth_; }
    ReentryGuard(const ReentryGuard&) = delete;
    ReentryGuard& operator=(const ReentryGuard&) = delete;
    bool reentered() const { return depth_ > 1; }

private:
    int& depth_;
};
} // namespace spidy
