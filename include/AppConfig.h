#pragma once

namespace AppConfig {

static constexpr const char* FW_VERSION = "0.2.1";
static constexpr const char* API_VERSION = "v1";
static constexpr const char* DEVICE_PREFIX = "DINaudio";

static constexpr uint32_t SERIAL_BAUD = 115200;

// Wi-Fi
static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
static constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 15000;

// AP
static constexpr const char* AP_PASSWORD = "";
static constexpr uint8_t AP_CHANNEL = 6;

// WWW
static constexpr uint16_t HTTP_PORT = 80;

// Audio
static constexpr uint32_t AUDIO_SAMPLE_RATE = 44100;

// Test audio legacy
static constexpr uint32_t TEST_SAMPLE_RATE = 44100;
static constexpr float TEST_TONE_HZ = 440.0f;

// Bluetooth
static constexpr uint32_t BT_STATE_POLL_MS = 100;

} // namespace AppConfig
