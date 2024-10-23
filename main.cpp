#include "socket_utils.h"
#include "Exchange/matcher/matching_engine.h"
#include "Exchange/order_server/client_request.h"
#include "Exchange/order_server/client_response.h"
#include "Exchange/order_server/order_server.h"

int main() {
    auto clientRequests = new LFQueue<Exchange::MEClientRequest>(1024);
    auto clientResponses = new LFQueue<Exchange::MEClientResponse>(1024);
    auto marketUpdates = new LFQueue<Exchange::MEMarketUpdate>(1024);

    auto engine = new Exchange::MatchingEngine(clientRequests, clientResponses, marketUpdates);
    auto orderGateway = new Exchange::OrderServer(clientRequests, clientResponses, "lo", 8080);
    orderGateway->run();
}