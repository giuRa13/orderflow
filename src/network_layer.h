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
    void process_book_ticker(const nlohmann::json& j);
    void process_book_ticker(const std::string& symbol, const nlohmann::json& j);
    void process_depth_data(const std::string& symbol, const nlohmann::json& j);
    void process_depth_diff(const std::string& symbol, const nlohmann::json& j);

private:
    ix::WebSocket m_ws;
    MarketData& m_data;
};