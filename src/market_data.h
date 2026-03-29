#pragma once
#include <vector>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
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
};

struct SymbolData {
    std::vector<TickCandle> candles;
    std::deque<TapeTick> tape;
    double running_cvd = 0;
    double max_tape_qty = 1.0;
};

class MarketData
{
public:
    std::recursive_mutex mtx;
    std::unordered_map<std::string, SymbolData> assets;
    std::vector<TickCandle> candles;
    std::deque<TapeTick> tape;

    // Persistent Totals
    double running_cvd = 0;
    double max_tape_qty = 1.0;

    // Configuration (Shared across Network and UI)
    double tick_timeframe = 60.0; 
    int max_candles = 500;
    int max_tape_rows = 500;
    bool follow_live = true;

    // Connections
    std::string m_current_symbol = "btcusdt";
    bool m_is_futures = true;
    bool m_reconnect_requested = false;

    // App Theme (Centralized colors)
    ImVec4 ask_color = ImVec4(0.454f, 0.65f, 0.886f, 1.0f);
    ImVec4 bid_color = ImVec4(0.666f, 0.227f, 0.215f, 1.0f);
    ImVec4 crosshair_color = ImVec4(0.7f, 0.7f, 0.7f, 0.8f);

    SymbolData& get(const std::string& symbol) { return assets[symbol]; }

    void clear() 
    {
        std::lock_guard<std::recursive_mutex> lock(mtx);
        candles.clear();
        tape.clear();
        running_cvd = 0;
    }
};