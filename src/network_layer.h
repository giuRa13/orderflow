#pragma once
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>
#include <market_data.h>
#include <iostream>
#include <set>

class NetworkLayer
{
public:
    NetworkLayer(MarketData& data);

    void start_multi(const std::set<std::string>& symbols, bool is_futures);
    void end();

private:
    void process_tick_data(const std::string& symbol, const nlohmann::json& j);
    void process_book_ticker(const nlohmann::json& j);

private:
    ix::WebSocket m_ws;
    MarketData& m_data;
};