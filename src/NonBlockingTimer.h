#ifndef NON_BLOCKING_TIMER_H
#define NON_BLOCKING_TIMER_H

#include <Arduino.h>

class NonBlockingTimer {
private:
    unsigned long previousMillis;
    unsigned long interval;
    bool isRunning;
    bool autoReset;

public:
    NonBlockingTimer(unsigned long intervalMs = 1000, bool autoResetEnabled = true) 
        : previousMillis(0), interval(intervalMs), isRunning(false), autoReset(autoResetEnabled) {}

    void setInterval(unsigned long intervalMs) {
        interval = intervalMs;
    }

    void start() {
        previousMillis = millis();
        isRunning = true;
    }

    void stop() {
        isRunning = false;
    }

    void reset() {
        previousMillis = millis();
    }

    bool check() {
        if (!isRunning) return false;
        
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= interval) {
            if (autoReset) {
                previousMillis = currentMillis;
            } else {
                isRunning = false;
            }
            return true;
        }
        return false;
    }

    bool isExpired() {
        if (!isRunning) return false;
        return (millis() - previousMillis >= interval);
    }

    unsigned long getElapsed() {
        return millis() - previousMillis;
    }

    unsigned long getRemaining() {
        unsigned long elapsed = getElapsed();
        return (elapsed >= interval) ? 0 : (interval - elapsed);
    }

    bool getIsRunning() const {
        return isRunning;
    }

    unsigned long getInterval() const {
        return interval;
    }
};

#endif