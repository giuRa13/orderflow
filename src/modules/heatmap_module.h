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
    float module_header_height() override
    {
        const float slim_pad = 1.0f;
        const float slim_h   = ImGui::GetTextLineHeight() + slim_pad * 2.f;
        return slim_h * 2.0f + ImGui::GetStyle().ItemSpacing.y + ImGui::GetStyle().FramePadding.y;
    }
 
private:
    // --- Data ---
    void sample_bid_ask(double bid, double ask, double now);
    void ingest_trades(const SymbolData& sData);
 
    struct BidAskSample { 
        double time; 
        double bid; 
        double ask; 
    };
    struct TradePoint { 
        double glfw_time; 
        double price;
        double qty; 
        bool is_sell; 
    };
    std::deque<BidAskSample> m_history;
    std::deque<TradePoint>   m_trade_history;
    double m_last_sample_time = 0.0;
    double m_last_processed_tape_time = 0.0;
    double m_time_offset              = 0.0; // converts tape Unix time → glfwGetTime space
    std::string m_last_symbol;

    // --- Rendering sub-functions ---
    void setup_axes(double y_min, double y_max, double y_range, float pre_plot_h);
    void handle_interaction(double y_range);
    void draw_grid(double y_range);
    void draw_trail();
    void draw_live_lines(const SymbolData& sData);
    void draw_bubbles();
 
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
    float  m_lines_thickness = 2.0f;
    bool   m_show_best_bid_ask = true;
    bool   m_show_bubbles      = true;
    ImVec4 tick_grid_color = {0.30f, 0.30f, 0.30f, 0.45f};//IM_COL32(80, 80, 80, 60); //ImVec4(0.30f, 0.30f, 0.30f, 0.50f);
    // Cache for draw functions (set each frame in update_content)
    ImVec4 m_bid_color;
    ImVec4 m_ask_color;

    float  m_min_bubble_size   = 0.2f;   // minimum qty to draw a circle
    float  m_bubble_size     = 4.0f;   // base radius in px
    float  m_bubble_ratio    = 5.0f;   // max radius = bubble_size * ratio
};