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
        clipper.Begin((int)sData.tape.size());

        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
            {
                const auto& tick = sData.tape[row];
                ImVec4 color = tick.is_sell ? data.bid_color : data.ask_color;
                ImGui::TableNextRow();

                if (ImGui::TableNextColumn()) 
                {
                    int hrs = ((int)tick.time % 86400) / 3600;
                    int mins = ((int)tick.time % 3600) / 60;
                    int secs = (int)tick.time % 60;
                    ImGui::TextColored(color, "%02d:%02d:%02d", hrs, mins, secs);
                }

                if (ImGui::TableNextColumn())
                    ImGui::TextColored(color, "%.2f", tick.price);

                if (ImGui::TableNextColumn()) 
                {
                    // make large trades bold or bright
                    if (tick.quantity > 10.0) 
                    {
                        ImGui::TextDisabled("!"); ImGui::SameLine();
                        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.f, 1.0f), "%.3f", tick.quantity);
                    } 
                    else 
                    {
                        ImGui::Text("%.3f", tick.quantity);
                    }
                }

                if (ImGui::TableNextColumn()) 
                {
                    float fraction = (float)(tick.quantity / sData.max_tape_qty);
                    if (fraction < 0.05f) fraction = 0.005;
                    ImVec4 bar_color = tick.is_sell 
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
                    if (tick.is_sell) 
                        ImGui::TextColored(color, "SELL");
                    else              
                        ImGui::TextColored(color, "BUY");
                }
            }
        }
        ImGui::EndTable();
    }
}

void TapeModule::draw_settings_content(MarketData& data)
{
    ImGui::Text("Tape Specific Settings");
}

/*void TapeModule::render(SymbolData& sData, MarketData& settings)
{
    if (!is_open) return;
    ImGui::Begin("Tape (Time & Sales)", &is_open);

    ImGui::SetNextItemWidth(100);
    bool enter_pressed = ImGui::InputText("##TapeInput", symbol_input, sizeof(symbol_input), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    bool button_clicked = ImGui::Button(ICON_FA_SEARCH); 
    if (enter_pressed || button_clicked)  
    {
        // Logic handled by Application to see if we need new connection
        current_symbol = symbol_input; 
        std::transform(current_symbol.begin(), current_symbol.end(), current_symbol.begin(), ::tolower);
        ImGui::SetWindowFocus(nullptr);
    }
    ImGui::SameLine();
    std::string display_sym = current_symbol;
    std::transform(display_sym.begin(), display_sym.end(), display_sym.begin(), ::toupper);
    ImGui::TextDisabled("| %s", display_sym.c_str());
    ImGui::Separator();

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_COG))
    {
        ImGui::OpenPopup("TapeSettingsPopup");
    }

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
        clipper.Begin((int)sData.tape.size());

        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
            {
                const auto& tick = sData.tape[row];
                ImVec4 color = tick.is_sell ? settings.bid_color : settings.ask_color;
                ImGui::TableNextRow();

                if (ImGui::TableNextColumn()) 
                {
                    int hrs = ((int)tick.time % 86400) / 3600;
                    int mins = ((int)tick.time % 3600) / 60;
                    int secs = (int)tick.time % 60;
                    ImGui::TextColored(color, "%02d:%02d:%02d", hrs, mins, secs);
                }

                if (ImGui::TableNextColumn())
                    ImGui::TextColored(color, "%.2f", tick.price);

                if (ImGui::TableNextColumn()) 
                {
                    // make large trades bold or bright
                    if (tick.quantity > 10.0) 
                    {
                        ImGui::TextDisabled("!"); ImGui::SameLine();
                        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.f, 1.0f), "%.3f", tick.quantity);
                    } 
                    else 
                    {
                        ImGui::Text("%.3f", tick.quantity);
                    }
                }

                if (ImGui::TableNextColumn()) 
                {
                    float fraction = (float)(tick.quantity / sData.max_tape_qty);
                    if (fraction < 0.05f) fraction = 0.005;
                    ImVec4 bar_color = tick.is_sell 
                        ? ImVec4(settings.bid_color.x, settings.bid_color.y, settings.bid_color.z, 0.6f)
                        : ImVec4(settings.ask_color.x, settings.ask_color.y, settings.ask_color.z, 0.6f);
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
                    if (tick.is_sell) 
                        ImGui::TextColored(color, "SELL");
                    else              
                        ImGui::TextColored(color, "BUY");
                }
            }
        }
        ImGui::EndTable();
    }

    ImGui::End();
}*/
