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

#if ENABLE_IMU_TILT
#include <SensorQMI8658.hpp>
#endif

#if ENABLE_IMU_TILT

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
    Wire.begin(47, 48);
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

PanelOrientation imuReadOrientation() {
    if (!imuReady) return PanelOrientation::NORMAL;

    float ax = 0, ay = 0, az = 0;
    if (!qmi.getAccelerometer(ax, ay, az)) {
        return PanelOrientation::NORMAL;
    }

    constexpr float kMinDominantG = 0.5f;
    if (fabsf(ay) > fabsf(ax) && fabsf(ay) > kMinDominantG) {
        return (ay > 0) ? PanelOrientation::DND : PanelOrientation::BRB;
    }
    return PanelOrientation::NORMAL;
}

#else

bool imuBegin() {
    return false;
}

PanelOrientation imuReadOrientation() {
    return PanelOrientation::NORMAL;
}

#endif
