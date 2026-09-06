// firmware/DeskMatrix/services/ImuHardware.h
#pragma once

// Initializes the onboard QMI8658 IMU. Returns false if the sensor isn't
// found/doesn't init — caller should disable DND/BRB for this boot (per the
// design spec's error handling) rather than treat this as fatal.
bool imuBegin();

// Returns the current left/right tilt angle in degrees (positive = right,
// negative = left). Only meaningful if imuBegin() returned true.
float imuReadTiltDegrees();
