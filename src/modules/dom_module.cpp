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

    // --- Recenter ---
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

    // --- AGGREGATE LIMIT VOLUMES (only when order book changes) ---
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

    // --- SCROLL SMOOTHING ---
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

    // TOP UI
    render_top_ui(data, sData);

    // COLUMNS
    render_main_table(data, sData, step, live_bucket);
    
}

void DOMModule::render_main_table(MarketData& data, SymbolData& sData, double step, double live_bucket)
{
    const int VIEW_RANGE = 50;

    ImU32 ask_bg = ImGui::ColorConvertFloat4ToU32(ImVec4(ask_bg_color.x, ask_bg_color.y, ask_bg_color.z, 0.45f));
    ImU32 bid_bg = ImGui::ColorConvertFloat4ToU32(ImVec4(bid_bg_color.x, bid_bg_color.y, bid_bg_color.z, 0.45f));
    ImU32 price_bg = ImGui::ColorConvertFloat4ToU32(ImVec4(price_highlight.x, price_highlight.y, price_highlight.z, 1.0f));

    static ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg | 
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | 
        ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;

    if (!ImGui::BeginTable("##dom", col_number, flags, ImVec2(0, -1))) return;

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
        sprintf(p_buf, "%.*f", m_price_decimals, p);//sprintf(p_buf, "%.2f", p);
        float off = (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(p_buf).x) * 0.5f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);
        ImGui::TextColored(price_color, "%s", p_buf);

        // Column 1: Bid (Limits)
        ImGui::TableNextColumn();
        if (p <= live_bucket) 
            render_dom_bar(sData.bid_sums[p], sData.cached_max_vol, data.ask_color, right_to_left_bid, center_values_bid);

        // COLUMN 2: SELLS (MARKET) 
        ImGui::TableNextColumn();
        double last_s_time = sData.last_sell_time.count(p) ? sData.last_sell_time[p] : 0.0;
        render_market_cell(sData.market_sells[p], sData.max_market_vol, data.bid_color, sells_text_color, true, center_values_market_sells, last_s_time);

        // COLUMN 3: BUYS (MARKET) 
        ImGui::TableNextColumn();
        double last_b_time = sData.last_buy_time.count(p) ? sData.last_buy_time[p] : 0.0;
        render_market_cell(sData.market_buys[p], sData.max_market_vol, data.ask_color, buys_text_color, false, center_values_market_buys, last_b_time);

        // COLUMN 4: ASK (LIMITS) 
        ImGui::TableNextColumn();
        if (p > live_bucket) 
            render_dom_bar(sData.ask_sums[p], sData.cached_max_vol, data.bid_color ,right_to_left_ask, center_values_ask);
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
    sprintf(buf, "%.*f", m_limit_decimals, qty); // sprintf(buf, "%.2f", qty);
    ImVec2 text_size = ImGui::CalcTextSize(buf);
    float text_x = 0.0f;
    
    if (center_text) 
        text_x = pos.x + (size.x - text_size.x) * 0.5f;
    else 
        text_x = right_to_left ? (pos.x + size.x - text_size.x - 5.0f) : (pos.x + 5.0f);

    float text_y = pos.y + (size.y - text_size.y) * 0.5f;
    draw_list->AddText(ImVec2(text_x, text_y), IM_COL32_WHITE, buf);
}

void DOMModule::render_market_cell(double qty, double max_vol, ImVec4 color, ImVec4 text_color, bool align_right, bool center_text, double last_activity_time)
{
    if (qty <= 0.0000001) return;

    float height = ImGui::GetTextLineHeightWithSpacing();
    ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x, height);
    ImVec2 pos = ImGui::GetCursorScreenPos();

    ImGui::Dummy(size); // Create the space

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    double now = glfwGetTime();
    double time_diff = now - last_activity_time;

    float fade_duration = m_highlight_fadeout_ms / 1000.0f;
    float fade_alpha = 0.0f;
    if (last_activity_time > 0.0 && time_diff < fade_duration)
        fade_alpha = 1.0f - ((float)time_diff / fade_duration); // linear fade to 0

    if (fade_alpha > 0.0f)
    {
        ImU32 highlight_col = ImGui::ColorConvertFloat4ToU32(ImVec4(
            color.x, color.y, color.z,
            fade_alpha * 0.55f  // peak 0.55 opacity, fades to 0
        ));
        draw_list->AddRectFilled(
            ImVec2(pos.x, pos.y + 1),
            ImVec2(pos.x + size.x, pos.y + height - 1),
            highlight_col
        );
 
        // Optional border at peak of highlight
        if (fade_alpha > 0.8f && m_market_orders_border)
        {
            draw_list->AddRect(
                ImVec2(pos.x, pos.y + 1),
                ImVec2(pos.x + size.x, pos.y + height - 1),
                IM_COL32(255, 255, 255, (int)(fade_alpha * 120))
            );
        }
    }

    // Text
    char buf[32];
    sprintf(buf, "%.*f", m_market_decimals, qty); //sprintf(buf, "%.2f", qty);
    ImVec2 text_size = ImGui::CalcTextSize(buf);

    float text_x = 0.0f;
    if (center_text) 
        text_x = pos.x + (size.x - text_size.x) * 0.5f;
    else 
        text_x = align_right ? (pos.x + size.x - text_size.x - 5.0f) : (pos.x + 5.0f);
    float text_y = pos.y + (size.y - text_size.y) * 0.5f;

    ImU32 text_col = (fade_alpha > 0.5f) ? IM_COL32_WHITE : ImGui::ColorConvertFloat4ToU32(text_color);

    draw_list->AddText(ImVec2(text_x, text_y), text_col, buf);
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
    float buttons_area_width = 110.0f; 
    float right_pos = ImGui::GetWindowWidth() - buttons_area_width - ImGui::GetStyle().WindowPadding.x;
    if (ImGui::GetScrollMaxY() > 0) right_pos -= ImGui::GetStyle().ScrollbarSize;
    ImGui::SetCursorPosX(right_pos);

    if (ImGui::Button(ICON_FA_TRASH, ImVec2(32, 0))) 
    {
        std::lock_guard<std::recursive_mutex> lock(data.mtx);
        sData.market_buys.clear();
        sData.market_sells.clear();
        sData.max_market_vol = 1.0;
        sData.m_sell_gen = 0;     
        sData.m_buy_gen = 0;
        sData.m_last_sell_bucket = -1e30; 
        sData.m_last_buy_bucket = -1e30;
        sData.m_market_sells_gen.clear(); 
        sData.m_market_buys_gen.clear();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear Market Volume Columns");
    ImGui::SameLine();

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
    ImGui::Checkbox("Center Market Sells", &center_values_market_sells);
    ImGui::Checkbox("Center Market Buys", &center_values_market_buys);
    ImGui::Checkbox("Market Orders Border", &m_market_orders_border);
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled("Market Orders Mode");
    ImGui::Checkbox("Cumulative", &data.m_dom_cumulative);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(
        "ON:  volume accumulates at each level across the session\n"
        "OFF: level resets when price returns to it (only current visit)");
    ImGui::Spacing();

    ImGui::TextDisabled("Market Orders Highlight");
    ImGui::SetNextItemWidth(150);
    ImGui::SliderFloat("Fadeout (ms)", &m_highlight_fadeout_ms, 100.0f, 5000.0f, "%.0f ms");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("How long the background highlight persists after a trade executes");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled("Digits Precision");
    ImGui::SliderInt("Price Decimals", &m_price_decimals, 0, 2);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Decimals for the Price column");
    ImGui::SliderInt("Limit Decimals", &m_limit_decimals, 0, 2);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Decimals for Bid/Ask volume");
    ImGui::SliderInt("Market Decimals", &m_market_decimals, 0, 2);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Decimals for Sells/Buys volume");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled("Colors");
    ImGui::ColorEdit4("Price", &price_color.x);
    ImGui::ColorEdit4("Current Price", &price_highlight.x);
    ImGui::ColorEdit4("Ask Bg", &ask_bg_color.x);
    ImGui::ColorEdit4("Bid Bg", &bid_bg_color.x);
    ImGui::ColorEdit4("Buys Text", &buys_text_color.x);
    ImGui::ColorEdit4("Sells text", &sells_text_color.x);
    ImGui::Separator();
    ImGui::Spacing();
}