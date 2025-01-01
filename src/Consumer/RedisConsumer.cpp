#include <arpa/inet.h>
#include <assert.h>
#include <chrono>
#include <netdb.h>
#include <optional>
#include <sys/socket.h>
#include <unistd.h>

#include <hiredis/hiredis.h>

#include <sstream>

#include "../../include/Consumer/JsonMessageProcessorImpl.hpp"
#include "../../include/Consumer/RedisConsumer.hpp"
#include "../../include/Consumer/RedisConsumerUtils/redis_consumer_utils.hpp"

int RedisConsumer::next_id_ = 1;

class RedisConsumer::MessageProcessorImpl {
public:
  MessageProcessorImpl()
      : message_processor_(std::make_unique<JsonMessageProcessorImpl>()) {}

  std::optional<Message> ProcessMessage(const std::string &message) {
    return message_processor_->ProcessMessage(message);
  }

private:
  std::unique_ptr<IMessageProcessor> message_processor_;
};

RedisConsumer::RedisConsumer(bool verbose_outputs)
    : id_{next_id_++}, redis_server_hostname_{}, redis_server_port_{0},
      subscription_socket_file_descriptor_{-1},
      processing_socket_file_descriptor_{-1},
      initial_connection_established_{false},
      write_connection_established_{false}, subsciption_channel_{},
      processing_stream_{}, number_of_processed_messages_{0},
      number_of_processing_errors_{0},
      message_processor_impl_(std::make_unique<MessageProcessorImpl>()),
      verbose_outputs_{verbose_outputs} {}
RedisConsumer::~RedisConsumer() = default;

void RedisConsumer::EstablishConnection(
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

void RedisConsumer::EstablishConnection(
    const std::string &redis_server_hostname,
    unsigned short redis_server_port) {
  EstablishConnection(redis_server_hostname, redis_server_port,
                      subscription_socket_file_descriptor_);
  redis_server_hostname_ = redis_server_hostname;
  redis_server_port_ = redis_server_port;

  initial_connection_established_ = true;
  std::cout << "Connected to Redis server!" << std::endl;
}

void RedisConsumer::ProcessMessage(const std::string &message) {
  std::optional<Message> processed_message_opt =
      message_processor_impl_->ProcessMessage(message);
  if (processed_message_opt) {
    auto processed_message = processed_message_opt.value();
    processed_message.processor_id = id_;
    processed_message.processing_date_time = GetCurrentTime();
    processed_message.source_channel_name = subsciption_channel_;
    std::cout << "Post processing of message with id = ("
              << processed_message.message_id << ")." << std::endl
              << "Processed by consumer with id = "
              << processed_message.processor_id << " at "
              << processed_message.processing_date_time
              << ", received from channel ("
              << processed_message.source_channel_name << ")." << std::endl;
    // XADD
    if (!processing_stream_.empty()) {
      if (AddDataToStream(CreateWriteMessageToStreamCommand(processing_stream_,
                                                            processed_message),
                          true)) {
        if (verbose_outputs_) {
          std::cout << "Successfully added the message to the target stream "
                       "for processed messages!"
                    << std::endl;
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

  std::cout << "Messages processed so far: " << number_of_processed_messages_
            << std::endl;

  if (number_of_processing_errors_) {
    std::cout << "Number of encountered processing errors: "
              << number_of_processing_errors_ << std::endl;
  }
}

void RedisConsumer::SubscribeToChannel(const std::string &channel_name,
                                       const std::string &processing_stream) {
  if (!initial_connection_established_) {
    ReportError("The client is not connected to a Redis server! "
                "Please, make sure that there is a running Redis server and "
                "the client is connected to it.");
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
  if (!processing_stream.empty()) {
    EstablishConnection(redis_server_hostname_, redis_server_port_,
                        processing_socket_file_descriptor_);
    write_connection_established_ = true;
    processing_stream_ = processing_stream;
    std::cout
        << "Successfully established a connection for message processing!"
        << std::endl
        << "Processing stream set to: " << processing_stream_
        << ". All successfully processed messages will be added to that stream!"
        << std::endl;
  }

  redisReader *reader = redisReaderCreate();
  if (reader == nullptr) {
    ReportError("Failed to create a Redis reader!");
    close(subscription_socket_file_descriptor_);
    exit(EXIT_FAILURE);
  }

  char buffer[64] = {};
  // int msg_idx{0};
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
      // std::cout << "Msg Id: " << msg_idx++ << " Msg type: " << r->type;
      if (r->type == REDIS_REPLY_ARRAY && r->elements == 3) {
        if (!strcmp(r->element[0]->str, "subscribe")) {
          std::cout << "Subscribed to channel: " << r->element[1]->str << " "
                    << std::endl;
        } else if (!strcmp(r->element[0]->str, "message")) {
          if (verbose_outputs_) {
            std::cout << "Received message: " << r->element[2]->str
                      << std::endl;
          }
          // Sanity check: verify that the channel that sent the message was the
          // one that we subscribed for.

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

bool RedisConsumer::AddDataToStream(const std::string &resp_formatted_command,
                                    bool is_internal_call) const {
  int handling_socket_file_descriptor{-1};
  if (is_internal_call) {
    assert(write_connection_established_);
    handling_socket_file_descriptor = processing_socket_file_descriptor_;
  } else {
    EstablishConnection(redis_server_hostname_, redis_server_port_,
                        handling_socket_file_descriptor);
  }

  ssize_t bytes_sent =
      send(handling_socket_file_descriptor, resp_formatted_command.c_str(),
           resp_formatted_command.size(), 0);
  if (bytes_sent < 0) {
    ReportError("Failed to send the xadd command!");
    if (!is_internal_call) {
      close(handling_socket_file_descriptor);
    }
    return false;
  }

  redisReader *reader = redisReaderCreate();
  if (reader == nullptr) {
    ReportError("Failed to create a Redis reader!");
    if (!is_internal_call) {
      close(handling_socket_file_descriptor);
    }
    return false;
  }

  char buffer[1024] = {};
  ssize_t bytes_read =
      recv(handling_socket_file_descriptor, buffer, sizeof(buffer), 0);
  if (bytes_read < 0) {
    ReportError("Failed to read from the server!");
    if (!is_internal_call) {
      close(handling_socket_file_descriptor);
    }
    redisReaderFree(reader);
    return false;
  }

  if (redisReaderFeed(reader, buffer, bytes_read) != REDIS_OK) {
    ReportError("Failed to feed the Redis reader!");
    if (!is_internal_call) {
      close(handling_socket_file_descriptor);
    }
    redisReaderFree(reader);
    return false;
  }

  void *reply = nullptr;
  int status = redisReaderGetReply(reader, &reply);
  if (status != REDIS_OK) {
    ReportError("Failed to get a Redis reply!");
    if (!is_internal_call) {
      close(handling_socket_file_descriptor);
    }
    redisReaderFree(reader);
    return false;
  }

  if (redisReply *r = (redisReply *)reply) {
    if (r->type == REDIS_REPLY_STRING) {
      if (verbose_outputs_) {
        std::cout << "Successfully wrote the data to Stream with id = "
                  << r->str << std::endl;
      }
      if (!is_internal_call) {
        close(handling_socket_file_descriptor);
      }
      redisReaderFree(reader);
      return true;
    } else {
      ReportError("Unexpected response type");
      if (!is_internal_call) {
        close(handling_socket_file_descriptor);
      }
      redisReaderFree(reader);
      return false;
    }
  } else {
    if (!is_internal_call) {
      close(handling_socket_file_descriptor);
    }
    redisReaderFree(reader);
    return false;
  }

  if (!is_internal_call) {
    close(handling_socket_file_descriptor);
  }
  return true;
}

bool RedisConsumer::AddDataToStream(
    const std::string &stream_name,
    const std::vector<std::string> &values) const {
  if (stream_name.empty() || values.empty()) {
    ReportError("Adding data to stream has failed! " +
                (stream_name.empty()
                     ? "Please, provide a stream_name (key)."
                     : "There are no values to be added to stream: \"" +
                           stream_name + "\"!"));
    std::cout << "Sample usage:\r\n\tAddDataToStream(mystream, {\"John\", "
                 "\"Smith\");\r\n\tWill result in: XADD mystream * John Smith"
              << std::endl;
    return false;
  }

  const std::string redis_xadd_command =
      CreateWriteMessageToStreamCommand(stream_name, values);

  if (verbose_outputs_) {
    std::cout << "Redis_xadd_command:" << std::endl
              << "<Start>" << std::endl
              << redis_xadd_command << "<End>" << std::endl;
  }
  assert(!redis_xadd_command.empty());

  return AddDataToStream(redis_xadd_command, false);
}