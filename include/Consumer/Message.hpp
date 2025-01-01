#pragma once
#include <string>

struct Message {
  int processor_id;
  std::string processing_date_time;
  std::string source_channel_name;
  std::string message_id;
};