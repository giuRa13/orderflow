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

    //void render(SymbolData& sData, MarketData& settings);

private:
    bool tape_show_time = true;
	bool tape_show_price = true;
	bool tape_show_size = true;
	bool tape_show_profile = true;
	bool tape_show_side = true;
};