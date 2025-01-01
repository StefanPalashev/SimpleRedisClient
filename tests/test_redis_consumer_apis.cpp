#include <cstring>
#include <gtest/gtest.h>
#include <hiredis/hiredis.h>
#include <iostream>
#include <random>
#include <thread>

#include "../include/Consumer/RedisConsumer.hpp"

const std::string valid_server_hostname = "127.0.0.1";
unsigned short valid_server_port = 6379;
const std::string invalid_server_hostname = "256.256.256.256";
unsigned short invalid_server_port = 6380;

// A helper function that tests the availability to the Redis server
bool IsRedisServerAlive(const std::string &host, int port) {
  redisContext *c = redisConnect(host.c_str(), port);
  if (c != NULL && c->err) {
    std::cerr << "Error: " << c->errstr << std::endl;
    return false;
  }

  redisReply *reply = (redisReply *)redisCommand(c, "PING");
  if (reply != NULL && reply->type == REDIS_REPLY_STATUS &&
      strcmp(reply->str, "PONG") == 0) {
    std::cout << "Redis Server is up!" << std::endl;
    freeReplyObject(reply);
    redisFree(c);
    return true;
  }

  freeReplyObject(reply);
  redisFree(c);
  return false;
}

/*
 The following tests serve as integration tests, but they still utilize the
 GTest library These test require that the redis-server service is up and
 running and will use the function IsRedisServerAlive() to verify that by
 pinging the server.

 The tests could possibly be modified to start and stop the server as needed,
 however this would probably require some additional privileges like allowing
 passwordless calls to: sudo systemctl start redis-server and sudo systemctl
 stop redis-server. Because of that for now the test would require manual
 intervention for stopping and starting the service.
*/
TEST(RedisConsumerAPIsTest, WillEstablishAConnectionWhenEverythingIsOk) {
  RedisConsumer redis_consumer(false);

  // Verify that Redis is working
  ASSERT_EQ(IsRedisServerAlive(valid_server_hostname, valid_server_port), true);
  redis_consumer.EstablishConnection(valid_server_hostname, valid_server_port);
}

TEST(RedisConsumerAPIsTest, WillNotEstablishAConnectionWithInvalidHostname) {
  RedisConsumer redis_consumer(false);

  EXPECT_DEATH(redis_consumer.EstablishConnection(invalid_server_hostname,
                                                  valid_server_port),
               "Unable to connect to a Redis server!");
}

TEST(RedisConsumerAPIsTest, WillNotEstablishAConnectionWithInvalidPort) {
  RedisConsumer redis_consumer(false);

  EXPECT_DEATH(redis_consumer.EstablishConnection(valid_server_hostname,
                                                  invalid_server_port),
               "Unable to connect to a Redis server!");
}

// Get the number of subscriptions to a given channel
// Returns -1 in case of an error
int GetNumberOfSubscriptionsToChannel(const std::string &channel_name) {
  int number_of_subscriptions = -1;
  redisContext *c =
      redisConnect(valid_server_hostname.c_str(), valid_server_port);
  if (c == nullptr || c->err) {
    if (c) {
      std::cerr << "Connection error: " << c->errstr << std::endl;
    } else {
      std::cerr << "Connection error: can't allocate redis context"
                << std::endl;
    }
    return number_of_subscriptions;
  }

  std::string command = "PUBSUB NUMSUB " + channel_name;
  redisReply *reply = (redisReply *)redisCommand(c, command.c_str());

  if (reply == nullptr) {
    std::cerr << "Failed to execute command." << std::endl;
    redisFree(c);
    return number_of_subscriptions;
  }

  if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 2) {
    std::string channel_name = reply->element[0]->str;
    int subscribers_count = reply->element[1]->integer;
    std::cout << "Channel: " << channel_name
              << ", Subscribers: " << subscribers_count << std::endl;
    number_of_subscriptions = subscribers_count;
  } else {
    std::cerr << "Unexpected reply type." << std::endl;
  }

  freeReplyObject(reply);
  redisFree(c);
  return number_of_subscriptions;
}

std::string GenerateRandomString(size_t length) {
  const std::string charset =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

  std::random_device rd;
  std::mt19937 engine(rd());

  std::uniform_int_distribution<> dist(0, charset.size() - 1);

  std::string result{};
  for (size_t i = 0; i < length; ++i) {
    result += charset[dist(engine)];
  }

  return result;
}

TEST(RedisConsumerAPIsTest, CanSubscribeToAChannel) {
  RedisConsumer redis_consumer(false);
  // Generate a new channel each time the test is run
  const std::string testing_channel_name =
      "testing_channel_" + GenerateRandomString(7);

  // Verify that Redis is working
  ASSERT_EQ(IsRedisServerAlive(valid_server_hostname, valid_server_port), true);
  // Verify that there are no subscriptions to the channel
  ASSERT_EQ(GetNumberOfSubscriptionsToChannel(testing_channel_name), 0);

  // Connect and subscribe
  redis_consumer.EstablishConnection(valid_server_hostname, valid_server_port);
  // The subscription is on a different thread so the test can continue,
  // otherwise the consumer will block here waiting for messages from the
  // subscription
  std::thread subscription_thread([&redis_consumer, &testing_channel_name]() {
    redis_consumer.SubscribeToChannel(testing_channel_name);
  });
  subscription_thread.detach();

  // Give some time for the subscription to be established
  std::this_thread::sleep_for(std::chrono::seconds(2));

  // Verify that there is an active subsciption now
  ASSERT_EQ(GetNumberOfSubscriptionsToChannel(testing_channel_name), 1);
}

// Get the length of a given stream
// Returns -1 in case of an error
int GetStreamLength(const std::string &stream_name) {
  int stream_length = -1;
  redisContext *c =
      redisConnect(valid_server_hostname.c_str(), valid_server_port);
  if (c != NULL && c->err) {
    std::cout << "Error: " << c->errstr << std::endl;
    return stream_length;
  }

  std::string command = "XLEN " + stream_name;

  redisReply *reply = (redisReply *)redisCommand(c, command.c_str());
  if (reply == NULL) {
    std::cout << "Error: " << c->errstr << std::endl;
    return stream_length;
  }

  if (reply->type == REDIS_REPLY_INTEGER) {
    std::cout << "Stream (" << stream_name << ") length: " << reply->integer
              << std::endl;
    stream_length = reply->integer;
  }

  freeReplyObject(reply);
  redisFree(c);
  return stream_length;
}

TEST(RedisConsumerAPIsTest, CanWriteDataToAStream) {
  RedisConsumer redis_consumer(false);
  const std::string testing_stream_name =
      "testing_stream_" + GenerateRandomString(7);

  // Verify that Redis is working
  ASSERT_EQ(IsRedisServerAlive(valid_server_hostname, valid_server_port), true);
  // Verify that there are no records in the stream
  ASSERT_EQ(GetStreamLength(testing_stream_name), 0);

  // The consumer needs to be connected before it can send data to a stream
  redis_consumer.EstablishConnection(valid_server_hostname, valid_server_port);

  // Send a sample vector of data
  ASSERT_EQ(
      redis_consumer.AddDataToStream(testing_stream_name, {"John", "Smith"}),
      true);

  // Verify that there is a record in the stream
  ASSERT_EQ(GetStreamLength(testing_stream_name), 1);

  // Send another vector of data
  ASSERT_EQ(
      redis_consumer.AddDataToStream(testing_stream_name, {"Jane", "Smith"}),
      true);

  // Verify that the second vector was also recorded in the stream
  ASSERT_EQ(GetStreamLength(testing_stream_name), 2);
}