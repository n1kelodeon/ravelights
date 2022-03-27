#pragma once

#include "ESPAsyncWebServer.h"
#include "Pattern.hpp"
#include <FastLED.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

template <unsigned LED_ROW_COUNT, unsigned LED_COLUMN_COUNT, std::uint8_t LED_DATA_PIN = 4,
          EOrder RGB_ORDER = EOrder::RGB>
class RaveLights {

    struct PatternConfig {
        uint8_t brightness{255};
        unsigned color{CRGB::Red};
        unsigned patternIndex{8};
    };

   public:
    RaveLights() : leds_(LED_ROW_COUNT * LED_COLUMN_COUNT, CRGB::Black), server_(80) {
        //        FastLED.addLeds<WS2812, LED_DATA_PIN, RGB_ORDER>(leds_.data(), LED_ROW_COUNT * LED_COLUMN_COUNT);
        const int PIN_COUNT = 4;
        const int PIXELS_PER_PIN = 144;
        constexpr std::array<unsigned, PIN_COUNT> PINS = {19, 18, 22, 21};
        FastLED.addLeds<WS2812, PINS[0], RGB_ORDER>(leds_.data(), 0 * PIXELS_PER_PIN, PIXELS_PER_PIN);
        FastLED.addLeds<WS2812, PINS[1], RGB_ORDER>(leds_.data(), 1 * PIXELS_PER_PIN,
                                                    PIXELS_PER_PIN);  // this is the bad
        FastLED.addLeds<WS2812, PINS[2], RGB_ORDER>(leds_.data(), 2 * PIXELS_PER_PIN, PIXELS_PER_PIN);
        FastLED.addLeds<WS2812, PINS[3], RGB_ORDER>(leds_.data(), 3 * PIXELS_PER_PIN, PIXELS_PER_PIN);

        delay(100);
        setupRequestHandlers();
    }

    void testLeds() {
        FastLED.setBrightness(127);
        std::vector<CRGB> colors{CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::Black};
        for (const auto color : colors) {
            FastLED.showColor(color);
            delay(500);
        }
    }

    void addPattern(std::shared_ptr<Pattern::AbstractPattern> pattern) {
        pattern->init(LED_ROW_COUNT, LED_COLUMN_COUNT);
        patterns_.push_back(pattern);
    }

    void startWebServer() { server_.begin(); }

    void show() {
        while (!stopShowLoop_) {
            FastLED.clear(true);
            unsigned long offDurationMs =
                patterns_[currentPatternConfig_.patternIndex]->perform(leds_, currentPatternConfig_.color);
            unsigned long currentTimeMs = millis();
            do {
                {  // Begin of scope guarded by mutex
                    // Leave waiting loop prematurely if pattern change is requested by asynchronous web server thread
                    std::lock_guard<std::mutex> lockGuard(isPatternUpdatePendingMutex_);
                    if (isPatternUpdatePending_) {
                        currentPatternConfig_.patternIndex = nextPatternConfig_.patternIndex;
                        isPatternUpdatePending_ = false;
                        break;
                    }
                }  // End of scope guarded by mutex
            } while (millis() - currentTimeMs < offDurationMs);
            updatePatternConfig();
        }
    }

    void startShowLoop() { showLoopThread_ = std::thread(&RaveLights::show, this); }

    void stopShowLoop() {
        stopShowLoop_ = true;
        showLoopThread_.join();  // wait for thread to finish
        stopShowLoop_ = false;
    }

   private:
    std::vector<CRGB> leds_;
    std::vector<std::shared_ptr<Pattern::AbstractPattern>> patterns_;
    bool isPatternUpdatePending_{false};
    std::mutex isPatternUpdatePendingMutex_;
    AsyncWebServer server_;
    struct PatternConfig currentPatternConfig_;
    struct PatternConfig nextPatternConfig_;
    std::atomic_bool stopShowLoop_{false};
    std::thread showLoopThread_;

    void updatePatternConfig() {
        currentPatternConfig_ = nextPatternConfig_;
        FastLED.setBrightness(currentPatternConfig_.brightness);
    }

    void setupRequestHandlers() {
        server_.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send_P(200, "text/html", "Documentation in process...");
        });
        setupPatternRequestHandler();
        setupBrightnessRequestHandler();
        setupColorRequestHandler();
    }

    void setupPatternRequestHandler() {
        server_.on("/pattern", HTTP_GET, [this](AsyncWebServerRequest *request) {
            bool hasError = false;
            int patternIndex = 0;
            if (request->hasParam("value")) {
                patternIndex = request->getParam("value")->value().toInt();
                if (patternIndex < 0 || patternIndex >= patterns_.size()) {
                    hasError = true;
                }
            } else {
                hasError = true;
            }
            if (hasError) {
                request->send(200, "text/plain", "Error. Could not update pattern to #" + String(patternIndex));
            } else {
                nextPatternConfig_.patternIndex = patternIndex;
                request->send(200, "text/plain", "OK. Pattern Updated to #" + String(patternIndex));
                {
                    std::lock_guard<std::mutex> lockGuard(isPatternUpdatePendingMutex_);
                    isPatternUpdatePending_ = true;
                }
            }
        });
    }

    void setupBrightnessRequestHandler() {
        server_.on("/brightness", HTTP_GET, [this](AsyncWebServerRequest *request) {
            bool hasError = false;
            int brightness = 0;
            if (request->hasParam("value")) {
                brightness = request->getParam("value")->value().toInt();
                if (brightness < 0 || brightness > 255) {
                    hasError = true;
                }
            } else {
                hasError = true;
            }
            if (hasError) {
                request->send(200, "text/plain", "Error. Could not update brightness to " + String(brightness));
            } else {
                nextPatternConfig_.brightness = brightness;
                request->send(200, "text/plain", "OK. Brightness Updated to " + String(brightness));
            }
        });
    }

    void setupColorRequestHandler() {
        server_.on("/color", HTTP_GET, [this](AsyncWebServerRequest *request) {
            bool hasError = false;
            int color = 0;
            if (request->hasParam("value")) {
                String colorString = request->getParam("value")->value();
                color = strtol(colorString.c_str(), NULL, 16);
                if (color < 0 || color > 0xffffff) {
                    hasError = true;
                }
            } else {
                hasError = true;
            }
            if (hasError) {
                request->send(200, "text/plain", "Error. Could not update color to " + String(color));
            } else {
                nextPatternConfig_.color = color;
                request->send(200, "text/plain", "OK. Color Updated to 0x" + String(color));
            }
        });
    }
};
