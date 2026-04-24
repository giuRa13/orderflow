#include <modules/tape_module.h>
#include <stdio.h>
#include <algorithm>

TapeModule::TapeModule() 
    : BaseModule("Tape (Time & Sales)") 
{
}

void TapeModule::update_content(MarketData& data)
{
    auto& sData = data.get(current_symbol);

    rebuild_filtered_view(sData);

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_COLUMNS))
    {
        ImGui::OpenPopup("TapeSettingsPopup");
    }

    if (ImGui::BeginPopup("TapeSettingsPopup")) 
    {
        ImGui::TextDisabled("Column Visibility");
        ImGui::Separator();
        ImGui::Checkbox("Time", &tape_show_time);
        ImGui::Checkbox("Price", &tape_show_price);
        ImGui::Checkbox("Size", &tape_show_size);
        ImGui::Checkbox("Profile Bar", &tape_show_profile);
        ImGui::Checkbox("Side", &tape_show_side);
        ImGui::EndPopup();
    }

    static ImGuiTableFlags flags = 
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | 
        ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;

    // -1.0 for height to fill the window, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Resizable
    if (ImGui::BeginTable("##tape", 5, flags, ImVec2(0, -1))) 
    {
        ImGui::TableSetupScrollFreeze(0, 1); // Make header visible while scrolling
        ImGui::TableSetupColumn("Time");
        ImGui::TableSetupColumn("Price");
        ImGui::TableSetupColumn("Size");
        ImGui::TableSetupColumn("Profile", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Side",    ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableSetColumnEnabled(0, tape_show_time);
        ImGui::TableSetColumnEnabled(1, tape_show_price);
        ImGui::TableSetColumnEnabled(2, tape_show_size);
        ImGui::TableSetColumnEnabled(3, tape_show_profile);
        ImGui::TableSetColumnEnabled(4, tape_show_side);
        ImGui::TableHeadersRow();

        // This handles thousands of rows with zero lag
        ImGuiListClipper clipper;                
        clipper.Begin((int)m_filtered_view.size());

        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
            {
                //const auto& tick = sData.tape[row];
                const auto* tick = m_filtered_view[row];
                ImVec4 color = tick->is_sell ? data.bid_color : data.ask_color;
                ImGui::TableNextRow();

                // --- BELOW BID / ABOVE ASK  HIGHLIGHTING ---
                // By placing tape_show_slippage_highlights at the beginning of the if condition, 
                // C++ will "short-circuit" the logic. This means if the checkbox is unchecked, 
                // the CPU won't even waste time checking tick.bid_at_time > 0
                if (tape_show_slippage_highlights && tick->bid_at_time > 0 && tick->ask_at_time > 0) 
                {
                     if (tick->is_sell && tick->price < tick->bid_at_time) 
                        {
                            ImU32 bg = ImGui::ColorConvertFloat4ToU32(ImVec4(data.bid_color.x, data.bid_color.y, data.bid_color.z, 0.20f));
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, bg);
                        }
                        else if (!tick->is_sell && tick->price > tick->ask_at_time) 
                        {
                            ImU32 bg = ImGui::ColorConvertFloat4ToU32(ImVec4(data.ask_color.x, data.ask_color.y, data.ask_color.z, 0.20f));
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, bg);
                        }
                }

                if (ImGui::TableNextColumn()) 
                {
                    int hrs = ((int)tick->time % 86400) / 3600;
                    int mins = ((int)tick->time % 3600) / 60;
                    int secs = (int)tick->time % 60;
                    ImGui::TextColored(color, "%02d:%02d:%02d", hrs, mins, secs);
                }

                if (ImGui::TableNextColumn())
                    ImGui::TextColored(color, "%.2f", tick->price);

                if (ImGui::TableNextColumn()) 
                {
                    // make large trades bold or bright
                    if (tick->quantity > 10.0) 
                    {
                        ImGui::TextDisabled("!"); ImGui::SameLine();
                        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.f, 1.0f), "%.3f", tick->quantity);
                    } 
                    else 
                    {
                        ImGui::Text("%.3f", tick->quantity);
                    }
                }

                if (ImGui::TableNextColumn()) 
                {
                    float fraction = (float)(tick->quantity / sData.max_tape_qty);
                    if (fraction < 0.05f) fraction = 0.005;
                    ImVec4 bar_color = tick->is_sell 
                        ? ImVec4(data.bid_color.x, data.bid_color.y, data.bid_color.z, 0.6f)
                        : ImVec4(data.ask_color.x, data.ask_color.y, data.ask_color.z, 0.6f);
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, bar_color);
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0,0,0,0)); // Transparent background
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                    char label[32];
                    sprintf(label, "##bar%d", row);
                    ImGui::ProgressBar(fraction, ImVec2(-1, ImGui::GetTextLineHeightWithSpacing() * 0.8f), "");
                    ImGui::PopStyleVar(2);
                    ImGui::PopStyleColor(2);
                }

                if (ImGui::TableNextColumn()) 
                {
                    if (tick->is_sell) 
                        ImGui::TextColored(color, "SELL");
                    else              
                        ImGui::TextColored(color, "BUY");
                }
            }
        }
        ImGui::EndTable();
    }
}

void TapeModule::rebuild_filtered_view(SymbolData& sData) 
{
    m_filtered_view.clear();
    
    if (sData.tape.empty()) return;

    // Optional: Pre-allocate memory to avoid reallocations
    m_filtered_view.reserve(sData.tape.size() / 2);

    for (size_t i = 0; i < sData.tape.size(); ++i) 
    {
        const auto& tick = sData.tape[i];

        //  Apply Size Filter
        if (tick.quantity < m_min_trade_size) continue;

        // Aggregation Logic 
        if (m_aggregate_by_time && !m_filtered_view.empty()) 
        {
            auto last = const_cast<TapeTick*>(m_filtered_view.back());

            // If same side, same price, and within X milliseconds
            double time_diff = std::abs(tick.time - last->time);
            if (tick.is_sell == last->is_sell && 
                tick.price == last->price && 
                time_diff < (m_aggregation_ms / 1000.0)) 
            {
                // Sum the quantity instead of adding a new row
                last->quantity += tick.quantity;
                continue; 
            }
        }
        m_filtered_view.push_back(&tick);
    }
}

void TapeModule::draw_settings_content(MarketData& data)
{
    ImGui::TextColored(ImVec4(0.988f, 0.196f, 0.368f, 1.000f), "Tape Specific Settings");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled("Filtering & Noise Reduction");
    ImGui::SliderInt("Max Raw History", &data.m_max_tape_rows, 500, 50000);
    
    if (ImGui::Button("Reset Volume Scale")) {
        data.get(current_symbol).max_tape_qty = 1.0;
    }

    ImGui::SetNextItemWidth(120);
    ImGui::InputFloat("Min Trade Size", &m_min_trade_size, 0.1f, 1.0f, "%.2f");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Hide trades smaller than this amount");

    ImGui::Checkbox("Aggregate Tape", &m_aggregate_by_time);
    if (m_aggregate_by_time) 
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::SliderFloat("ms", &m_aggregation_ms, 10.0f, 500.0f, "%.0f");
    }

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("Display Options");
    ImGui::Checkbox("Time Column", &tape_show_time);
    ImGui::Checkbox("Price Column", &tape_show_price);
    ImGui::Checkbox("Size Column", &tape_show_size);
    ImGui::Checkbox("Side Column", &tape_show_side);
    ImGui::Checkbox("Profile Bars", &tape_show_profile);

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("Order Flow Visuals");
    ImGui::Checkbox("Highlight Above Ask / Below Bid", &tape_show_slippage_highlights);
    
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Highlights trades that occurred outside the Best Bid/Offer (Slippage/Aggression)");
}

