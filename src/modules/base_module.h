#pragma once
#include <string>
#include <algorithm>
#include <imgui.h>
#include <IconsFontAwesome5.h>
#include "market_data.h"

class BaseModule 
{
public:
    bool is_open = false;
    bool open_settings = false;
    char symbol_input[16] = "btcusdt";
    std::string current_symbol = "btcusdt";
    std::string window_name;

    BaseModule(const std::string& name);
    virtual ~BaseModule() = default;

    void render_common_header();
    void render_standalone(MarketData& data);

    virtual void update_content(MarketData& data) = 0;
    void render_settings_window(MarketData& data);
    virtual void draw_settings_content(MarketData& data) = 0;
};