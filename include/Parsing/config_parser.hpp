#pragma once
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "../common.hpp"

[[nodiscard]] std::unordered_map<std::string, std::string>
CreateDefaultConfiguration() {
  std::unordered_map<std::string, std::string> config;
  // Redis server ip and port
  config[CFG_KEY_HOST] = REDIS_SERVER_HOSTNAME;
  config[CFG_KEY_PORT] = REDIS_SERVER_PORT;
  // The group size
  config[CFG_KEY_GROUP_SIZE] = "1";
  // Channels / Streams
  config[CFG_KEY_SUB_CHANNEL] = "messages:published";
  config[CFG_KEY_PROC_STREAM] = "messages:processed";
  // Monitoring interval in seconds
  config[CFG_KEY_MONITORING_INTERVAL] = "3";

  std::cout << "Created a default configuration." << std::endl;
  return config;
}

[[nodiscard]] std::unordered_map<std::string, std::string>
CreateCustomConfiguration(int number_of_consumers = 0) {
  std::cout << "No configuration was provided! Please, create one using the "
               "standard input..."
            << std::endl;
  std::unordered_map<std::string, std::string> config;
  std::string buffer{};
  // Redis server ip and port
  std::cout << "Enter the host's IPv4 address (.1 for localhost): ";
  std::cin >> buffer;
  config[CFG_KEY_HOST] = buffer == ".1" ? "127.0.0.1" : buffer;
  std::cout << "Enter the host's port: (0 for Redis' default port - 6379): ";
  std::cin >> buffer;
  config[CFG_KEY_PORT] = buffer == "0" ? "6379" : buffer;
  // The group size
  if (number_of_consumers) {
    std::cout << "The number of consumers has already been configured to "
              << number_of_consumers
              << " by a command line argument. Skipping..." << std::endl;
    config[CFG_KEY_GROUP_SIZE] = std::to_string(number_of_consumers);
  } else {
    std::cout << "Enter the number of consumers: ";
    std::cin >> buffer;
    config[CFG_KEY_GROUP_SIZE] = buffer;
  }
  // Channels / Streams
  std::cout << "Enter the default channel to subscribe to: ";
  std::cin >> buffer;
  config[CFG_KEY_SUB_CHANNEL] = buffer;
  std::cout << "Enter the default stream where processed messages will be "
               "published: ";
  std::cin >> buffer;
  config[CFG_KEY_PROC_STREAM] = buffer;
  // Monitoring interval in seconds
  std::cout << "Enter a monitoring interval in seconds. It will be used to "
               "display information about the processed messages: ";
  std::cin >> buffer;
  config[CFG_KEY_MONITORING_INTERVAL] = buffer;

  std::cout << std::endl
            << "Created a custom configuration using the standard input!"
            << std::endl;
  return config;
}

[[nodiscard]] bool ParseConfigurationFromFile(
    const std::string &filename,
    std::unordered_map<std::string, std::string> &config) {
  std::ifstream file(filename);

  if (!file.is_open()) {
    std::cerr << "Error opening file: " << filename << std::endl;
    return false;
  }

  std::string current_line;
  while (std::getline(file, current_line)) {
    // Skip empty lines and comments
    if (current_line.empty() || current_line[0] == '#' ||
        current_line[0] == ';') {
      continue;
    }

    std::size_t equals_position = current_line.find('=');
    if (equals_position != std::string::npos) {
      std::string key = current_line.substr(0, equals_position);
      std::string value = current_line.substr(equals_position + 1);

      // Trim whitespaces
      key.erase(std::remove_if(key.begin(), key.end(), ::isspace), key.end());
      value.erase(std::remove_if(value.begin(), value.end(), ::isspace),
                  value.end());

      config[key] = value;
    }
  }

  file.close();

  return true;
}

[[nodiscard]] bool ValidateConfiguration(
    const std::unordered_map<std::string, std::string> &config) {
  std::unordered_set<std::string> required_keys = {
      CFG_KEY_HOST,        CFG_KEY_PORT,        CFG_KEY_GROUP_SIZE,
      CFG_KEY_SUB_CHANNEL, CFG_KEY_PROC_STREAM, CFG_KEY_MONITORING_INTERVAL};

  std::string missing_keys{};

  for (const std::string &key : required_keys) {
    if (config.find(key) == config.end()) {
      missing_keys += key + ", ";
    }
  }

  if (!missing_keys.empty()) {
    // remove the last ", "
    missing_keys.pop_back();
    missing_keys.pop_back();
    std::cout << "Missing mandatory key(s): " << missing_keys << std::endl;
    return false;
  }

  // A simple validity check for all of the integer config parameters.
  bool all_numeric_values_are_valid = true;
  try {
    for (const std::string &parameter :
         {CFG_KEY_PORT, CFG_KEY_GROUP_SIZE, CFG_KEY_MONITORING_INTERVAL}) {
      std::stringstream ss(config.at(parameter));
      unsigned short buffer{0};
      ss >> buffer;

      if (ss.fail() || (config.at(parameter) != std::to_string(buffer))) {
        std::cerr << " The value of parameter " << parameter
                  << " is invalid. Value (" << buffer << ")" << std::endl;
        all_numeric_values_are_valid = false;
      }
    }

    return all_numeric_values_are_valid;
  } catch (...) {
    return false;
  }
}