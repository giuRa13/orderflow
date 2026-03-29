#pragma once
#include <imgui.h>
#include <market_data.h>

namespace CommonRender
{
    void draw_price_line(double price, ImVec4 color);
	void draw_custom_crosshair(ImVec4 color);
	void plot_candlesticks(const char* label_id, const TickCandle* candles, int count, double width_seconds, bool is_cvd, ImVec4 bull, ImVec4 bear);
};