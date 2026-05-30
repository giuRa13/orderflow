#pragma once
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXHttpClient.h>
#include <ixwebsocket/IXHttp.h>
#include <nlohmann/json.hpp>
#include <market_data.h>
#include <iostream>
#include <set>
#include <memory>

class NetworkLayer
{
public:
    NetworkLayer(MarketData& data);
    ~NetworkLayer();

    void start_multi(const std::set<std::string>& symbols, bool is_futures);
    void end();

    void fetch_dom_snapshot(const std::string& symbol, bool is_futures);

    int connection_status = 0;

private:
    void process_tick_data(const std::string& symbol, const nlohmann::json& j);
    void process_book_ticker(const std::string& symbol, const nlohmann::json& j);
    void process_depth_diff(const std::string& symbol, const nlohmann::json& j);

private:
     // Futures needs two sockets: aggTrade is on /market, depth+bookTicker on /public
    // Spot uses a single combined socket (old URL still works)
    ix::WebSocket m_ws_market;  // Futures: /market/stream (aggTrade)
                                // Spot:    combined stream (all three)
    ix::WebSocket m_ws_public;  // Futures: /public/stream (depth + bookTicker)
                                // Spot:    unused
    MarketData& m_data;
};