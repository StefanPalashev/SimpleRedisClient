#pragma once

class IObservableConsumer {
public:
  virtual ~IObservableConsumer() = default;
  virtual long long GetNumberOfProcessedMessages() const = 0;
};