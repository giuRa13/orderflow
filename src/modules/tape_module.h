#pragma once
#include <modules/base_module.h>
#include <imgui.h>
#include <IconsFontAwesome5.h>
#include <market_data.h>

class TapeModule : public BaseModule
{
public:
    TapeModule();
    void update_content(MarketData& data) override;
    void draw_settings_content(MarketData& data) override;

private:
    void rebuild_filtered_view(SymbolData& sData);

private:
    // A "View" of the data to keep the Clipper happy
    std::vector<TapeTick> m_filtered_view;
    float m_min_trade_size = 0.0f;
    bool m_aggregate_by_time = false;
    float m_aggregation_ms = 100.0f; // Group trades within 100ms

    bool tape_show_time = true;
	bool tape_show_price = true;
	bool tape_show_size = true;
	bool tape_show_profile = true;
	bool tape_show_side = true;

    bool tape_show_slippage_highlights = true;
};