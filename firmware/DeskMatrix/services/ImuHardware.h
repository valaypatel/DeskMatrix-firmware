// firmware/DeskMatrix/services/ImuHardware.h
#pragma once

// Initializes the onboard QMI8658 IMU. Returns false if the sensor isn't
// found/doesn't init — caller should disable DND/BRB for this boot (per the
// design spec's error handling) rather than treat this as fatal.
bool imuBegin();

// Returns the current left/right tilt angle in degrees (positive = right,
// negative = left). Only meaningful if imuBegin() returned true.
float imuReadTiltDegrees();

// Enables double-tap detection. Call once after a successful imuBegin().
// Returns false if the IMU isn't ready — the caller should just skip the
// feature for this boot, same as imuBegin()'s own failure handling.
//
// The QMI8658's own hardware tap detector was tried first and never
// classified a real desk-tap during on-device testing (its internal
// algorithm is tuned for a tap on the device itself, not one transmitted
// through a desk) — see imuCheckDoubleTap() for the software-based
// detector used instead.
bool imuEnableTap();

// Polls the raw accelerometer for a double-tap (two sharp jolts within a
// short window — see the constants inside for exact values, tuned against
// real captured desk-tap data) and returns true exactly once per detected
// double-tap. Call this frequently — every loop() iteration is fine, this
// does not gate itself — there's no interrupt pin wired on this board and
// a tap is a brief transient, so a coarse poll interval could miss it.
// Only meaningful if imuEnableTap() returned true.
bool imuCheckDoubleTap();
