#pragma once

enum class TiltDirection { CENTER, LEFT, RIGHT };

// Debounces a stream of tilt-angle readings (degrees, positive = right,
// negative = left) into a stable TiltDirection, so brief bumps don't
// trigger DND/BRB. `thresholdDeg` is how far from level counts as a tilt;
// `stableReadingsNeeded` is how many consecutive readings past threshold
// (or back within it) are required before the reported direction changes.
class TiltDebouncer {
public:
    TiltDebouncer(float thresholdDeg, int stableReadingsNeeded)
        : threshold_(thresholdDeg), needed_(stableReadingsNeeded) {}

    TiltDirection update(float angleDeg) {
        TiltDirection instant = TiltDirection::CENTER;
        if (angleDeg <= -threshold_) instant = TiltDirection::LEFT;
        else if (angleDeg >= threshold_) instant = TiltDirection::RIGHT;

        if (instant == pendingDirection_) {
            pendingCount_++;
        } else {
            pendingDirection_ = instant;
            pendingCount_ = 1;
        }

        if (pendingCount_ >= needed_) {
            stable_ = pendingDirection_;
        }
        return stable_;
    }

private:
    float threshold_;
    int needed_;
    TiltDirection pendingDirection_ = TiltDirection::CENTER;
    int pendingCount_ = 0;
    TiltDirection stable_ = TiltDirection::CENTER;
};
