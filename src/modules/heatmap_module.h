#pragma once
#include <modules/base_module.h>
#include <deque>
 
class HeatmapModule : public BaseModule
{
public:
    HeatmapModule();
 
    void update_content(MarketData& data) override;
    void draw_settings_content(MarketData& data) override;
    void render_module_specific_header(MarketData& data) override;
 
private:
    // --- Data ---
    void sample_bid_ask(double bid, double ask, double now);
 
    struct BidAskSample { double time, bid, ask; };
    std::deque<BidAskSample> m_history;
    double m_last_sample_time = 0.0;
    std::string m_last_symbol;

    // --- Rendering sub-functions ---
    void setup_axes(double y_min, double y_max, double y_range, float pre_plot_h);
    void handle_interaction(double y_range);
    void draw_grid(double y_range);
    void draw_trail();
    void draw_live_lines(const SymbolData& sData, const MarketData& data);
 
    // axis — controlled, never controlled by ImPlot
    double m_x_min = 0.0;
    double m_x_max = 0.0;
    double m_y_center = 0.0;

    // Drag pan tracking
    ImVec2 m_prev_mouse = {-1.0f, -1.0f};
 
    // --- Settings ---
    float  m_time_window_sec   = 300.0f;
    float  m_right_pad_sec     = 10.0f;
    float  m_max_history_hrs   = 4.0f;
    float  m_sample_rate_ms    = 100.0f;
    float  m_tick_size         = 1.0f;
    int    m_visible_rows      = 30;

    bool   m_follow_price      = true;
    bool   m_show_crosshair    = true;
    bool   m_extend_lines_full = true;
    ImVec4 tick_grid_color = {0.30f, 0.30f, 0.30f, 0.45f};//IM_COL32(80, 80, 80, 60); //ImVec4(0.30f, 0.30f, 0.30f, 0.50f);

    // Cache for draw functions (set each frame in update_content)
    ImVec4 m_bid_color;
    ImVec4 m_ask_color;
};