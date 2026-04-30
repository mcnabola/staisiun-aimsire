#pragma once

#include <drogon/HttpController.h>

class ReadingController : public drogon::HttpController<ReadingController> {
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(ReadingController::createReading, "/api/v1/readings", drogon::Post);
    METHOD_LIST_END

    void createReading(
        const drogon::HttpRequestPtr &request,
        std::function<void(const drogon::HttpResponsePtr &)> &&callback) const;
};
