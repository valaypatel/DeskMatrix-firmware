// firmware/DeskMatrix/OrientationDebouncer.h
#pragma once
#include "services/ImuHardware.h"

// Debounces a stream of PanelOrientation readings into a stable value, so
// a brief transitional reading mid-rotation (or noise) doesn't flicker
// DND/BRB on and off. `stableReadingsNeeded` is how many consecutive
// identical readings are required before the reported orientation changes
// — mirrors TiltDebouncer's approach, generalized to a discrete 3-state
// classifier instead of a continuous angle threshold (see
// services/ImuHardware.h for why this project moved from tilt-angle to
// orientation classification).
class OrientationDebouncer {
public:
    explicit OrientationDebouncer(int stableReadingsNeeded) : needed_(stableReadingsNeeded) {}

    PanelOrientation update(PanelOrientation instant) {
        if (instant == pendingOrientation_) {
            pendingCount_++;
        } else {
            pendingOrientation_ = instant;
            pendingCount_ = 1;
        }

        if (pendingCount_ >= needed_) {
            stable_ = pendingOrientation_;
        }
        return stable_;
    }

private:
    int needed_;
    PanelOrientation pendingOrientation_ = PanelOrientation::NORMAL;
    int pendingCount_ = 0;
    PanelOrientation stable_ = PanelOrientation::NORMAL;
};
