#pragma once
#include <modules/base_module.h>
#include <imgui.h>
#include <IconsFontAwesome5.h>
#include <market_data.h>

class CandleChartModule : public BaseModule
{
public:
    CandleChartModule();
    void update_content(MarketData& data) override;
    void draw_settings_content(MarketData& data) override;
    //void render(MarketData& data);

private:
    std::string last_drawn_symbol; // detect switches
    
    bool show_ma = false;
};