#include "debug_mode.h"
#include "logger.h"

// Static member initialization
bool RuntimeDebugContext::enabled = false;
int RuntimeDebugContext::level = 0;

void RuntimeDebugContext::setEnabled(bool enable) {
    enabled = enable;
    if (enable) {
        LOG_INFO("Runtime debug output enabled");
    } else {
        LOG_INFO("Runtime debug output disabled");
    }
}

bool RuntimeDebugContext::isEnabled() {
    return enabled;
}

void RuntimeDebugContext::setLevel(int new_level) {
    level = new_level;
    LOG_INFO("Runtime debug level set to %d", level);
}

int RuntimeDebugContext::getLevel() {
    return level;
}

void RuntimeDebugContext::log(const char* fmt, ...) {
    if (!enabled) return;

    va_list args;
    va_start(args, fmt);

    // Simple implementation - just print to serial if enabled
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    Serial.println(buffer);

    va_end(args);
}
