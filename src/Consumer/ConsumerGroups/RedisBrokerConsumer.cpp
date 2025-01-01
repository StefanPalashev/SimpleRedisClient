#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h>
#include <optional>
#include <sys/socket.h>
#include <unistd.h>

#include <hiredis/hiredis.h>

#include <sstream>

#include "../../../include/Consumer/ConsumerGroups/RedisBrokerConsumer.hpp"
#include "../../../include/Consumer/JsonMessageProcessorImpl.hpp"
#include "../../../include/Consumer/RedisConsumerUtils/redis_consumer_utils.hpp"

class RedisBrokerConsumer::MessageProcessorImpl {
public:
  MessageProcessorImpl()
      : message_processor_(std::make_shared<JsonMessageProcessorImpl>()) {}

  std::optional<Message> ProcessMessage(const std::string &message) {
    return message_processor_->ProcessMessage(message);
  }

private:
  std::shared_ptr<IMessageProcessor> message_processor_;
};

class RedisBrokerConsumer::BrokerWorker {
public:
  BrokerWorker(std::shared_ptr<MessageProcessorImpl> message_processor_impl,
               std::queue<std::string> &message_queue, std::mutex &queue_mutex,
               std::condition_variable &cv,
               const std::string &source_channel_name,
               const std::string &processing_stream_name, bool verbose_outputs)
      : id_{next_id_++}, message_processor_impl_(message_processor_impl),
        message_queue_(message_queue), queue_mutex_(queue_mutex),
        cv_(cv), source_channel_name_{source_channel_name},
        processing_stream_name_{processing_stream_name},
        verbose_outputs_{verbose_outputs},
        stop_(false), writing_socket_file_descriptor_{-1},
        number_of_processed_messages_{0}, number_of_processing_errors_{0} {
    worker_identifier_ = "[Broker Worker " + std::to_string(id_) + "]";
    if (verbose_outputs_) {
      if (!processing_stream_name_.empty()) {
        std::cout << worker_identifier_
                  << " Processing stream set to: " << processing_stream_name_
                  << std::endl;
      } else {
        std::cout << worker_identifier_ << " No processing stream set!"
                  << std::endl;
      }
    }
  }

  void Start() { thread_ = std::thread(&BrokerWorker::ProcessMessages, this); }

  void Stop() {
    stop_ = true;
    if (writing_socket_file_descriptor_ != -1) {
      close(writing_socket_file_descriptor_);
    }
    cv_.notify_all();
    thread_.join();
  }

  void SetWritingSocketFileDescriptor(int writing_socket_file_descriptor) {
    writing_socket_file_descriptor_ = writing_socket_file_descriptor;
  }

  void ReportError(const std::string &error_message) const {
    std::cerr << worker_identifier_ << " " << error_message << std::endl;
  }

  [[nodiscard]] bool
  AddDataToStream(const std::string &resp_formatted_command) const {
    ssize_t bytes_sent =
        send(writing_socket_file_descriptor_, resp_formatted_command.c_str(),
             resp_formatted_command.size(), 0);
    if (bytes_sent < 0) {
      ReportError("Failed to send the xadd command!");
      return false;
    }

    redisReader *reader = redisReaderCreate();
    if (reader == nullptr) {
      ReportError("Failed to create a Redis reader!");
      return false;
    }

    char buffer[1024] = {};
    ssize_t bytes_read =
        recv(writing_socket_file_descriptor_, buffer, sizeof(buffer), 0);
    if (bytes_read < 0) {
      ReportError("Failed to read from the server!");
      redisReaderFree(reader);
      return false;
    }

    if (redisReaderFeed(reader, buffer, bytes_read) != REDIS_OK) {
      ReportError("Failed to feed the Redis reader!");
      redisReaderFree(reader);
      return false;
    }

    void *reply = nullptr;
    int status = redisReaderGetReply(reader, &reply);
    if (status != REDIS_OK) {
      ReportError("Failed to get a Redis reply!");
      redisReaderFree(reader);
      return false;
    }

    if (redisReply *r = (redisReply *)reply) {
      if (r->type == REDIS_REPLY_STRING) {
        if (verbose_outputs_) {
          std::cout << "Successfully wrote the data to Stream with id = "
                    << r->str << std::endl;
        }
        redisReaderFree(reader);
        return true;
      } else {
        ReportError("Unexpected response type");
        redisReaderFree(reader);
        return false;
      }
    } else {
      redisReaderFree(reader);
      return false;
    }
    return true;
  }

  void ProcessMessages() {
    std::cout << worker_identifier_ << " ready!" << std::endl;
    while (!stop_) {
      std::string message;
      // Consume a message from the shared queue
      {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        cv_.wait(lock, [this] { return !message_queue_.empty() || stop_; });

        if (stop_ && message_queue_.empty()) {
          break;
        }

        message = message_queue_.front();
        message_queue_.pop();
      }

      auto processed_message_opt =
          message_processor_impl_->ProcessMessage(message);
      if (processed_message_opt) {
        auto processed_message = processed_message_opt.value();
        processed_message.processor_id = id_;
        processed_message.processing_date_time = GetCurrentTime();
        processed_message.source_channel_name = source_channel_name_;

        std::cout << "Post processing of message with id = ("
                  << processed_message.message_id << ")." << std::endl
                  << "Processed by " << worker_identifier_ << " at "
                  << processed_message.processing_date_time
                  << ", received from channel ("
                  << processed_message.source_channel_name << ")." << std::endl;

        if (!processing_stream_name_.empty()) {
          if (AddDataToStream(CreateWriteMessageToStreamCommand(
                  processing_stream_name_, processed_message))) {
            if (verbose_outputs_) {
              std::cout << worker_identifier_
                        << " Successfully added the message to the stream for "
                           "processed messages - "
                        << processing_stream_name_ << std::endl;
            }
            number_of_processed_messages_++;
          } else {
            number_of_processing_errors_++;
          }
        } else {
          number_of_processed_messages_++;
        }
      } else {
        number_of_processing_errors_++;
      }

      std::cout << worker_identifier_ << " Messages processed so far: "
                << number_of_processed_messages_ << std::endl;

      if (number_of_processing_errors_) {
        std::cout << worker_identifier_
                  << " Number of encountered processing errors: "
                  << number_of_processing_errors_ << std::endl;
      }
    }
  }

  long long GetNumberOfProcessedMessages() const {
    return number_of_processed_messages_;
  }

private:
  static int next_id_;
  int id_;
  std::string worker_identifier_;

  std::shared_ptr<MessageProcessorImpl> message_processor_impl_;
  std::queue<std::string> &message_queue_;
  std::mutex &queue_mutex_;
  std::condition_variable &cv_;
  std::thread thread_;

  std::string source_channel_name_;
  std::string processing_stream_name_;

  int writing_socket_file_descriptor_;

  bool verbose_outputs_;

  bool stop_;
  std::atomic<long long> number_of_processed_messages_;
  std::atomic<long long> number_of_processing_errors_;
};

int RedisBrokerConsumer::BrokerWorker::next_id_ = 1;

RedisBrokerConsumer::RedisBrokerConsumer(bool verbose_outputs,
                                         int number_of_workers)
    : redis_server_hostname_{}, redis_server_port_{0},
      subscription_socket_file_descriptor_{-1},
      initial_connection_established_{false}, subsciption_channel_{},
      message_processor_impl_(std::make_shared<MessageProcessorImpl>()),
      verbose_outputs_{verbose_outputs}, number_of_workers_{number_of_workers} {
  if (number_of_workers_ < 1) {
    number_of_workers = 1;
  }
}

RedisBrokerConsumer::~RedisBrokerConsumer() {
  for (auto &worker : workers_) {
    worker->Stop();
  }
}

void RedisBrokerConsumer::EstablishConnection(
    const std::string &redis_server_hostname, unsigned short redis_server_port,
    int &file_descriptor) const {
  file_descriptor = socket(AF_INET, SOCK_STREAM, 0);
  if (file_descriptor < 0) {
    ReportError("Failed to create a socket!");
    exit(EXIT_FAILURE);
  }
  sockaddr_in server_address;
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = inet_addr(redis_server_hostname.c_str());
  server_address.sin_port = htons(redis_server_port);

  if (connect(file_descriptor, (struct sockaddr *)&server_address,
              sizeof(server_address)) < 0) {
    ReportError("Unable to connect to a Redis server!");
    close(file_descriptor);
    exit(EXIT_FAILURE);
  }
}

void RedisBrokerConsumer::EstablishConnection(
    const std::string &redis_server_hostname,
    unsigned short redis_server_port) {
  EstablishConnection(redis_server_hostname, redis_server_port,
                      subscription_socket_file_descriptor_);
  redis_server_hostname_ = redis_server_hostname;
  redis_server_port_ = redis_server_port;

  initial_connection_established_ = true;
  std::cout << "[RedisBrokerConsumer] Connected to Redis server!" << std::endl;
}

void RedisBrokerConsumer::ProcessMessage(const std::string &message) {
  // Round-robin message distribution to the broker's workers
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    message_queue_.push(message);
  }
  cv_.notify_one();
}

void RedisBrokerConsumer::SubscribeToChannel(
    const std::string &channel_name, const std::string &processing_stream) {
  if (!initial_connection_established_) {
    ReportError("Not connected to a Redis server! "
                "Please, make sure that there is a running Redis server and "
                "connect to it.");
    exit(EXIT_FAILURE);
  }

  const std::string redis_channel_subscription_command =
      CreateSubscriptionCommand(channel_name);

  ssize_t bytes_sent = send(subscription_socket_file_descriptor_,
                            redis_channel_subscription_command.c_str(),
                            redis_channel_subscription_command.size(), 0);
  if (bytes_sent < 0) {
    ReportError("Failed to send the subscription command!");
    close(subscription_socket_file_descriptor_);
    exit(EXIT_FAILURE);
  }

  subsciption_channel_ = channel_name;
  // If the subscription was successful, create the workers.
  for (int i = 0; i < number_of_workers_; ++i) {
    workers_.emplace_back(std::make_unique<BrokerWorker>(
        message_processor_impl_, message_queue_, queue_mutex_, cv_,
        subsciption_channel_, processing_stream, verbose_outputs_));
    // If there's a processing stream, the broker consumer will try to establish
    // a connection to the Redis server and assign the socket to the worker. The
    // worker's socket will be used to write to the processing stream.
    if (!processing_stream.empty()) {
      int current_worker_socket_file_descriptor = -1;
      EstablishConnection(redis_server_hostname_, redis_server_port_,
                          current_worker_socket_file_descriptor);
      workers_.back()->SetWritingSocketFileDescriptor(
          current_worker_socket_file_descriptor);
    }
    workers_.back()->Start();
  }

  redisReader *reader = redisReaderCreate();
  if (reader == nullptr) {
    ReportError("Failed to create a Redis reader!");
    close(subscription_socket_file_descriptor_);
    exit(EXIT_FAILURE);
  }

  char buffer[64] = {};
  while (true) {
    ssize_t bytes_read =
        recv(subscription_socket_file_descriptor_, buffer, sizeof(buffer), 0);
    if (bytes_read < 0) {
      ReportError("Failed to read from the server!");
      break;
    }

    if (redisReaderFeed(reader, buffer, bytes_read) != REDIS_OK) {
      ReportError("Failed to feed the Redis reader!");
      break;
    }

    void *reply = nullptr;
    int status = redisReaderGetReply(reader, &reply);
    if (status != REDIS_OK) {
      ReportError("Failed to get a Redis reply!");
      break;
    }

    if (redisReply *r = (redisReply *)reply) {
      if (r->type == REDIS_REPLY_ARRAY && r->elements == 3) {
        if (!strcmp(r->element[0]->str, "subscribe")) {
          std::cout << "Subscribed to channel: " << r->element[1]->str << " "
                    << std::endl;
        } else if (!strcmp(r->element[0]->str, "message")) {
          if (verbose_outputs_) {
            std::cout << "Received message: " << r->element[2]->str
                      << std::endl;
          }
          // Sanity check: verify that the channel that sent the message was
          // the one that we subscribed for.

          // assert(!strcmp(r->element[1]->str, channel_name.c_str()));
          if (!strcmp(r->element[1]->str, channel_name.c_str())) {
            ProcessMessage(r->element[2]->str);
          }
        }
      }
      // std::cout << std::endl;
    }
  }

  close(subscription_socket_file_descriptor_);
  redisReaderFree(reader);
}

long long RedisBrokerConsumer::GetNumberOfProcessedMessages() const {
  long long current_number_of_messages{0};
  for (const auto &worker : workers_) {
    current_number_of_messages += worker->GetNumberOfProcessedMessages();
  }
  return current_number_of_messages;
}