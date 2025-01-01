#pragma once
#include "Message.hpp"
#include <optional>

class IMessageProcessor {
public:
  virtual ~IMessageProcessor() = default;
  virtual std::optional<Message> ProcessMessage(const std::string &) = 0;
};