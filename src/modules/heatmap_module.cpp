#include <modules/heatmap_module.h>
#include <common_render.h>
#include <implot.h>
#include <implot_internal.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <vector>
 
HeatmapModule::HeatmapModule()
    : BaseModule("Heatmap")
{
}
 
// ─── DATA ────────────────────────────────────────────────────────────────────

void HeatmapModule::sample_bid_ask(double bid, double ask, double now)
{
    if (bid <= 0.0 || ask <= 0.0) return;
    if (now - m_last_sample_time < m_sample_rate_ms / 1000.0) return;
    m_last_sample_time = now;
    m_history.push_back({now, bid, ask});
    double cutoff = now - (double)(m_max_history_hrs * 3600.0f);
    while (!m_history.empty() && m_history.front().time < cutoff)
        m_history.pop_front();
}
 
void HeatmapModule::ingest_trades(const SymbolData& sData)
{
    if (sData.tape.empty()) return;
 
    // keep time offset in sync so tape Unix timestamps → glfwGetTime space
    m_time_offset = glfwGetTime() - sData.tape.front().time;

    // tape[0]=newest, tape[size-1]=oldest — walk newest→oldest, stop at already-processed
    std::vector<TradePoint> new_trades;
    for (int i = 0; i < (int)sData.tape.size(); i++)
    {
        const auto& t = sData.tape[i];
        if (t.time <= m_last_processed_tape_time) break;
        if (t.quantity >= (double)m_min_bubble_size)
            new_trades.push_back({t.time + m_time_offset, t.price, t.quantity, t.is_sell});
    }
    // Insert in chronological order (oldest first) to keep deque sorted
    for (int i = (int)new_trades.size() - 1; i >= 0; i--)
        m_trade_history.push_back(new_trades[i]);

    m_last_processed_tape_time = sData.tape.front().time;

    // trim to max history
    double cutoff = glfwGetTime() - (double)(m_max_history_hrs * 3600.0f);
    while (!m_trade_history.empty() && m_trade_history.front().glfw_time < cutoff)
        m_trade_history.pop_front();
}

// ─── AXES SETUP ──────────────────────────────────────────────────────────────

void HeatmapModule::setup_axes(double y_min, double y_max, double y_range, float pre_plot_h)
{
    ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoGridLines);
    ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
    ImPlot::SetupAxis(ImAxis_Y1, nullptr, ImPlotAxisFlags_Opposite | ImPlotAxisFlags_NoGridLines);

    // Y label interval: scale up in steps until labels are ≥18px apart and ≤200 total.
    // Must be called before SetupAxisLimits to avoid locking the setup phase.
    {
        static const double mults[] = {1,2,5,10,20,50,100,200,500,1000};
        double interval = (double)m_tick_size;
        for (double m : mults)
        {
            interval    = (double)m_tick_size * m;
            float  px   = (y_range > 0.0 && pre_plot_h > 0.0f)
                          ? (float)(pre_plot_h * interval / y_range) : 999.0f;
            int    n    = (int)((y_range + 2.0 * interval) / interval) + 1;
            if (px >= 18.0f && n <= 200) break;
        }
        double t0 = std::floor((y_min - interval) / interval) * interval;
        int    n  = std::clamp((int)((y_max - t0 + interval) / interval) + 1, 2, 200);
        static std::vector<double> tv;
        tv.resize(n);
        for (int i = 0; i < n; i++) tv[i] = t0 + i * interval;
        ImPlot::SetupAxisTicks(ImAxis_Y1, tv.data(), n, nullptr, false);
    }
 
    ImPlot::SetupAxisLimits(ImAxis_X1, m_x_min, m_x_max, ImPlotCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, y_min,   y_max,   ImPlotCond_Always);
}

// ─── INTERACTION ─────────────────────────────────────────────────────────────
 
void HeatmapModule::handle_interaction(double y_range)
{
    bool hovered = ImPlot::IsPlotHovered();
 
    // Scroll: Y axis area → Y zoom, everywhere else → X zoom
    float wheel = ImGui::GetIO().MouseWheel;
    if (wheel != 0.0f)
    {
        ImVec2 plot_pos  = ImPlot::GetPlotPos();
        ImVec2 plot_size = ImPlot::GetPlotSize();
        ImVec2 mouse     = ImGui::GetMousePos();
 
        bool near_plot = mouse.x >= plot_pos.x - 10.0f
                      && mouse.x <= plot_pos.x + plot_size.x + 80.0f
                      && mouse.y >= plot_pos.y - 20.0f
                      && mouse.y <= plot_pos.y + plot_size.y + 40.0f;
 
        if (near_plot)
        {
            ImGui::GetIO().MouseWheel = 0.0f;
            float factor = (wheel > 0.0f) ? 0.8f : 1.25f;
            bool  over_y = mouse.x > plot_pos.x + plot_size.x;
 
            if (over_y)
            {
                m_visible_rows = (int)std::clamp((float)m_visible_rows * factor, 5.0f, 2000.0f);
            }
            else
            {
                double cx        = (m_x_min + m_x_max) * 0.5;
                double half      = (m_x_max - m_x_min) * 0.5 * (double)factor;
                double new_x_min = cx - half;
                double new_x_max = cx + half;
 
                if (m_follow_price)
                    m_time_window_sec = std::clamp(
                        (float)(new_x_max - new_x_min) - m_right_pad_sec,
                        5.0f, m_max_history_hrs * 3600.0f);
                else
                { m_x_min = new_x_min; m_x_max = new_x_max; }
            }
        }
    }
 
    // Drag: disable follow, pan both axes
    if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        if (m_follow_price) m_follow_price = false;
 
        ImVec2 curr  = ImGui::GetMousePos();
        ImVec2 psize = ImPlot::GetPlotSize();
        if (m_prev_mouse.x >= 0.0f && psize.x > 0 && psize.y > 0)
        {
            double dx = (curr.x - m_prev_mouse.x) / (psize.x / (m_x_max - m_x_min));
            double dy = (curr.y - m_prev_mouse.y) / (psize.y / y_range);
            m_x_min    -= dx;
            m_x_max    -= dx;
            m_y_center += dy;
        }
        m_prev_mouse = curr;
    }
    else
    {
        m_prev_mouse = {-1.0f, -1.0f};
    }
}

// ─── RENDERING ───────────────────────────────────────────────────────────────

void HeatmapModule::draw_grid(double y_range)
{
    float plot_h     = ImPlot::GetPlotSize().y;
    float px_per_tick = (y_range > 0.0) ? (float)(plot_h * (double)m_tick_size / y_range) : 0.0f;
    if (px_per_tick < 4.0f) return;
 
    ImDrawList* dl  = ImPlot::GetPlotDrawList();
    ImU32       col = ImGui::ColorConvertFloat4ToU32(tick_grid_color);
    ImPlotRect  lim = ImPlot::GetPlotLimits();
    float       x0  = ImPlot::PlotToPixels(lim.X.Min, 0).x;
    float       x1  = ImPlot::PlotToPixels(lim.X.Max, 0).x;
    double      g0  = std::floor(lim.Y.Min / (double)m_tick_size) * (double)m_tick_size;
 
    ImPlot::PushPlotClipRect();
    for (double p = g0; p <= lim.Y.Max + (double)m_tick_size; p += (double)m_tick_size)
    {
        float py = ImPlot::PlotToPixels(0, p).y;
        dl->AddLine(ImVec2(x0, py), ImVec2(x1, py), col, 1.0f);
    }
    ImPlot::PopPlotClipRect();
}
 
void HeatmapModule::draw_trail()
{
    if (m_history.size() < 2) return;
 
    ImDrawList* dl      = ImPlot::GetPlotDrawList();
    ImU32       ask_col = ImGui::ColorConvertFloat4ToU32(ImVec4(m_bid_color.x, m_bid_color.y, m_bid_color.z, 0.85f));
    ImU32       bid_col = ImGui::ColorConvertFloat4ToU32(ImVec4(m_ask_color.x, m_ask_color.y, m_ask_color.z, 0.85f));
    double      tick    = (double)m_tick_size;
 
    ImPlot::PushPlotClipRect();
    for (size_t i = 1; i < m_history.size(); i++)
    {
        auto& prev = m_history[i - 1];
        auto& curr = m_history[i];
        if (curr.time < m_x_min || prev.time > m_x_max) continue;
 
        double bid_p = std::floor(prev.bid / tick) * tick + tick * 0.5;
        double bid_c = std::floor(curr.bid / tick) * tick + tick * 0.5;
        double ask_p = bid_p + tick;
        double ask_c = bid_c + tick;
 
        // Horizontal segments
        dl->AddLine(ImPlot::PlotToPixels(prev.time, bid_p), ImPlot::PlotToPixels(curr.time, bid_p), bid_col, m_lines_thickness);
        dl->AddLine(ImPlot::PlotToPixels(prev.time, ask_p), ImPlot::PlotToPixels(curr.time, ask_p), ask_col, m_lines_thickness);
 
        // Vertical connectors on row change
        if (bid_p != bid_c)
        {
            dl->AddLine(ImPlot::PlotToPixels(curr.time, bid_p), ImPlot::PlotToPixels(curr.time, bid_c), bid_col, m_lines_thickness);
            dl->AddLine(ImPlot::PlotToPixels(curr.time, ask_p), ImPlot::PlotToPixels(curr.time, ask_c), ask_col, m_lines_thickness);
        }
    }
    ImPlot::PopPlotClipRect();
}
 
void HeatmapModule::draw_live_lines(const SymbolData& sData)
{
    double x_from = m_extend_lines_full ? -1.0
                  : (!m_history.empty() ? m_history.back().time : -1.0);
    CommonRender::draw_bid_ask_lines(sData.last_best_bid, sData.last_best_ask,
                                     (double)m_tick_size, m_ask_color, m_bid_color, m_lines_thickness, x_from);
}

void HeatmapModule::draw_bubbles()
{
    if (m_trade_history.empty()) return;

    ImDrawList* dl       = ImPlot::GetPlotDrawList();
    ImU32       buy_col  = ImGui::ColorConvertFloat4ToU32(ImVec4(m_ask_color.x, m_ask_color.y, m_ask_color.z, 0.80f));
    ImU32       sell_col = ImGui::ColorConvertFloat4ToU32(ImVec4(m_bid_color.x, m_bid_color.y, m_bid_color.z, 0.80f));
    double      tick     = (double)m_tick_size;

    ImPlot::PushPlotClipRect();
    for (auto& t : m_trade_history)
    {
        if (t.glfw_time < m_x_min || t.glfw_time > m_x_max) continue;
 
        // Snap to tick row midpoint
        double price_y = std::floor(t.price / tick) * tick + tick * 0.5;
        ImVec2 center  = ImPlot::PlotToPixels(t.glfw_time, price_y);
 
        // Radius: sqrt scaling so a 4x trade is 2x bigger, not 4x
        float radius = std::min(
            (float)std::sqrt(t.qty / (double)m_min_bubble_size) * m_bubble_size,
            m_bubble_size * m_bubble_ratio);
        if (radius < 1.5f) radius = 1.5f;
 
        ImU32 col = t.is_sell ? sell_col : buy_col;
        dl->AddCircleFilled(center, radius, col);
        // Thin border for small bubbles to keep them visible
        dl->AddCircle(center, radius, ImGui::ColorConvertFloat4ToU32(
            ImVec4(t.is_sell ? m_bid_color.x : m_ask_color.x,
                   t.is_sell ? m_bid_color.y : m_ask_color.y,
                   t.is_sell ? m_bid_color.z : m_ask_color.z, 0.5f)), 0, 1.0f);
    }
    ImPlot::PopPlotClipRect();
}

// ─── MAIN UPDATE ─────────────────────────────────────────────────────────────
 
void HeatmapModule::update_content(MarketData& data)
{
    auto& sData = data.get(current_symbol);
    if (!sData.snapshot_loaded || sData.last_best_bid <= 0.0)
    {
        ImGui::Text("Waiting for market data..."); return;
    }
 
    double now = glfwGetTime();
    sample_bid_ask(sData.last_best_bid, sData.last_best_ask, now);
    ingest_trades(sData);
 
    // Symbol change — clear stale history
    if (current_symbol != m_last_symbol)
    {
        m_last_symbol      = current_symbol;
        m_follow_price     = true;
        m_history.clear();
        m_last_sample_time = 0.0;
        m_last_processed_tape_time   = 0.0;
        m_x_min = m_x_max = m_y_center = 0.0;
    }
 
    // Cache colors for sub-functions
    m_bid_color = data.bid_color;
    m_ask_color = data.ask_color;
 
    double mid     = (sData.last_best_bid + sData.last_best_ask) * 0.5;
    double y_range = (double)m_visible_rows * (double)m_tick_size;
 
    // First frame init
    if (m_x_min == 0.0 && m_x_max == 0.0)
    {
        m_x_min    = now - m_time_window_sec;
        m_x_max    = now + m_right_pad_sec;
        m_y_center = mid;
    }
 
    if (m_follow_price)
    {
        m_x_min = now - m_time_window_sec;
        m_x_max = now + m_right_pad_sec;
        if (m_y_center == 0.0 || std::abs(mid - m_y_center) > y_range * 0.38)
            m_y_center = mid;
    }
 
    double y_min = m_y_center - y_range * 0.5;
    double y_max = m_y_center + y_range * 0.5;
 
    float pre_plot_h = ImGui::GetContentRegionAvail().y;
 
    std::string plot_id = current_symbol + "##heatmap";
    if (!ImPlot::BeginPlot(plot_id.c_str(), ImVec2(-1, -1))) return;
 
    setup_axes(y_min, y_max, y_range, pre_plot_h);
    handle_interaction(y_range);
    draw_grid(y_range);
    if (m_show_bubbles)      draw_bubbles();
    if (m_show_best_bid_ask) { draw_trail(); draw_live_lines(sData); }
    if (m_show_crosshair) CommonRender::draw_custom_crosshair(data.crosshair_color);
 
    ImPlot::EndPlot();
}

// ─── SETTINGS ────────────────────────────────────────────────────────────────
 
void HeatmapModule::draw_settings_content(MarketData& data)
{
    ImGui::Spacing();
    ImGui::TextDisabled("Tick Size (row height)");
    static const char* tick_labels[] = { "0.01","0.05","0.10","0.50","1.0","5.0","10.0","50.0","100.0" };
    static float       tick_values[] = { 0.01f, 0.05f, 0.10f, 0.50f, 1.0f, 5.0f, 10.0f, 50.0f, 100.0f };
    int cur_tick = 4;
    for (int i = 0; i < IM_ARRAYSIZE(tick_values); i++)
        if (m_tick_size == tick_values[i]) { cur_tick = i; break; }
    ImGui::SetNextItemWidth(160);
    if (ImGui::Combo("Tick Size", &cur_tick, tick_labels, IM_ARRAYSIZE(tick_labels)))
    {
        m_tick_size    = tick_values[cur_tick];
        m_follow_price = true;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Price per row.\nBTC futures: 5.0  |  ETH: 0.5 or 1.0");
 
    ImGui::SetNextItemWidth(160);
    ImGui::SliderInt("Visible Rows", &m_visible_rows, 5, 500);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Scroll over Y axis to change live. Fewer = more zoom.");
    ImGui::Text("Visible range: %.2f", (float)m_visible_rows * m_tick_size);
 
    ImGui::Separator(); 
    ImGui::Spacing();
    ImGui::TextDisabled("Time Window");
    ImGui::SetNextItemWidth(160);
    ImGui::SliderFloat("Window (sec)",      &m_time_window_sec, 10.0f,  600.0f, "%.0f s");
    ImGui::SetNextItemWidth(160);
    ImGui::SliderFloat("Right Pad (sec)",   &m_right_pad_sec,    0.0f,   60.0f, "%.0f s");
    ImGui::SetNextItemWidth(160);
    ImGui::SliderFloat("Max History (hrs)", &m_max_history_hrs,  0.5f,    8.0f, "%.1f h");
    ImGui::SetNextItemWidth(160);
    ImGui::SliderFloat("Sample Rate (ms)",  &m_sample_rate_ms,  50.0f, 1000.0f, "%.0f ms");

    ImGui::Separator(); ImGui::Spacing();
    ImGui::TextDisabled("Bubbles");
    ImGui::Checkbox("Show Bubbles", &m_show_bubbles);
    ImGui::BeginDisabled(!m_show_bubbles);
    ImGui::SetNextItemWidth(160);
    ImGui::SliderFloat("Min Size",       &m_min_bubble_size, 0.1f,  50.0f, "%.1f");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Minimum trade qty to draw a bubble.");
    ImGui::SetNextItemWidth(160);
    ImGui::SliderFloat("Bubble Size (px)", &m_bubble_size,  1.0f,  20.0f, "%.0f px");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Bubble Size (base radius for minimum-size trades).");
    ImGui::SetNextItemWidth(160);
    ImGui::SliderFloat("Size Ratio",  &m_bubble_ratio,   2.0f,  20.0f, "%.0f px");
    if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Max radius = size x ratio.");
    ImGui::EndDisabled();
 
    ImGui::Separator(); 
    ImGui::Spacing();
    ImGui::TextDisabled("Display");
    ImGui::Checkbox("Show Best Bid/Ask", &m_show_best_bid_ask);
    ImGui::SetNextItemWidth(160);
    ImGui::SliderFloat("Best bid/Ask Thickness",  &m_lines_thickness,   1.5f,  4.0f, "%.1f");
    ImGui::BeginDisabled(!m_show_best_bid_ask);
    ImGui::Checkbox("Extend Lines Full Width", &m_extend_lines_full);
    ImGui::EndDisabled();
    ImGui::Checkbox("Show Crosshair",             &m_show_crosshair);
    ImGui::Checkbox("Follow Price", &m_follow_price);
    ImGui::ColorEdit4("Tick Grid", &tick_grid_color.x);
 
    ImGui::Separator(); 
    ImGui::Spacing();
    if (ImGui::Button("Clear History", ImVec2(-1, 0)))
    {
        m_history.clear();
        m_trade_history.clear();
        m_last_sample_time         = 0.0;
        m_last_processed_tape_time = 0.0;
    }
}

void HeatmapModule::render_module_specific_header(MarketData& data)
{
    const float slim_pad = 1.0f;
    const float slim_h   = ImGui::GetTextLineHeight() + slim_pad * 2.0f;
    // Buttons: just SameLine — cursor Y already correct from common header's v_off
    ImGui::SameLine(0, 70);
    ImGui::TextDisabled("|");
    ImGui::SameLine(0, 20);
    bool was_follow = m_follow_price;
    if (was_follow) 
    {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.988f, 0.196f, 0.368f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.988f, 0.196f, 0.368f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.988f, 0.196f, 0.368f, 0.6f));
    }

    if (ImGui::Button(m_follow_price ? ICON_FA_LOCK : ICON_FA_LOCK_OPEN, ImVec2(30, 0))) 
    {
        m_follow_price = !m_follow_price;
        if (m_follow_price) m_x_min = m_x_max = 0.0; // Reset view to snap to live
    }
    if (was_follow) ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(m_follow_price ? "Auto-Follow: ON" : "Auto-Follow: OFF (Free Pan)");

    // CLEAR HISTORY
    ImGui::SameLine(0, 5);
    if (ImGui::Button(ICON_FA_TRASH, ImVec2(30, 0))) 
    {
        m_history.clear();
        m_trade_history.clear();
        m_last_sample_time         = 0.0;
        m_last_processed_tape_time = 0.0;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear Heatmap History");

    // QUICK TICK SIZE SHORTCUT
    ImGui::SameLine(0, 20); 
    ImGui::TextDisabled("|");
    ImGui::SameLine(0, 20); 
    static const char* tick_labels[] = { "0.01","0.05","0.10","0.50","1.0","5.0","10.0","50.0","100.0" };
    static float       tick_values[] = { 0.01f, 0.05f, 0.10f, 0.50f, 1.0f, 5.0f, 10.0f, 50.0f, 100.0f };
    int cur_tick = 4; // default 1.0
    for (int i = 0; i < IM_ARRAYSIZE(tick_values); i++)
        if (m_tick_size == tick_values[i]) { cur_tick = i; break; }
    ImGui::SetNextItemWidth(90);
    if (ImGui::Combo("##TickSizeTop", &cur_tick, tick_labels, IM_ARRAYSIZE(tick_labels)))
    {
        m_tick_size    = tick_values[cur_tick];
        m_follow_price = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Tick Size");

    // BUBBLES SETTINGS (slim, stacked, naturally at top_y)
    ImGui::SameLine(0, 20); 
    ImGui::TextDisabled("|");
    ImGui::SameLine(0, 20); 
    //ImGui::SetCursorPosY(m_header_top_y + 2);
    //ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, slim_pad));
    float gx       = ImGui::GetCursorPosX();
    float icon_w   = ImGui::CalcTextSize(ICON_FA_CIRCLE).x + 4.0f;
    float slider_w = 110.0f;
    float row2_y   = m_header_top_y + slim_h + ImGui::GetStyle().ItemSpacing.y;

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, slim_pad));
    ImGui::BeginGroup();
    // Row 1: icon + size slider — absolutely positioned
        ImGui::SetCursorPos({gx, m_header_top_y});
        ImGui::TextDisabled(ICON_FA_CIRCLE);
        ImGui::SetCursorPos({gx + icon_w, m_header_top_y});
        ImGui::SetNextItemWidth(slider_w);
        ImGui::SliderFloat("##BubSize", &m_bubble_size, 1.0f, 20.0f, "%.0f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Bubble Size");

        // Row 2: icon + ratio slider — absolutely positioned
        ImGui::SetCursorPos({gx, row2_y});
        ImGui::TextDisabled(ICON_FA_EXPAND_ALT);
        ImGui::SetCursorPos({gx + icon_w, row2_y});
        ImGui::SetNextItemWidth(slider_w);
        ImGui::SliderFloat("##BubRatio", &m_bubble_ratio, 2.0f, 20.0f, "%.0fx");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Size Ratio");
    ImGui::EndGroup();
    ImGui::PopStyleVar();

    ImGui::SameLine(0, 20); 
    ImGui::TextDisabled("|");
}
