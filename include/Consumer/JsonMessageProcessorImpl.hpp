#pragma once
#include "IMessageProcessor.hpp"

class JsonMessageProcessorImpl : public IMessageProcessor {
public:
  std::optional<Message> ProcessMessage(const std::string &json) override;
};