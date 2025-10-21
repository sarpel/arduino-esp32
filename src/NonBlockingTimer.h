#ifndef NON_BLOCKING_TIMER_H
#define NON_BLOCKING_TIMER_H

#include <Arduino.h>

/**
 * @brief Non-blocking timer for periodic or one-shot events
 *
 * Provides millisecond-precision timing without blocking the main loop.
 * Supports both auto-reset (periodic) and one-shot modes, with convenient
 * methods for checking expiration and querying remaining time.
 *
 * Typical usage:
 * @code
 * NonBlockingTimer timer(1000, true);  // 1 second, auto-reset
 * timer.start();
 *
 * void loop() {
 *     if (timer.check()) {
 *         // Timer expired, auto-resets if enabled
 *         doPeriodicTask();
 *     }
 * }
 * @endcode
 *
 * @note Safe against millis() rollover (every ~49.7 days)
 */
class NonBlockingTimer
{
private:
    unsigned long previousMillis; ///< Timestamp of last start/reset
    unsigned long interval;       ///< Timer interval in milliseconds
    bool isRunning;               ///< Timer running state
    bool autoReset;               ///< Auto-reset on expiration flag

public:
    /**
     * @brief Construct a non-blocking timer
     * @param intervalMs Timer interval in milliseconds (default: 1000)
     * @param autoResetEnabled Auto-reset on expiration (default: true for periodic)
     */
    NonBlockingTimer(unsigned long intervalMs = 1000, bool autoResetEnabled = true)
        : previousMillis(0), interval(intervalMs), isRunning(false), autoReset(autoResetEnabled) {}

    /**
     * @brief Set the timer interval
     * @param intervalMs New interval in milliseconds
     * @note Does not affect currently running timer until next start/reset
     */
    void setInterval(unsigned long intervalMs)
    {
        interval = intervalMs;
    }

    /**
     * @brief Start the timer from current time
     * @note Resets internal timestamp to now
     */
    void start()
    {
        previousMillis = millis();
        isRunning = true;
    }

    /**
     * @brief Start timer in already-expired state for immediate first trigger
     * @note Useful for immediate execution followed by periodic intervals
     */
    void startExpired()
    {
        // Start timer in already-expired state for immediate first trigger
        previousMillis = millis() - interval - 1;
        isRunning = true;
    }

    /**
     * @brief Stop the timer
     * @note Timer can be restarted with start()
     */
    void stop()
    {
        isRunning = false;
    }

    /**
     * @brief Reset the timer to current time without stopping
     * @note Resets countdown to full interval
     */
    void reset()
    {
        previousMillis = millis();
    }

    /**
     * @brief Check if timer has expired and handle auto-reset
     * @return True if timer expired, false otherwise
     * @note If auto-reset enabled, timer automatically restarts
     * @note If auto-reset disabled, timer stops after first expiration
     */
    bool check()
    {
        if (!isRunning)
            return false;

        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= interval)
        {
            if (autoReset)
            {
                previousMillis = currentMillis;
            }
            else
            {
                isRunning = false;
            }
            return true;
        }
        return false;
    }

    /**
     * @brief Check if timer has expired without affecting state
     * @return True if expired, false otherwise
     * @note Unlike check(), does not auto-reset or stop timer
     */
    bool isExpired()
    {
        if (!isRunning)
            return false;
        return (millis() - previousMillis >= interval);
    }

    /**
     * @brief Get elapsed time since timer started/reset
     * @return Elapsed milliseconds
     */
    unsigned long getElapsed()
    {
        return millis() - previousMillis;
    }

    /**
     * @brief Get remaining time until expiration
     * @return Remaining milliseconds (0 if already expired)
     */
    unsigned long getRemaining()
    {
        unsigned long elapsed = getElapsed();
        return (elapsed >= interval) ? 0 : (interval - elapsed);
    }

    /**
     * @brief Check if timer is currently running
     * @return True if running, false if stopped
     */
    bool getIsRunning() const
    {
        return isRunning;
    }

    /**
     * @brief Get configured timer interval
     * @return Interval in milliseconds
     */
    unsigned long getInterval() const
    {
        return interval;
    }
};

#endif