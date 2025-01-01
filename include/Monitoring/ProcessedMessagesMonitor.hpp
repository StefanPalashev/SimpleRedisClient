#pragma once
#include "../Consumer/IObservableConsumer.hpp"
#include <chrono>
#include <thread>

class ProcessedMessagesMonitor {
public:
  ProcessedMessagesMonitor(
      std::vector<IObservableConsumer *> &redis_observable_consumers,
      unsigned int report_interval_in_seconds)
      : redis_observable_consumers_(redis_observable_consumers),
        report_interval_in_seconds_(report_interval_in_seconds) {}

  void StartMonitoring() {
    using namespace std::chrono;
    if (report_interval_in_seconds_ == 0 ||
        redis_observable_consumers_.size() == 0) {
      std::cout << "Disabling the monitoring of processed messages!"
                << std::endl;
      return;
    }

    long long last_reported_count = 0;
    steady_clock::time_point last_report_time = steady_clock::now();

    while (true) {
      std::this_thread::sleep_for(seconds(1));

      long long messages_processed_this_second = 0;
      for (auto &consumer : redis_observable_consumers_) {
        messages_processed_this_second +=
            consumer->GetNumberOfProcessedMessages();
      }

      double messages_per_second =
          (messages_processed_this_second - last_reported_count) /
          static_cast<double>(report_interval_in_seconds_);

      auto seconds_since_last_report =
          duration_cast<seconds>(steady_clock::now() - last_report_time)
              .count();

      if (seconds_since_last_report >= report_interval_in_seconds_) {
        std::time_t now_time_t = system_clock::to_time_t(system_clock::now());
        std::cout << "Current report time: " << std::ctime(&now_time_t)
                  << "Messages processed per second in last "
                  << report_interval_in_seconds_
                  << " seconds: " << messages_per_second << " messages/sec"
                  << std::endl;

        last_reported_count = messages_processed_this_second;
        last_report_time = steady_clock::now();
      }
    }
  }

private:
  std::vector<IObservableConsumer *> redis_observable_consumers_;
  unsigned int report_interval_in_seconds_;
};