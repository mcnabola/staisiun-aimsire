#pragma once

#include "models/CreateReadingRequest.hpp"
#include "repositories/ReadingRepository.hpp"

#include <json/value.h>

#include <string>

class ReadingService {
  public:
    explicit ReadingService(ReadingRepository repository = {});

    [[nodiscard]] std::string createReading(const Json::Value &payload) const;

  private:
    ReadingRepository repository_;
};
