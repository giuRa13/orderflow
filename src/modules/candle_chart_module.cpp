#include <modules/candle_chart_module.h>
#include <common_render.h>
#include <implot.h>
#include <algorithm>

CandleChartModule::CandleChartModule() 
    : BaseModule("Tick Chart") 
{
}

void CandleChartModule::update_content(MarketData& data)
{
    auto& sData = data.get(current_symbol);
    if (sData.candles.empty()) 
    {
        ImGui::Text("Loading %s...", current_symbol.c_str());
        return;
    }

    // detect Symbol Change
    bool symbol_was_just_switched = false;
    if (current_symbol != last_drawn_symbol) 
    {
        symbol_was_just_switched = true;
        last_drawn_symbol = current_symbol;
        
        data.follow_live = true; 
    }

    // calculate price bounds for Y-axis
    double min_p = sData.candles[0].low;
    double max_p = sData.candles[0].high;
    for (const auto& c : sData.candles)
    {
        if (c.low < min_p)  min_p = c.low;
        if (c.high > max_p) max_p = c.high;
    }
    double range = max_p - min_p;
    min_p -= range * 0.3; // extra padding
    max_p += range * 0.3;

    std::string plot_title = current_symbol;
    std::transform(plot_title.begin(), plot_title.end(), plot_title.begin(), ::toupper);
    plot_title += " ##Plot";
    if (ImPlot::BeginPlot(plot_title.c_str(), ImVec2(-1, -1)))
    {

        ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoGridLines);
        ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
        ImPlot::SetupAxis(ImAxis_Y1, nullptr, ImPlotAxisFlags_Opposite | ImPlotAxisFlags_NoGridLines);

        double latest_time = sData.candles.back().time;
        ImPlotCond y_condition = (data.follow_live || symbol_was_just_switched) 
                                 ? ImPlotCond_Always 
                                 : ImPlotCond_Once;
        ImPlot::SetupAxisLimits(ImAxis_Y1, min_p, max_p, y_condition);
        ImPlot::SetupAxisLimits(ImAxis_X1, latest_time - 300, latest_time + 30, y_condition);

        if (ImPlot::IsPlotHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) 
            data.follow_live = false;

        CommonRender::plot_candlesticks("ETH/USDT", sData.candles.data(), (int)sData.candles.size(), data.tick_timeframe * 0.8, false, data.ask_color, data.bid_color);

        double current_price = sData.candles.back().close;
        ImVec4 tag_color = (sData.candles.back().close >= sData.candles.back().open) 
            ? data.ask_color   
            : data.bid_color;
        CommonRender::draw_price_line(current_price, tag_color);
        
        CommonRender::draw_custom_crosshair(data.crosshair_color);

        ImPlot::EndPlot();
    }
}

void CandleChartModule::draw_settings_content(MarketData& data)
{
    ImGui::Text("Chart Specific Settings");
    ImGui::Checkbox("Show Moving Average", &show_ma); 
}

/*void CandleChartModule::render(MarketData& data)
{
    if (!is_open) return;

    if (data.candles.empty()) 
    {
        ImGui::Begin("Tick Chart", &is_open);
        ImGui::Text("Awaiting Data...");
        ImGui::End();
        return;
    }

    ImGui::Begin("Tick Chart", &is_open);

    // calculate price bounds for Y-axis
    double min_p = data.candles[0].low;
    double max_p = data.candles[0].high;
    for (const auto& c : data.candles)
    {
        if (c.low < min_p)  min_p = c.low;
        if (c.high > max_p) max_p = c.high;
    }
    double range = max_p - min_p;
    min_p -= range * 0.3; // extra padding
    max_p += range * 0.3;

    if (ImPlot::BeginPlot("##CandlePlot", ImVec2(-1, -1)))
    {
        ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
        ImPlot::SetupAxis(ImAxis_Y1, nullptr, ImPlotAxisFlags_Opposite);

        double latest_time = data.candles.back().time;
        if (data.follow_live)
        {
            // only the last 5 minutes
            ImPlot::SetupAxisLimits(ImAxis_X1, latest_time - 300, latest_time + 30, ImPlotCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, min_p, max_p, ImPlotCond_Always);
        }
        else 
        {
            // This allows the user to click and drag (manual scroll)
            ImPlot::SetupAxisLimits(ImAxis_X1, data.candles[0].time, data.candles.back().time, ImPlotCond_Once);
            ImPlot::SetupAxisLimits(ImAxis_Y1, min_p, max_p, ImPlotCond_Once);
        }

        if (ImPlot::IsPlotHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) 
            data.follow_live = false;

        CommonRender::plot_candlesticks("ETH/USDT", data.candles.data(), (int)data.candles.size(), data.tick_timeframe * 0.8, false, data.ask_color, data.bid_color);

        double current_price = data.candles.back().close;
        ImVec4 tag_color = (data.candles.back().close >= data.candles.back().open) 
            ? data.ask_color   
            : data.bid_color;
        CommonRender::draw_price_line(current_price, tag_color);
        
        CommonRender::draw_custom_crosshair(data.crosshair_color);

        ImPlot::EndPlot();
    }

    ImGui::End();
}*/


/*void Application::candlestick_chart()
{
    ImGui::Begin("Binance");

    // 1. Lock data to prevent the background thread from changing it while we draw
    std::lock_guard<std::recursive_mutex> lock(m_data_mtx);

    if (m_candles.empty()) 
    {
        ImGui::Text("Connecting to Binance...");
        ImGui::End();
        return;
    } 

    // 1. MANUAL PRICE CALCULATION (Since DrawList bypasses AutoFit)
    double min_p = m_candles[0].low;
    double max_p = m_candles[0].high;
    for (const auto& c : m_candles) 
    {
        if (c.low < min_p)  min_p = c.low;
        if (c.high > max_p) max_p = c.high;
    }

    // Add 30% padding so candles don't touch the top/bottom
    double range = max_p - min_p;
    min_p -= range * 0.3;
    max_p += range * 0.3;

    // ImPlotAxisFlags_NoGridLines, ImPlotAxisFlags_NoLabel
    ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0,0));
    ImPlot::PushStyleVar(ImPlotStyleVar_FitPadding, ImVec2(0.1f, 0.1f));
    if (ImPlot::BeginPlot("ETH/USDT Chart", ImVec2(-1, -1))) 
    { 
        // SETUP AXES
        ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
        //ImPlot::SetupAxis(ImAxis_Y1, "Price");
        ImPlot::SetupAxis(ImAxis_Y1, nullptr, ImPlotAxisFlags_Opposite); 

        if (follow_live) 
        {
            double latest_time = m_candles.back().time;
            // Show last 20 minutes (1200 seconds)
            ImPlot::SetupAxisLimits(ImAxis_X1, latest_time - 1200, latest_time + 30, ImPlotCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, min_p, max_p, ImPlotCond_Always);
        }
        else 
        {
            // This allows the user to click and drag (manual scroll)
            ImPlot::SetupAxisLimits(ImAxis_X1, m_candles[0].time, m_candles.back().time, ImPlotCond_Once);
            ImPlot::SetupAxisLimits(ImAxis_Y1, min_p, max_p, ImPlotCond_Once);
        }

        // if user starts dragging the plot with the mouse, automatically turn off "follow_live"
        if (ImPlot::IsPlotHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) 
            follow_live = false;

        plot_candlesticks("ETHUSDT", m_candles.data(), (int)m_candles.size(), 40.0, false);

        double current_price = m_candles.back().close;
        ImVec4 tag_color = (m_candles.back().close >= m_candles.back().open) 
            ? ask_color   
            : bid_color;
        draw_price_line(current_price, tag_color);

        draw_custom_crosshair(crosshair_color);

        ImPlot::EndPlot();
    }
    ImPlot::PopStyleVar();

    ImGui::End();
}*/