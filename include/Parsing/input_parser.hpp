#pragma once
#include <getopt.h>
#include <optional>
#include <string>
#include <tuple>

/*
A function to parse the input parameters.

Parsing the input parameters will return an optional with the following
possibilities:
  - std::nullopt in case of an error.
  - a tuple of bool, bool, bool, std::string and int when there is no error.
  The tuple values are:
    bool should_display_help - to display the help if the user has requested it
and exit gracefully.
    bool use_default_config - whether or not the default
configuration should be used.
    bool silent_mode - enters silent mode and makes the program's output less
verbose std::string config_file_path - the provided configuration file path. int
number_of_consumers - to override the number of consumers.
*/
[[nodiscard]] std::optional<std::tuple<bool, bool, bool, std::string, int>>
ParseInputParameters(int argc, char *argv[]) {
  bool use_default_config{false};
  bool silent_mode{false};
  std::string config_file_path{""};
  int number_of_consumers{0};

  static struct option long_options[] = {
      {"default", no_argument, nullptr, 'd'},
      {"number", required_argument, nullptr, 'n'},
      {"help", no_argument, nullptr, 'h'},
      {"silent", no_argument, nullptr, 's'},
      {nullptr, 0, nullptr, 0}};

  int opt;
  try {
    // Handle the long options
    while ((opt = getopt_long(argc, argv, "dn:hs", long_options, nullptr)) !=
           -1) {
      switch (opt) {
        case_break('d', use_default_config = true);
        case_break('s', silent_mode = true);
        case_break('n', number_of_consumers = std::stoi(optarg));
        case_break('h', return std::make_tuple(true, use_default_config,
                                               silent_mode, config_file_path,
                                               number_of_consumers));
      default:
        return {};
      }
    }
  } catch (std::invalid_argument &ex) {
    // In cases when the number argument is passed but the corresponding
    // argument value is not an integer.
    return {};
  }

  // Handle the FILE argument
  if (optind < argc) {
    config_file_path = argv[optind];
  } else {
    config_file_path = "-";
  }

  return std::make_tuple(false, use_default_config, silent_mode,
                         config_file_path, number_of_consumers);
}