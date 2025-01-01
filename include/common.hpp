#pragma once
#include <iostream>
#include <string.h>

#define REDIS_SERVER_HOSTNAME "127.0.0.1"
#define REDIS_SERVER_PORT "6379"

#define CFG_KEY_HOST "host"
#define CFG_KEY_PORT "port"
#define CFG_KEY_GROUP_SIZE "group_size"
#define CFG_KEY_SUB_CHANNEL "default_subscription_channel"
#define CFG_KEY_PROC_STREAM "default_processing_stream"
#define CFG_KEY_MONITORING_INTERVAL "monitoring_interval"

#define print(param) std::cout << param
#define println(param) print(param) << std::endl

#define case_break(condition, statement)                                       \
  case condition: {                                                            \
    statement;                                                                 \
  } break
