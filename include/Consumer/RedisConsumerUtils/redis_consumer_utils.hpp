#pragma once
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "../Message.hpp"

inline std::string
StringToRespProtocolFormat(const std::string &string_to_format) {
  return "$" + std::to_string(string_to_format.length()) + "\r\n" +
         string_to_format + "\r\n";
}

inline std::string CreateSubscriptionCommand(const std::string &channel_name) {
  const std::string resp_formatted_subscription_command =
      "*2\r\n$9\r\nSUBSCRIBE\r\n";
  const std::string resp_formatted_channel_name =
      StringToRespProtocolFormat(channel_name);

  return resp_formatted_subscription_command + resp_formatted_channel_name;
}

inline std::string
CreateWriteMessageToStreamCommand(const std::string &stream_name,
                                  const std::vector<std::string> &values) {
  if (stream_name.empty() || values.empty()) {
    return "";
  }

  std::ostringstream command_output_stream;
  // Add XADD + the stream's name + '*' = 3
  // Plus 2 * the number of values in the "values" vector.
  command_output_stream << "*" + std::to_string(3 + (2 * values.size())) +
                               "\r\n";
  command_output_stream << StringToRespProtocolFormat(
      "XADD"); // The XADD command
  command_output_stream << StringToRespProtocolFormat(
      stream_name); // The stream's name
  command_output_stream << StringToRespProtocolFormat(
      "*"); // Auto generate stream id

  for (int i = 0; i < values.size(); ++i) {
    command_output_stream << StringToRespProtocolFormat("value" +
                                                        std::to_string(i + 1))
                          << StringToRespProtocolFormat(values[i]);
  }
  return command_output_stream.str();
}

inline std::string
CreateWriteMessageToStreamCommand(const std::string &stream_name,
                                  const Message &message) {
  assert(!stream_name.empty());

  std::ostringstream command_output_stream;
  // Apply RESP formatting
  command_output_stream << "*11\r\n"; // The number of parameters
  command_output_stream << StringToRespProtocolFormat(
      "XADD"); // The XADD command
  command_output_stream << StringToRespProtocolFormat(
      stream_name); // The stream's name
  command_output_stream << StringToRespProtocolFormat(
      "*"); // Auto generate stream id

  /* Add the following values to the stream:
    "Processor_id", <The ID of the consumer / worker that processed the message>
    "Processing_date_time", <The date and time when the message was processed>
    "Source_channel_name", <The name of the channel that sent the message>
    "Message_id", <The ID of the message>
  */
  command_output_stream << StringToRespProtocolFormat("Processor_id")
                        << StringToRespProtocolFormat(
                               std::to_string(message.processor_id));
  command_output_stream << StringToRespProtocolFormat("Processing_date_time")
                        << StringToRespProtocolFormat(
                               message.processing_date_time);
  command_output_stream << StringToRespProtocolFormat("Source_channel_name")
                        << StringToRespProtocolFormat(
                               message.source_channel_name);
  command_output_stream << StringToRespProtocolFormat("Message_id")
                        << StringToRespProtocolFormat(message.message_id);
  return command_output_stream.str();
}

inline std::string GetCurrentTime() {
  // Get current time as a system clock time point with milliseconds
  // precision
  auto now = std::chrono::system_clock::now();
  // Convert time_point to time_t (for C-style time formatting)
  std::time_t now_time = std::chrono::system_clock::to_time_t(now);
  // Convert to tm struct for local time formatting
  std::tm local_time = *std::localtime(&now_time);
  // Extract milliseconds as duration
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) %
            1000;
  // Use stringstream to store formatted time in a string
  std::ostringstream time_stream;
  time_stream << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S") << '.'
              << std::setw(3) << std::setfill('0') << ms.count();
  return time_stream.str();
}