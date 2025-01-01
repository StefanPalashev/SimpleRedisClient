#include "../include/Consumer/JsonMessageProcessorImpl.hpp"
#include <gtest/gtest.h>
#include <iostream>

TEST(JsonMessageProcessorTest, ValidJson_ReturnsMessage) {
  JsonMessageProcessorImpl processor;
  std::string valid_json = R"({"message_id": "12345"})";

  auto result = processor.ProcessMessage(valid_json);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->message_id, "12345");
}

TEST(JsonMessageProcessorTest, InvalidJson_ReturnsNullOpt) {
  JsonMessageProcessorImpl processor;
  std::string json = R"({"message": this_is_not_an_id})";

  auto result = processor.ProcessMessage(json);

  EXPECT_FALSE(result.has_value());
}

TEST(JsonMessageProcessorTest, EmptyMessageId_StillReturnsMessage) {
  JsonMessageProcessorImpl processor;
  std::string json = R"({"message_id": ""})";

  auto result = processor.ProcessMessage(json);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->message_id.size(), 0);
}
