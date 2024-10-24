#include <csignal>
#include "matching_engine.h"
#include "order_server.h"

Common::Logger* logger = nullptr;
Exchange::MatchingEngine* matchingEngine = nullptr;

void signalHandler(int) {
    using namespace std::literals::chrono_literals;
    
    std::this_thread::sleep_for(10s);
    delete logger; 
    logger = nullptr;
    
    delete matchingEngine; 
    matchingEngine = nullptr;
    
    std::this_thread::sleep_for(10s);
    exit(EXIT_SUCCESS);
}

int main(int, char**) {
    logger = new Common::Logger("exchange_main.log");
    std::signal(SIGINT, signalHandler);
    const int sleepTime = 100 * 1000;

    Exchange::ClientRequestLFQueue clientRequestLFQueue(ME_MAX_CLIENT_UPDATES);
    Exchange::ClientResponseLFQueue clientResponseLFQueue(ME_MAX_CLIENT_UPDATES);
    Exchange::MEMarketUpdateLFQueue marketUpdatesLFQUeue(ME_MAX_MARKET_UPDATES);

    std::string timeStr;
    logger->log("%:% %() % Starting Matching Engine...\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&timeStr));
    
    auto matchingEngine = new Exchange::MatchingEngine(&clientRequestLFQueue, &clientResponseLFQueue, &marketUpdatesLFQUeue);
    matchingEngine->start();
    
    auto orderGateway = new Exchange::OrderServer(&clientRequestLFQueue, &clientResponseLFQueue, "lo", 8080);
    orderGateway->start();

    while(true) {
        logger->log("%:% %() % Sleeping for a few milliseconds..\n", __FILE__, __LINE__, __FUNCTION__,Common::getCurrentTimeStr(&timeStr));
        usleep(sleepTime);
    }
}