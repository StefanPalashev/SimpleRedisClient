#pragma once
#include "../../common.hpp"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "../IObservableConsumer.hpp"
// Forward declaration for Pimpl
// Pointers only need a forward declaration to compile.
class IMessageProcessor;

class RedisBrokerConsumer : public IObservableConsumer {
private:
  void ReportError(const std::string &error_message) const {
    std::cerr << "[RedisBrokerConsumer] " << error_message << std::endl;
  }

  void ProcessMessage(const std::string &message);

  void EstablishConnection(const std::string &redis_server_hostname,
                           unsigned short redis_server_port,
                           int &file_descriptor) const;

public:
  RedisBrokerConsumer(bool verbose_outputs, int number_of_workers);
  ~RedisBrokerConsumer();

  void EstablishConnection(const std::string &redis_server_hostname,
                           unsigned short redis_server_port);

  void SubscribeToChannel(const std::string &channel_name,
                          const std::string &processing_stream = "");

  long long GetNumberOfProcessedMessages() const override;

private:
  bool verbose_outputs_;
  int number_of_workers_;

  std::string redis_server_hostname_;
  unsigned short redis_server_port_;

  int subscription_socket_file_descriptor_;
  bool initial_connection_established_;

  std::string subsciption_channel_;

  class MessageProcessorImpl;
  std::shared_ptr<MessageProcessorImpl> message_processor_impl_;
  std::queue<std::string> message_queue_;

  class BrokerWorker;
  std::vector<std::unique_ptr<BrokerWorker>> workers_;

  std::mutex queue_mutex_;
  std::condition_variable cv_;
};