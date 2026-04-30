#include "ReadingService.hpp"

#include "services/ValidationService.hpp"

ReadingService::ReadingService(ReadingRepository repository)
    : repository_(std::move(repository)) {
}

std::string ReadingService::createReading(const Json::Value &payload) const {
    const auto errors = ValidationService::validateCreateReadingRequest(payload);
    if (!errors.empty()) {
        throw std::invalid_argument(ValidationService::joinErrors(errors));
    }

    return repository_.createReading(ValidationService::buildCreateReadingRequest(payload));
}
