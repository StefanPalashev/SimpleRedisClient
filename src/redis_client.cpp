#include <assert.h>
#include <hiredis/hiredis.h>
#include <optional>
#include <thread>
#include <unordered_map>

#include "../include/Consumer/ConsumerGroups/RedisBrokerConsumer.hpp"
#include "../include/Consumer/RedisConsumer.hpp"

#include "../include/Parsing/config_parser.hpp"
#include "../include/Parsing/input_parser.hpp"
#include "../include/common.hpp"

#include "../include/Monitoring/ProcessedMessagesMonitor.hpp"

using namespace std;

void PrintHelp() {
  println("simple_redis_client");
  {
    println("NAME"
            "\n\tsimple_redis_client - acts as a consumer from a Redis Server");
    println("SYNOPSIS:"
            "\n\tsimple_redis_client [OPTION]... [FILE]...");
    println("DESCRIPTION:"
            "\n\tFILE is the path to the configuration."
            "\n\tWith no FILE, or when FILE is -, read standard input.\n"
            "\n\t-d, --default\tuse a default configuration"
            "\n\t-n, --number\tnumber of consumers"
            "\n\t-s, --silent\treduce the program's output"
            "\n\t-h, --help\tdisplay this help and exit");
    println("EXAMPLES:"
            "\n\tpath/to/simple_redis_client "
            "path/to/custom_config.cfg\tProvide a custom configuration."
            "\n\tpath/to/simple_redis_client --default\tUse the default "
            "configuration."
            "\n\tpath/to/simple_redis_client --n\tOverride the number of "
            "consumers.");
  }
}

int main(int argc, char *argv[]) {
  std::unordered_map<std::string, std::string> config;
  bool verbose_outputs = true;

  auto print_config = [&](std::unordered_map<std::string, std::string> cfg) {
    std::string config_delimiter = std::string(50, '=');
    std::cout << std::endl << "Printing the configuration:" << std::endl;
    std::cout << config_delimiter << std::endl;
    for (const pair<std::string, std::string> &cfg_pair : cfg) {
      std::cout << cfg_pair.first << "=" << cfg_pair.second << std::endl;
    }
    std::cout << config_delimiter << std::endl << std::endl;
  };

  auto input_parameters_opt = ParseInputParameters(argc, argv);

  if (input_parameters_opt) {
    auto [should_display_help, use_default_config, silent_mode,
          config_file_path, number_of_consumers] = input_parameters_opt.value();
    if (should_display_help) {
      PrintHelp();
      return EXIT_SUCCESS;
    } else {
      if (use_default_config) {
        config = CreateDefaultConfiguration();
      } else {
        if (config_file_path == "-") {
          config = CreateCustomConfiguration(number_of_consumers);
          if (ValidateConfiguration(config)) {
            std::cout << "The configuration is valid! Proceeding..."
                      << std::endl;
          } else {
            std::cout << "The created configuration is invalid as some of "
                         "the required keys have incorrect values."
                      << std::endl
                      << "A default configuration will be created now, which "
                         "can be used as a template for a custom configuration."
                      << std::endl;
            config = CreateDefaultConfiguration();
          }
        } else {
          if (ParseConfigurationFromFile(config_file_path, config)) {
            std::cout << "Successfully parsed a configuration from: "
                      << config_file_path << std::endl
                      << "Validating the configuration...";
            if (ValidateConfiguration(config)) {
              std::cout << "The configuration is valid! Proceeding..."
                        << std::endl;
            } else {
              std::cout
                  << "The provided configuration is invalid as some of "
                     "the required keys are either missing or have incorrect "
                     "values."
                  << std::endl
                  << "A default configuration will be created now, which "
                     "can be used as a template for a custom configuration."
                  << std::endl;
              config = CreateDefaultConfiguration();
            }
          } else {
            std::cout << "Falling back to the default configuration..."
                      << std::endl;
            config = CreateDefaultConfiguration();
          }
        }
      }

      if (number_of_consumers > 0 &&
          (use_default_config || config_file_path != "-")) {
        std::cout << "Overriding the number of consumers to "
                  << number_of_consumers << "." << std::endl;
        config[CFG_KEY_GROUP_SIZE] = std::to_string(number_of_consumers);
      }

      if (silent_mode) {
        verbose_outputs = false;
        std::cout << "Entering silent mode! From now on the program's output "
                     "will be less verbose."
                  << std::endl;
      }

      print_config(config);
    }
  } else {
    PrintHelp();
    return EXIT_FAILURE;
  }

  // When the group size is 1, use the RedisConsumer class, which will subscribe
  // and process the messages itself
  if (atoi(config[CFG_KEY_GROUP_SIZE].c_str()) == 1) {
    RedisConsumer redis_consumer(verbose_outputs);
    redis_consumer.EstablishConnection(config[CFG_KEY_HOST],
                                       atoi(config[CFG_KEY_PORT].c_str()));
    // Subscribe without posting the processed messages to a stream
    // redis_consumer.SubscribeToChannel(config[CFG_KEY_SUB_CHANNEL]);

    // Subscribe and post the processed messages to a stream
    // redis_consumer.SubscribeToChannel(config[CFG_KEY_SUB_CHANNEL],
    //                                   config[CFG_KEY_PROC_STREAM]);

    // Testing call for the XADD API
    // bool successful_addition = redis_consumer.AddDataToStream(
    //     "testing_stream", {"John", "Smith", "Jane", "Smith"});

    std::thread subscription_thread([&redis_consumer, &config]() {
      redis_consumer.SubscribeToChannel(config[CFG_KEY_SUB_CHANNEL],
                                        config[CFG_KEY_PROC_STREAM]);
    });

    std::vector<IObservableConsumer *> consumers = {&redis_consumer};
    ProcessedMessagesMonitor processed_messages_monitor(
        consumers, atoi(config[CFG_KEY_MONITORING_INTERVAL].c_str()));
    std::thread monitoring_thread(&ProcessedMessagesMonitor::StartMonitoring,
                                  &processed_messages_monitor);

    subscription_thread.join();
    monitoring_thread.join();
  } else {
    RedisBrokerConsumer redis_broker_consumer(
        verbose_outputs, atoi(config[CFG_KEY_GROUP_SIZE].c_str()));
    redis_broker_consumer.EstablishConnection(
        config[CFG_KEY_HOST], atoi(config[CFG_KEY_PORT].c_str()));

    std::thread subscription_thread([&redis_broker_consumer, &config]() {
      redis_broker_consumer.SubscribeToChannel(config[CFG_KEY_SUB_CHANNEL],
                                               config[CFG_KEY_PROC_STREAM]);
    });

    std::vector<IObservableConsumer *> consumers = {&redis_broker_consumer};
    ProcessedMessagesMonitor processed_messages_monitor(
        consumers, atoi(config[CFG_KEY_MONITORING_INTERVAL].c_str()));
    std::thread monitoring_thread(&ProcessedMessagesMonitor::StartMonitoring,
                                  &processed_messages_monitor);

    subscription_thread.join();
    monitoring_thread.join();
  }

  return EXIT_SUCCESS;
}