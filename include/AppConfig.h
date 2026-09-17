#pragma once

namespace AppConfig {

static constexpr const char* FW_VERSION = "0.1.1-m1";
static constexpr const char* API_VERSION = "v1";
static constexpr const char* DEVICE_PREFIX = "DINaudio";

static constexpr uint32_t SERIAL_BAUD = 115200;

// Wi-Fi
static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
static constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 15000;

// AP konfiguracyjny
static constexpr const char* AP_PASSWORD = ""; // otwarty AP na M1; docelowo rozważymy hasło
static constexpr uint8_t AP_CHANNEL = 6;

// WWW
static constexpr uint16_t HTTP_PORT = 80;

// Test audio
static constexpr uint32_t TEST_SAMPLE_RATE = 44100;
static constexpr float TEST_TONE_HZ = 440.0f;

} // namespace AppConfig
