#include "../../include/Consumer/JsonMessageProcessorImpl.hpp"
#include <iostream>

std::optional<Message>
JsonMessageProcessorImpl::ProcessMessage(const std::string &json) {
  // Simple sanity check that there's a message_id in the json
  if (json.find("message_id") == std::string::npos) {
    return {};
  }

  auto end = json.find_last_of('\"');
  if (end != std::string::npos) {
    auto start = json.find_last_of('\"', end - 1);
    if (start != std::string::npos) {
      Message msg = {.message_id = json.substr(start + 1, end - start - 1)};
      // std::cout << "Extracted:[" << msg.message_id << "]" << std::endl;
      return msg;
    }
  }

  return {};
}