#ifndef SERIAL_COMMAND_H
#define SERIAL_COMMAND_H

#include <Arduino.h>

// Forward declarations
class NetworkManager;

// Serial command handler for runtime control
class SerialCommandHandler {
public:
  static void initialize();
  static void processCommands();

private:
  static const size_t BUFFER_SIZE = 128;
  static char command_buffer[BUFFER_SIZE];
  static size_t buffer_index;

  // Command handlers
  static void handleStatusCommand();
  static void handleConfigCommand(const char *args);
  static void handleRestartCommand();
  static void handleDisconnectCommand();
  static void handleConnectCommand();
  static void handleStatsCommand();
  static void handleHealthCommand();
  static void handleHelpCommand();

  // Utility functions
  static void printStatus();
  static void printHealth();
  static void clearBuffer();
  static char *getNextToken(char *str, const char *delim);
};

#endif // SERIAL_COMMAND_H
