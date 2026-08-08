// firmware/DeskMatrix/ImuHardware.cpp
//
// QMI8658 IMU driver for the Waveshare ESP32-S3 RGB Matrix board.
//
// Library/pins/init sequence were taken from Waveshare's own reference
// example for this exact board:
//   https://github.com/waveshareteam/ESP32-S3-RGB-Matrix
//   example/arduino_v3.3.7/08_Sensor_Test/08_Sensor_Test.ino
// which uses the "SensorLib" Arduino library (by Lewis He / lewisxhe,
// installed via `arduino-cli lib install "SensorLib"`), the
// SensorQMI8658.hpp driver class, I2C pins SDA=47/SCL=48, and an init
// sequence of: Wire.begin(sda, scl) -> probe both possible I2C addresses
// -> qmi.begin(Wire, addr) -> configAccelerometer(...) -> enableAccelerometer().

#include "services/ImuHardware.h"

#include <Arduino.h>
#include <Wire.h>
#include <SensorQMI8658.hpp>

namespace {
constexpr int kImuSdaPin = 47;
constexpr int kImuSclPin = 48;

SensorQMI8658 qmi;
bool imuReady = false;

bool probeI2cAddress(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

bool readI2cRegister8(uint8_t addr, uint8_t reg, uint8_t& value) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    const uint8_t readLen = Wire.requestFrom(static_cast<int>(addr), 1);
    if (readLen != 1 || !Wire.available()) return false;
    value = Wire.read();
    return true;
}

// Matches Waveshare's example: probe both possible QMI8658 I2C addresses
// and confirm the WHO_AM_I register (0x00) reads back 0x05 before trusting
// the address.
uint8_t detectQmiAddress() {
    const uint8_t candidates[] = {QMI8658_H_SLAVE_ADDRESS, QMI8658_L_SLAVE_ADDRESS};
    for (uint8_t addr : candidates) {
        uint8_t whoami = 0;
        if (probeI2cAddress(addr) && readI2cRegister8(addr, 0x00, whoami) && whoami == 0x05) {
            return addr;
        }
    }
    return 0;
}
}  // namespace

bool imuBegin() {
    Wire.begin(kImuSdaPin, kImuSclPin);
    Wire.setClock(400000);

    uint8_t addr = detectQmiAddress();
    if (addr == 0) {
        imuReady = false;
        return false;
    }

    delay(20);

    if (!qmi.begin(Wire, addr)) {
        imuReady = false;
        return false;
    }

    if (!qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_4G,
                                  SensorQMI8658::ACC_ODR_125Hz,
                                  SensorQMI8658::LPF_MODE_0)) {
        imuReady = false;
        return false;
    }

    if (!qmi.enableAccelerometer()) {
        imuReady = false;
        return false;
    }

    imuReady = true;
    return true;
}

float imuReadTiltDegrees() {
    if (!imuReady) return 0.0f;

    float ax = 0, ay = 0, az = 0;
    if (!qmi.getAccelerometer(ax, ay, az)) {
        return 0.0f;
    }

    // NOT PHYSICALLY VERIFIED — no hardware is attached in this environment,
    // so the axis/sign below could not be confirmed by tilting a real board
    // (the brief's Step 3 verification is genuinely impossible without
    // hardware; it depends on how the sensor die is oriented on this PCB).
    //
    // This uses the standard tilt-angle formula atan2(x, z), which is the
    // typical convention for "left/right" tilt about the axis perpendicular
    // to the display face, per Waveshare's example reading ax/ay/az in that
    // axis order. TiltDebouncer's contract requires negative = left,
    // positive = right (see TiltDebouncer.h) — once real hardware is
    // available, tilt the board left and right while watching
    // Serial.println(imuReadTiltDegrees()) and, if the sign or axis is
    // backwards, swap the axes or negate the result. That is expected to be
    // a one-line fix here, not a redesign.
    float angleRad = atan2(ax, az);
    float angleDeg = angleRad * 180.0f / PI;
    return angleDeg;
}
