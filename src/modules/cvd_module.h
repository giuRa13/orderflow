#pragma once
#include <modules/base_module.h>
#include <imgui.h>
#include <IconsFontAwesome5.h>
#include <market_data.h>

class CVDModule : public BaseModule
{
public:
    CVDModule();
    void update_content(MarketData& data) override;
    void draw_settings_content(MarketData& data) override;

    //void render(MarketData& data);

private:
    bool show_ma = false;
};