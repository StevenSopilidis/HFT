#include <csignal>
#include "matching_engine.h"
#include "order_server.h"
#include "market_data_publisher.h"

Common::Logger* logger = nullptr;
Exchange::MatchingEngine* matchingEngine = nullptr;
Exchange::OrderServer* orderServer = nullptr;
Exchange::MarketDataPublisher* marketDataPublisher = nullptr;

void signalHandler(int) {
    using namespace std::literals::chrono_literals;
    
    std::this_thread::sleep_for(10s);
    delete logger; 
    logger = nullptr;
    
    delete matchingEngine; 
    matchingEngine = nullptr;

    delete orderServer; 
    orderServer = nullptr;

    delete marketDataPublisher; 
    marketDataPublisher = nullptr;
    
    std::this_thread::sleep_for(10s);
    exit(EXIT_SUCCESS);
}

int main(int, char**) {
    logger = new Logger("exchange_main.log");
    std::signal(SIGINT, signalHandler);

    const int sleep = 100 * 1000;

    Exchange::ClientRequestLFQueue clientRequests(ME_MAX_CLIENT_UPDATES);
    Exchange::ClientResponseLFQueue clientResponses(ME_MAX_CLIENT_UPDATES);
    Exchange::MEMarketUpdateLFQueue marketUpdates(ME_MAX_MARKET_UPDATES);
    
    std::string timeStr;
    logger->log("%:% %() % Starting Matching Engine...\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&timeStr));
    matchingEngine = new Exchange::MatchingEngine(&clientRequests, &clientResponses, &marketUpdates);
    matchingEngine->start();

    const std::string marketPublisherIface = "lo";
    const std::string snapshotPublishIP = "233.252.14.1", incrementalUpdatesPublishIP = "233.252.14.3";
    const int snapshotPublishPort = 20000, incrementalUpdatesPublishPort = 20001;
    logger->log("%:% %() % Starting Market Data Publisher...\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&timeStr));
    marketDataPublisher = new Exchange::MarketDataPublisher(&marketUpdates, marketPublisherIface, snapshotPublishIP, snapshotPublishPort, incrementalUpdatesPublishIP, incrementalUpdatesPublishPort);
    marketDataPublisher->start();

    const std::string orderGatewayIface = "lo";
    const int orderGatewayPort = 12345;
    logger->log("%:% %() % Starting Order Server...\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&timeStr));
    orderServer = new Exchange::OrderServer(&clientRequests, &clientResponses, orderGatewayIface, orderGatewayPort);
    orderServer->start();
    
    while(true) {
        logger->log("%:% %() % Sleeping for a few milliseconds..\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&timeStr));
        usleep(sleep * 1000);
    }
}