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
    void render_spread_row(SymbolData& sData, MarketData& data, double last_price);
    void render_top_ui(MarketData& data, SymbolData& sData);

    // states
    std::string m_last_symbol = ""; 
    int col_number = 5;
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
    bool right_to_left_ask = false;
    bool right_to_left_bid = true;
    bool center_values_ask = true;
    bool center_values_bid = true;
    bool m_auto_scroll = true;
    float m_scroll_interval = 8.0f;
};