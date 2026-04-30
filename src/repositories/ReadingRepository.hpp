#pragma once

#include "models/CreateReadingRequest.hpp"

#include <string>

class ReadingRepository {
  public:
    [[nodiscard]] std::string createReading(const CreateReadingRequest &request) const;
};
