// firmware/DeskMatrix/services/ImuHardware.h
#pragma once

// Initializes the onboard QMI8658 IMU. Returns false if the sensor isn't
// found/doesn't init — caller should disable DND/BRB for this boot (per the
// design spec's error handling) rather than treat this as fatal.
bool imuBegin();

// The physical gesture here isn't a rock/tilt — the panel sits on a desk
// stand (with some backward lean) as a "frame" and gets rotated 90° in
// place to change which edge faces up. Confirmed on real hardware: each of
// the 3 named orientations has one accelerometer axis reading near +/-1g
// (dominant) while the other stays small, cleanly distinguishing them —
//   A-top/C-bottom (normal): ax dominant, positive (~+0.975)
//   D-top/B-bottom (DND):    ay dominant, positive (~+1.116)
//   B-top/D-bottom (BRB):    ay dominant, negative (predicted by symmetry
//                            with the other two measured pairs — NOT
//                            physically confirmed; the panel's charging
//                            cable made this specific rotation untestable
//                            when this was calibrated. Verify against
//                            Serial output the first time BRB is used for
//                            real and adjust the sign here if backwards.)
// C-top/A-bottom (measured: ax dominant, negative, ~-1.016) isn't one of
// the named gestures and falls back to NORMAL, same as any in-between/
// transitional reading during the rotation itself.
enum class PanelOrientation { NORMAL, DND, BRB };

// Classifies the panel's current rotation. Only meaningful if imuBegin()
// returned true (returns NORMAL otherwise).
PanelOrientation imuReadOrientation();
