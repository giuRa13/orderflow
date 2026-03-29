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