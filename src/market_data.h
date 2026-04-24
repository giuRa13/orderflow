#pragma once
#include <vector>
#include <deque>
#include <mutex>
#include <string>
#include <map>
#include <unordered_map>
#include<algorithm>
#include <imgui.h>

/*struct Candle {
    double time; // Use double for ImPlot time axis
    double open;
    double high;
    double low;
    double close;
};*/

struct TickCandle {
	double time;
	double open, high, low, close;
	double cvd_open, cvd_high, cvd_low, cvd_close;
	
	double buy_vol = 0;   // Volume from Market Buys (Taker Buy)
    double sell_vol = 0;  // Volume from Market Sells (Taker Sell)
    double delta = 0;     // buy_vol - sell_vol
    double cvd = 0;       
    int trade_count = 0;
};

struct RawTick { // specifically for Big Trade alerts
    double price;
    double qty;
    bool is_sell;
};

struct TapeTick {
    double time;
    double price;
    double quantity;
    bool is_sell;

    double bid_at_time; 
    double ask_at_time;
};

struct BookLevel {
    double price;
    double qty;
};

struct SymbolData {
    std::vector<TickCandle> candles;
    std::deque<TapeTick> tape;
    // STORAGE for DOM levels
    std::map<double, double, std::greater<double>> full_asks;
    std::map<double, double, std::greater<double>> full_bids;
    //std::map<double, double> ask_sums; 
    //std::map<double, double> bid_sums;
    std::map<double, double> ask_sums; // Change from unordered_map
    std::map<double, double> bid_sums;
    double running_cvd = 0;
    double max_tape_qty = 1.0;

    double last_best_bid = 0.0;
    double last_best_ask = 0.0;

    long long last_update_id = 0;
    bool snapshot_loaded = false;
    bool dom_dirty = true;
    double cached_max_vol = 1.0;
    double cached_spread = 0.0;
};

class MarketData
{
public:
    std::recursive_mutex mtx;
    std::unordered_map<std::string, SymbolData> assets;

    // Configuration (Shared across Network and UI)
    double tick_timeframe = 60.0; 
    int max_candles = 500;
    int m_max_tape_rows = 20000; // for C++ app, storing 10,000 or 50,000 trades in a std::deque is very cheap (only a few megabytes of RAM)
    double tick_size = 0.10;
    double dom_step = 5.0;
    bool follow_live = true;

    // Connections
    std::string m_current_symbol = "btcusdt";
    bool m_is_futures = true;
    bool m_reconnect_requested = false;

    ImVec4 ask_color = ImVec4(0.454f, 0.65f, 0.886f, 1.0f);
    ImVec4 bid_color = ImVec4(0.666f, 0.227f, 0.215f, 1.0f);
    ImVec4 crosshair_color = ImVec4(0.7f, 0.7f, 0.7f, 0.8f);

    //SymbolData& get(const std::string& symbol) { return assets[symbol]; }
    SymbolData& get(const std::string& symbol) 
    {
        std::string lower_sym = symbol;
        for (auto & c: lower_sym) c = tolower(c);
        
        std::lock_guard<std::recursive_mutex> lock(mtx);
        return assets[lower_sym]; 
    }

    void clear() 
    {
        std::lock_guard<std::recursive_mutex> lock(mtx);
        assets.clear();
    }
};