#ifndef DEBUG_MODE_H
#define DEBUG_MODE_H

// Debug level control
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 1 // 0=OFF, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=VERBOSE
#endif

// Compile-time debug macros
#if DEBUG_LEVEL >= 1
#define DEBUG_ERROR(fmt, ...) LOG_ERROR(fmt, ##__VA_ARGS__)
#else
#define DEBUG_ERROR(fmt, ...)
#endif

#if DEBUG_LEVEL >= 2
#define DEBUG_WARN(fmt, ...) LOG_WARN(fmt, ##__VA_ARGS__)
#else
#define DEBUG_WARN(fmt, ...)
#endif

#if DEBUG_LEVEL >= 3
#define DEBUG_INFO(fmt, ...) LOG_INFO(fmt, ##__VA_ARGS__)
#else
#define DEBUG_INFO(fmt, ...)
#endif

#if DEBUG_LEVEL >= 4
#define DEBUG_LOG(fmt, ...) LOG_DEBUG(fmt, ##__VA_ARGS__)
#else
#define DEBUG_LOG(fmt, ...)
#endif

#if DEBUG_LEVEL >= 5
#define DEBUG_VERBOSE(fmt, ...) LOG_DEBUG(fmt, ##__VA_ARGS__)
#else
#define DEBUG_VERBOSE(fmt, ...)
#endif

// Runtime debug context - allows toggling debug output without recompiling
class RuntimeDebugContext {
public:
  static void setEnabled(bool enable);
  static bool isEnabled();
  static void setLevel(int level);
  static int getLevel();

  // Conditional logging based on runtime settings
  static void log(const char *fmt, ...);

private:
  static bool enabled;
  static int level;
};

#endif // DEBUG_MODE_H
