#include <drogon/drogon.h>

int main() {
    drogon::app().loadConfigFile("config/config.json");
    drogon::app().run();
    return 0;
}
