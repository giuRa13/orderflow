#include <common_render.h>
#include <implot.h>
#include <implot_internal.h>

namespace CommonRender
{
    void plot_candlesticks(const char* label_id, const TickCandle* candles, 
        int count, double width_seconds, bool is_cvd, ImVec4 bull, ImVec4 bear
    ) 
    {
        // Get the current draw list for the plot
        ImDrawList* draw_list = ImPlot::GetPlotDrawList();
        
        // We need to clip the drawing to the plot area
        ImPlot::PushPlotClipRect();

        for (int i = 0; i < count; ++i) 
        {
            const TickCandle& c = candles[i];

            double o = is_cvd ? c.cvd_open : c.open;
            double h = is_cvd ? c.cvd_high : c.high;
            double l = is_cvd ? c.cvd_low  : c.low;
            double cl = is_cvd ? c.cvd_close : c.close;

            // if in CVD mode, compare cvd_close vs cvd_open
            ImU32 color = (cl >= o) 
                ? ImGui::ColorConvertFloat4ToU32(bull) 
                : ImGui::ColorConvertFloat4ToU32(bear);

            // Convert Chart Coordinates to Screen Pixels
            ImVec2 open_pos  = ImPlot::PlotToPixels(c.time, o);
            ImVec2 close_pos = ImPlot::PlotToPixels(c.time, cl);
            ImVec2 high_pos  = ImPlot::PlotToPixels(c.time, h);
            ImVec2 low_pos   = ImPlot::PlotToPixels(c.time, l);

            // Calculate half-width in pixels
            // This ensures the candle width scales correctly when you zoom in/out
            ImVec2 width_pts = ImPlot::PlotToPixels(c.time + width_seconds * 0.5, o);
            float half_width = std::abs(width_pts.x - open_pos.x);

            // Draw Wick
            draw_list->AddLine(high_pos, low_pos, color, 1.0f);

            // Draw Body
            // Note: Min and Max points are needed for AddRectFilled
            ImVec2 rect_min = ImVec2(open_pos.x - half_width, std::min(open_pos.y, close_pos.y));
            ImVec2 rect_max = ImVec2(open_pos.x + half_width, std::max(open_pos.y, close_pos.y));
            
            // Ensure body is at least 1 pixel high even if price is flat
            if (rect_max.y - rect_min.y < 1.0f) rect_max.y = rect_min.y + 1.0f;

            draw_list->AddRectFilled(rect_min, rect_max, color);
        }

        ImPlot::PopPlotClipRect();
    }

    void draw_price_line(double price, ImVec4 color)
    {
        /*double current_price = m_candles.back().close;
        ImVec4 tag_color = (m_candles.back().close >= m_candles.back().open) 
            ? ImVec4(0.454f, 0.65f, 0.886f, 1.0f)   
            : ImVec4(0.666f, 0.227f, 0.215f, 1.0f);*/

        ImPlot::TagY(price, color, "%.2f", price);

        ImPlot::PushPlotClipRect();
        {
            ImDrawList* draw_list = ImPlot::GetPlotDrawList();
            ImU32 col32 = ImGui::ColorConvertFloat4ToU32(color);

            // get the left and right edges of the visible plot area
            ImPlotRect limits = ImPlot::GetPlotLimits();
            ImVec2 start = ImPlot::PlotToPixels(limits.X.Min, price);
            ImVec2 end   = ImPlot::PlotToPixels(limits.X.Max, price);
            // convert chart coords to screen pixels

            float dash_size = 4.0f; 
            float gap_size  = 4.0f;

            for (float x = start.x; x < end.x; x += (dash_size + gap_size))
            {
                float x_end = std::min(x + dash_size, end.x);
                draw_list->AddLine(ImVec2(x, start.y), ImVec2(x_end, start.y), col32, 1.0f);
            }
        }
        ImPlot::PopPlotClipRect();
    }

    /*void draw_bid_ask_lines(double best_bid, double best_ask, ImVec4 bid_color, ImVec4 ask_color)
    {
        if (best_bid <= 0.0 || best_ask <= 0.0) return;

        ImDrawList*  draw_list = ImPlot::GetPlotDrawList();
        ImPlotRect   limits    = ImPlot::GetPlotLimits();
        const float  dash      = 5.0f;
        const float  gap       = 4.0f;

        ImPlot::PushPlotClipRect();

        // Shaded spread zone between bid and ask
        ImVec2 spread_tl = ImPlot::PlotToPixels(limits.X.Min, best_ask);
        ImVec2 spread_br = ImPlot::PlotToPixels(limits.X.Max, best_bid);
        ImU32  spread_col = IM_COL32(200, 200, 200, 18);
        draw_list->AddRectFilled(spread_tl, spread_br, spread_col);

        // Best Bid line
        {
            ImVec2 s = ImPlot::PlotToPixels(limits.X.Min, best_bid);
            ImVec2 e = ImPlot::PlotToPixels(limits.X.Max, best_bid);
            ImU32  c = ImGui::ColorConvertFloat4ToU32(ImVec4(bid_color.x, bid_color.y, bid_color.z, 0.9f));
            for (float x = s.x; x < e.x; x += dash + gap)
                draw_list->AddLine(ImVec2(x, s.y), ImVec2(std::min(x + dash, e.x), s.y), c, 1.5f);
        }
    
        // Best Ask line
        {
            ImVec2 s = ImPlot::PlotToPixels(limits.X.Min, best_ask);
            ImVec2 e = ImPlot::PlotToPixels(limits.X.Max, best_ask);
            ImU32  c = ImGui::ColorConvertFloat4ToU32(ImVec4(ask_color.x, ask_color.y, ask_color.z, 0.9f));
            for (float x = s.x; x < e.x; x += dash + gap)
                draw_list->AddLine(ImVec2(x, s.y), ImVec2(std::min(x + dash, e.x), s.y), c, 1.5f);
        }

        // Price tags on Y axis
        ImPlot::TagY(best_bid, bid_color, "B %.2f", best_bid);
        ImPlot::TagY(best_ask, ask_color, "A %.2f", best_ask);
    
        ImPlot::PopPlotClipRect();
    }*/
    void draw_bid_ask_lines(double best_bid, double best_ask, double tick_size, ImVec4 bid_color, ImVec4 ask_color, double thickness, double x_from)
    {
        if (best_bid <= 0.0 || best_ask <= 0.0 || tick_size <= 0.0) return;
    
        // Snap to tick grid — bid line at its row, ask line exactly one tick above.
        // The one-tick gap is always the visual separation, same as bookmap.
        // Mid-point of each tick row, matching bookmap's visual convention (+ tick_size * 0.5)
        double bid_snapped = std::floor(best_bid / tick_size) * tick_size + tick_size * 0.5;
        double ask_snapped = bid_snapped + tick_size;
    
        ImDrawList*  dl     = ImPlot::GetPlotDrawList();
        ImPlotRect   limits = ImPlot::GetPlotLimits();
        const float  dash   = 6.0f;
        const float  gap    = 4.0f;
    
        ImPlot::PushPlotClipRect();
    
        auto draw_dashed = [&](double price, ImVec4 color) {
            //ImVec2 s = ImPlot::PlotToPixels(limits.X.Min, price);
            double start_x = (x_from > 0.0) ? x_from : limits.X.Min;
            ImVec2 s = ImPlot::PlotToPixels(start_x, price);
            ImVec2 e = ImPlot::PlotToPixels(limits.X.Max, price);
            ImU32  c = ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, 0.9f));
            for (float x = s.x; x < e.x; x += dash + gap)
                dl->AddLine(ImVec2(x, s.y), ImVec2(std::min(x + dash, e.x), s.y), c, thickness);
        };
    
        draw_dashed(bid_snapped, bid_color);
        draw_dashed(ask_snapped, ask_color);
    
        ImPlot::PopPlotClipRect();

        ImPlot::TagY(bid_snapped, bid_color, "B %.1f", best_bid);
        ImPlot::TagY(ask_snapped, ask_color, "A %.1f", best_ask);
    }

    void draw_custom_crosshair(ImVec4 color)
    {
        if (!ImPlot::IsPlotHovered()) return;

        ImPlotPoint mouse_pos = ImPlot::GetPlotMousePos();
        ImDrawList* draw_list = ImPlot::GetPlotDrawList();
        ImU32 col32 = ImGui::ColorConvertFloat4ToU32(color);
        ImPlotRect limits = ImPlot::GetPlotLimits();

        // Get Plot geometry to position tags manually (prevent chart shift to make place for the tag)
        ImVec2 plot_pos = ImPlot::GetPlotPos();
        ImVec2 plot_size = ImPlot::GetPlotSize();

        ImPlot::PushPlotClipRect();

        const float dash_size = 4.0f;
        const float gap_size = 4.0f;

        // Horizontal Line
        ImVec2 h_start = ImPlot::PlotToPixels(limits.X.Min, mouse_pos.y);
        ImVec2 h_end   = ImPlot::PlotToPixels(limits.X.Max, mouse_pos.y);
        for (float x = h_start.x; x < h_end.x; x += (dash_size + gap_size)) 
        {
            draw_list->AddLine(
                ImVec2(x, h_start.y),
                ImVec2(std::min(x + dash_size, h_end.x), h_start.y), 
                col32, 
                1.0f
            );
        }

        // Vertical Line
        ImVec2 v_start = ImPlot::PlotToPixels(mouse_pos.x, limits.Y.Min);
        ImVec2 v_end   = ImPlot::PlotToPixels(mouse_pos.x, limits.Y.Max);
        for (float y = v_end.y; y < v_start.y; y += (dash_size + gap_size)) 
        {
            float current_dash_end_y = std::min(y + dash_size, v_start.y);
            draw_list->AddLine(
                ImVec2(v_start.x, y), 
                ImVec2(v_start.x, current_dash_end_y), 
                col32, 
                1.0f
            );
        }

        ImPlot::PopPlotClipRect();

        // --- Y-AXIS TAG --
        ImPlot::TagY(mouse_pos.y, color, "%.2f", mouse_pos.y);

        // --- DRAW X-AXIS TAG - MANUAL VERSION TO PREVENT SHIFT ---
        char time_buf[32];
        int hrs = ((int)mouse_pos.x % 86400) / 3600;
        int mins = ((int)mouse_pos.x % 3600) / 60;
        int secs = (int)mouse_pos.x % 60;
        sprintf(time_buf, "%02d:%02d:%02d", hrs, mins, secs);

        ImVec2 text_size = ImGui::CalcTextSize(time_buf);
        ImVec2 label_padding = ImVec2(4, 2);

        // Position: Exactly at the mouse X, at the bottom of the plot area
        ImVec2 tag_center_bottom = ImVec2(v_start.x, plot_pos.y + plot_size.y);

        ImVec2 rect_min = ImVec2(tag_center_bottom.x - text_size.x / 2 - label_padding.x, tag_center_bottom.y);
        ImVec2 rect_max = ImVec2(tag_center_bottom.x + text_size.x / 2 + label_padding.x, tag_center_bottom.y + text_size.y + label_padding.y * 2);

        // Background rect
        draw_list->AddRectFilled(rect_min, rect_max, ImGui::GetColorU32(color), 0.f); // ImGui::GetColorU32(ImGuiCol_TitleBgActive)
        // Boundary line
        draw_list->AddRect(rect_min, rect_max, IM_COL32_BLACK, 2.0f);
        // Text
        draw_list->AddText(ImVec2(rect_min.x + label_padding.x, rect_min.y + label_padding.y), IM_COL32_BLACK, time_buf); // IM_COL32_WHITE
    }
}