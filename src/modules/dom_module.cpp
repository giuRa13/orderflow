#include <modules/dom_module.h>
#include <GLFW/glfw3.h>
#include <map>
#include <math.h>

DOMModule::DOMModule() 
    : BaseModule("DOM") 
{
}

void DOMModule::update_content(MarketData& data)
{
    auto& sData = data.get(current_symbol);
    if (!sData.snapshot_loaded || sData.full_asks.empty()) 
    {
        m_needs_recenter = true;
        ImGui::Text("Loading L2 Data..."); return;
    }

   if (current_symbol != m_last_symbol || m_anchor_bucket == 0.0) 
    {
        m_needs_recenter = true;
        m_last_symbol = current_symbol;
        double start_p = sData.tape.empty() ? sData.full_asks.rbegin()->first : sData.tape[0].price;
        m_anchor_bucket = std::floor(start_p / data.dom_step) * data.dom_step;
    }

    double current_time = glfwGetTime();
    double step = data.dom_step;
    double last_price = sData.tape.empty() ? sData.last_best_bid : sData.tape[0].price;
    double live_bucket = std::floor(last_price / step) * step;
    const int VIEW_RANGE = 50;

    double row_distance = std::abs(live_bucket - m_anchor_bucket) / step;

    if (m_needs_recenter || row_distance > 40.0 || 
       (m_auto_scroll && !ImGui::IsWindowHovered() && (current_time - m_last_recenter_time >= m_scroll_interval))) 
    {
        float row_h = ImGui::GetTextLineHeightWithSpacing();
        if (!m_needs_recenter) 
        {
            double shift_rows = (live_bucket - m_anchor_bucket) / step;
            m_current_visual_scroll += (float)(shift_rows * row_h);
        }
        m_anchor_bucket = live_bucket;
        sData.dom_dirty = true;
        m_last_recenter_time = current_time;
    }

    /*if (ImGui::IsWindowHovered() && (ImGui::GetIO().MouseWheel != 0 || ImGui::IsMouseDown(0))) 
    {
        m_last_recenter_time = current_time;
    }*/

    // Aggregate volume into sData maps (Persistent)
    if (sData.dom_dirty) 
    {
        sData.ask_sums.clear();
        sData.bid_sums.clear();
        for (auto const& [rp, rq] : sData.full_asks) sData.ask_sums[std::floor(rp / step) * step] += rq;
        for (auto const& [rp, rq] : sData.full_bids) sData.bid_sums[std::floor(rp / step) * step] += rq;
        
        sData.cached_max_vol = 0.0001;
        for (auto const& [p, q] : sData.ask_sums) if (q > sData.cached_max_vol) sData.cached_max_vol = q;
        for (auto const& [p, q] : sData.bid_sums) if (q > sData.cached_max_vol) sData.cached_max_vol = q;
        
        sData.dom_dirty = false;
    }

    float max_scroll = ImGui::GetScrollMaxY();
    float base_center = max_scroll * 0.5f; 
    float row_h = ImGui::GetTextLineHeightWithSpacing();

    if (m_auto_scroll && !ImGui::IsWindowHovered() && !m_needs_recenter) 
    {
        float dt = ImGui::GetIO().DeltaTime;
        double row_drift = (live_bucket - m_anchor_bucket) / step;
        float target_scroll = base_center - (float)(row_drift * row_h);

        float lerp_factor = 1.0f - expf(-m_scroll_smoothing * dt); 
        m_current_visual_scroll += (target_scroll - m_current_visual_scroll) * lerp_factor;
        ImGui::SetScrollY(m_current_visual_scroll);
    } 
    else if (!m_needs_recenter) 
    {
        m_current_visual_scroll = ImGui::GetScrollY();
    }

    ImU32 ask_bg = ImGui::ColorConvertFloat4ToU32(ImVec4(ask_bg_color.x, ask_bg_color.y, ask_bg_color.z, 0.45f));
    ImU32 bid_bg = ImGui::ColorConvertFloat4ToU32(ImVec4(bid_bg_color.x, bid_bg_color.y, bid_bg_color.z, 0.45f));
    ImU32 price_bg = ImGui::ColorConvertFloat4ToU32(ImVec4(price_highlight.x, price_highlight.y, price_highlight.z, 1.0f));

    static ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg | 
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | 
        ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;

    // TOP UI
    render_top_ui(data, sData);

    // --- DOM TABLE -------------------------------------- 
    if (ImGui::BeginTable("##dom", col_number, flags, ImVec2(0, -1))) 
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthStretch, 0.20f);
        ImGui::TableSetupColumn("Bid",   ImGuiTableColumnFlags_WidthStretch, 0.25f);
        ImGui::TableSetupColumn("Sells", ImGuiTableColumnFlags_WidthStretch, 0.15f);
        ImGui::TableSetupColumn("Buys",  ImGuiTableColumnFlags_WidthStretch, 0.15f);
        ImGui::TableSetupColumn("Ask",   ImGuiTableColumnFlags_WidthStretch, 0.25f);
        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
        for (int i = 0; i < 5; i++)
        {
            ImGui::TableSetColumnIndex(i);
            const char* name = ImGui::TableGetColumnName(i);
            float off = (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(name).x) * 0.5f;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);
            ImGui::TableHeader(name);
        }

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 line_pos(0,0); 
        bool line_ready = false;

        for (int i = VIEW_RANGE; i >= -VIEW_RANGE; i--) 
        {
            double p = m_anchor_bucket + (i * step);
            ImGui::TableNextRow();

            if (m_needs_recenter && i == 0) 
            {
                ImGui::SetScrollHereY(0.5f);
                m_current_visual_scroll = ImGui::GetScrollY();
                m_needs_recenter = false;
            }

            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, bid_bg, 1);
            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ask_bg, 4);

            // Column 0 (Price)
            ImGui::TableNextColumn();
            if (std::abs(p - live_bucket) < (step * 0.1)) 
            {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, price_bg, 0);
                line_pos = ImGui::GetCursorScreenPos();
                line_ready = true;
            }

            char p_buf[32]; 
            sprintf(p_buf, "%.2f", p);
            float off = (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(p_buf).x) * 0.5f;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);
            ImGui::TextColored(price_color, "%s", p_buf);

            if (p > live_bucket) 
            {
                for(int c=0; c<4; c++) ImGui::TableNextColumn();
                render_dom_bar(sData.ask_sums[p], sData.cached_max_vol, data.bid_color, right_to_left_ask, center_values_ask);
            } 
            else 
            {
                ImGui::TableNextColumn();
                render_dom_bar(sData.bid_sums[p], sData.cached_max_vol, data.ask_color, right_to_left_bid, center_values_bid);
                for(int c=0; c<3; c++) ImGui::TableNextColumn();
            }
        }
        
        if (line_ready) 
        {
            float x_start = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMin().x;
            float x_end   = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
            
            // Draw the line at the captured Y position
            draw_list->AddLine(
                ImVec2(x_start, line_pos.y), 
                ImVec2(x_end, line_pos.y), 
                price_bg, 
                2.5f
            );
        }

        ImGui::EndTable();
    }
}

void DOMModule::render_dom_bar(double qty, double max_vol, ImVec4 color, bool right_to_left, bool center_text)
{
    float height = ImGui::GetTextLineHeightWithSpacing();
    ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x, height);
    ImVec2 pos = ImGui::GetCursorScreenPos();

    ImGui::Dummy(size);
    if (qty <= 0.0000001) return; 
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    float fraction = (float)(qty / max_vol);
    if (fraction < 0.002f) fraction = 0.002f;
    if (fraction > 1.0f)   fraction = 1.0f;
    float bar_width = size.x * fraction;

    ImVec2 bar_min, bar_max;
    if (right_to_left) 
    {
        bar_min = ImVec2(pos.x + size.x - bar_width, pos.y);
        bar_max = ImVec2(pos.x + size.x, pos.y + size.y);
    } 
    else 
    {
        bar_min = pos;
        bar_max = ImVec2(pos.x + bar_width, pos.y + size.y);
    }

    ImU32 bar_col = ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, 0.75f));
    draw_list->AddRectFilled(bar_min, bar_max, bar_col);

    char buf[32];
    sprintf(buf, "%.2f", qty);
    ImVec2 text_size = ImGui::CalcTextSize(buf);
    float text_x = 0.0f;
    
    if (center_text) 
        text_x = pos.x + (size.x - text_size.x) * 0.5f;
    else 
        text_x = right_to_left ? (pos.x + size.x - text_size.x - 5.0f) : (pos.x + 5.0f);

    float text_y = pos.y + (size.y - text_size.y) * 0.5f;
    draw_list->AddText(ImVec2(text_x, text_y), IM_COL32_WHITE, buf);
}

void DOMModule::render_top_ui(MarketData& data, SymbolData& sData)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 3));
    
    // LEFT SIDE
    static const char* step_labels[] = { "0.1", "0.2", "0.5", "1.0", "5.0", "10.0" };
    static double step_values[]      = { 0.1,   0.2,   0.5,   1.0,   5.0,   10.0 };
    int current_step_idx = 4;
    for (int n = 0; n < IM_ARRAYSIZE(step_values); n++) 
        if (data.dom_step == step_values[n]) { current_step_idx = n; break; }

    ImGui::SetNextItemWidth(126.0f); 
    if (ImGui::Combo("##StepTop", &current_step_idx, step_labels, IM_ARRAYSIZE(step_labels))) 
    {
        data.dom_step = step_values[current_step_idx];
        sData.dom_dirty = true;
        m_needs_recenter = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Price Grouping Step");

    // RIGHT SIDE
    ImGui::SameLine();
    float buttons_area_width = 72.0f; 
    float right_pos = ImGui::GetWindowWidth() - buttons_area_width - ImGui::GetStyle().WindowPadding.x;
    if (ImGui::GetScrollMaxY() > 0) right_pos -= ImGui::GetStyle().ScrollbarSize;
    ImGui::SetCursorPosX(right_pos);

    bool was_auto_scroll_active = m_auto_scroll; 
    if (was_auto_scroll_active) 
    {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.988f, 0.196f, 0.368f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.988f, 0.196f, 0.368f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.988f, 0.196f, 0.368f, 0.6f));
    }

    if (ImGui::Button(m_auto_scroll ? ICON_FA_LOCK : ICON_FA_LOCK_OPEN, ImVec2(32, 0))) 
    {
        m_auto_scroll = !m_auto_scroll;
        if (m_auto_scroll) m_needs_recenter = true;
    }
    if (was_auto_scroll_active) ImGui::PopStyleColor(3); 
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(m_auto_scroll ? "Auto-Center: ON" : "Auto-Center: OFF");

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_CROSSHAIRS, ImVec2(32, 0))) 
    {
        m_needs_recenter = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Recenter Now");

    ImGui::PopStyleVar(); // Pop FramePadding
    ImGui::Separator();
}

void DOMModule::render_spread_row(SymbolData& sData, MarketData& data, double last_price)
{
    ImGui::TableNextRow(ImGuiTableRowFlags_None, 1.0f); // Fixed height for spread
    ImU32 price_bg = ImGui::ColorConvertFloat4ToU32(price_highlight);

    for (int i = 0; i < col_number; i++) 
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, price_bg, i);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 min_p = ImGui::GetCursorScreenPos();
    draw_list->AddLine(min_p, ImVec2(min_p.x + ImGui::GetContentRegionAvail().x, min_p.y), price_bg);

    // Remaining columns
    for(int i=0; i<4; i++) ImGui::TableNextColumn(); 
}

void DOMModule::draw_settings_content(MarketData& data)
{
    auto& sData = data.get(current_symbol);
    ImGui::Spacing();
    ImGui::TextDisabled("Grouping");

    // Grouping Steps
    static const char* step_labels[] = { "0.1", "0.2", "0.5", "1.0", "5.0", "10.0" };
    static double step_values[] = { 0.1, 0.2, 0.5, 1.0, 5.0, 10.0 };
    static int current_idx = 4; // Default 5.0
    ImGui::SetNextItemWidth(150);
    if (ImGui::Combo("Price Step", &current_idx, step_labels, IM_ARRAYSIZE(step_labels))) 
    {
        data.dom_step = std::atof(step_labels[current_idx]);
        sData.dom_dirty = true; // Force the Ceil/Floor math to run next frame
        m_needs_recenter = true;
    }
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled("Auto Scroll (Recenter)");
    if (ImGui::Checkbox("Enabled", &m_auto_scroll)) 
        if (m_auto_scroll) m_needs_recenter = true;

    ImGui::BeginDisabled(!m_auto_scroll);
    ImGui::SetNextItemWidth(150);
    // When the user moves the slider, the m_scroll_interval changes instantly
    ImGui::SliderFloat("Interval (sec)", &m_scroll_interval, 0.5f, 50.0f, "%.1f s");
    ImGui::EndDisabled();
    if (ImGui::Button("Recenter Now", ImVec2(-1, 0))) 
    {
        m_needs_recenter = true;
    }
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled("Right to Left Bars");
    ImGui::Checkbox("Ask", &right_to_left_ask);
    ImGui::Checkbox("Bid", &right_to_left_bid);
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled("Center Values");
    ImGui::Checkbox("Center Ask", &center_values_ask);
    ImGui::Checkbox("Center Bid", &center_values_bid);
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled("Colors");
    ImGui::ColorEdit4("Price", &price_color.x);
    ImGui::ColorEdit4("Current Price", &price_highlight.x);
    ImGui::ColorEdit4("Ask Bg", &ask_bg_color.x);
    ImGui::ColorEdit4("Bid Bg", &bid_bg_color.x);
    ImGui::Separator();
    ImGui::Spacing();
}