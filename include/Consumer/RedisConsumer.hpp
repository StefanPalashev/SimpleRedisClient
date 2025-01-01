#pragma once
#include "../common.hpp"
#include <atomic>
#include <memory>
#include <vector>

#include "IObservableConsumer.hpp"

// Forward declaration for Pimpl
// Pointers only need a forward declaration to compile.
class IMessageProcessor;

class RedisConsumer : public IObservableConsumer {
private:
  class MessageProcessorImpl;
  std::unique_ptr<MessageProcessorImpl> message_processor_impl_;

  void ReportError(const std::string &error_message) const {
    std::cerr << "[Consumer Id = " << id_ << "] " << error_message << std::endl;
  }

  void ProcessMessage(const std::string &message);

  void EstablishConnection(const std::string &redis_server_hostname,
                           unsigned short redis_server_port,
                           int &file_descriptor) const;
  [[nodiscard]] bool AddDataToStream(const std::string &resp_formatted_command,
                                     bool is_internal_call) const;

public:
  RedisConsumer(bool verbose_outputs);
  ~RedisConsumer();

  long long GetNumberOfProcessedMessages() const override {
    return number_of_processed_messages_;
  }

  void EstablishConnection(const std::string &redis_server_hostname,
                           unsigned short redis_server_port);

  void SubscribeToChannel(const std::string &channel_name,
                          const std::string &processing_stream = "");

  [[nodiscard]] bool
  AddDataToStream(const std::string &stream_name,
                  const std::vector<std::string> &values) const;

private:
  static int next_id_;
  int id_;
  bool verbose_outputs_;

  std::string redis_server_hostname_;
  unsigned short redis_server_port_;

  int subscription_socket_file_descriptor_;
  int processing_socket_file_descriptor_;

  bool initial_connection_established_;
  bool write_connection_established_;

  std::string subsciption_channel_;
  std::string processing_stream_;

  std::atomic<long long> number_of_processed_messages_;
  std::atomic<long long> number_of_processing_errors_;
};