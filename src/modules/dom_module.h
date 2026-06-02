#pragma once
#include <modules/base_module.h>
#include <imgui.h>
#include <IconsFontAwesome5.h>
#include <market_data.h>

class DOMModule : public BaseModule
{
public:
    DOMModule();
    void update_content(MarketData& data) override;
    void draw_settings_content(MarketData& data) override;

private:
    void render_dom_bar(double qty, double max_vol, ImVec4 color, bool right_to_left, bool center_text);
    void render_market_cell(double qty, double max_vol, ImVec4 color, ImVec4 text_color, bool align_right, bool center_text, double last_activity_time);
    void render_spread_row(SymbolData& sData, MarketData& data, double last_price);
    void render_top_ui(MarketData& data, SymbolData& sData);
    void render_main_table(MarketData& data, SymbolData& sData, double step, double live_bucket);

    // states
    std::string m_last_symbol = ""; 
    int col_number = 7;
    bool m_last_side_was_sell = false;
    bool m_needs_recenter = true;
    double m_last_recenter_time = 0.0;
    double m_last_centered_price = -1.0;
    double m_anchor_bucket = 0.0; 

    float m_current_visual_scroll = 0.0f; 
    float m_scroll_smoothing = 0.02f;     

    // user settings
    ImVec4 price_color = {0.462f, 0.462f, 0.462f, 1.0f};
    ImVec4 price_highlight  = {1.0f, 1.0f, 0.0f, 1.0f};
    ImVec4 ask_bg_color  = {0.443f, 0.027f, 0.015f, 1.0f}; // 113, 7, 4
    ImVec4 bid_bg_color  = {0.027f, 0.200f, 0.407f, 1.0f}; // 7, 51, 104
    ImVec4 buys_text_color  = {0.325f, 0.490f, 0.694f, 1.0f}; // 83, 125, 177
    ImVec4 sells_text_color  = {0.752f, 0.313f, 0.301f, 1.0f}; // 192, 80, 77
    ImVec4 vp_poc_color     = {0.894f,  0.584f, 0.086f,  1.0f};  
    ImVec4 vp_va_color      = {0.796f, 0.803f, 0.796f, 1.0f};  
    ImVec4 vp_outside_color = {0.4f, 0.4f, 0.4f, 1.0f};
    bool right_to_left_ask = false;
    bool right_to_left_bid = true;
    bool right_to_left_delta = true;
    bool right_to_left_volume = true;
    bool center_values_ask = true;
    bool center_values_bid = true;
    bool center_values_market_sells = true;
    bool center_values_market_buys = true;
    bool center_values_delta         = true;
    bool center_values_volume        = true;
    bool m_market_orders_border = false;
    bool m_auto_scroll = true;
    float m_scroll_interval = 8.0f;
    int m_price_decimals = 2;
    int m_limit_decimals = 1;
    int m_market_decimals = 1;
    float m_highlight_fadeout_ms = 1000.0f;
};