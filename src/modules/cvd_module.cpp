#include <modules/cvd_module.h>
#include <common_render.h>
#include <implot.h>
#include <implot_internal.h>
#include <algorithm>

CVDModule::CVDModule() 
    : BaseModule("CVD") 
{
}

void CVDModule::update_content(MarketData& data)
{
    auto& sData = data.get(current_symbol);
    if (sData.candles.empty()) 
    {
        ImGui::Text("Loading %s...", current_symbol.c_str());
        return;
    }

    double min_c = sData.candles[0].cvd_low;
    double max_c = sData.candles[0].cvd_high;
    for (auto& c : sData.candles) 
    {
        min_c = std::min(min_c, c.cvd_low);
        max_c = std::max(max_c, c.cvd_high);
    }

    // This ensures the scaling looks identical to the price chart
    double c_range = max_c - min_c;
    if (c_range == 0) c_range = 1.0; // Prevent division by zero
    min_c -= c_range * 0.2; 
    max_c += c_range * 0.2;

    if (ImPlot::BeginPlot("Cumulative Volume Delta", ImVec2(-1, -1)))
    {
        ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoGridLines);
        ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
        ImPlot::SetupAxis(ImAxis_Y1, nullptr, ImPlotAxisFlags_Opposite | ImPlotAxisFlags_NoGridLines);

        double latest_time = sData.candles.back().time;

        if (data.follow_live)
        {
            // Match the 300 second (5 min) window from tick_chart
            ImPlot::SetupAxisLimits(ImAxis_X1, latest_time - 300, latest_time + 30, ImPlotCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, min_c, max_c, ImPlotCond_Always);
        }
        else 
        {
            ImPlot::SetupAxisLimits(ImAxis_X1, sData.candles[0].time, sData.candles.back().time, ImPlotCond_Once);
            ImPlot::SetupAxisLimits(ImAxis_Y1, min_c, max_c, ImPlotCond_Once);
        }

        if (ImPlot::IsPlotHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) 
            data.follow_live = false;

        CommonRender::plot_candlesticks("CVD", sData.candles.data(), (int)sData.candles.size(), data.tick_timeframe * 0.8, true, data.ask_color, data.bid_color);
        
        double current_cvd = sData.candles.back().cvd_close;
        ImVec4 tag_color = (sData.candles.back().cvd_close >= sData.candles.back().cvd_open) 
            ? data.ask_color   
            : data.bid_color;
        CommonRender::draw_price_line(current_cvd, tag_color);
        
        CommonRender::draw_custom_crosshair(data.crosshair_color);

        ImPlot::EndPlot();
    }
}

void CVDModule::draw_settings_content(MarketData& data)
{
    ImGui::Text("CVD Specific Settings");
    ImGui::Checkbox("Show Moving Average", &show_ma); 
}
