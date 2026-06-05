#include <modules/base_module.h>

BaseModule::BaseModule(const std::string& name) 
    : window_name(name) 
{
    //memset(symbol_input, 0, sizeof(symbol_input));
    strncpy(symbol_input, "btcusdt", sizeof(symbol_input));
    //current_symbol = "btcusdt";
}


void BaseModule::render_standalone(MarketData& data) 
{
    if (!is_open) return;

    if (ImGui::Begin(window_name.c_str(), &is_open)) 
    {
        render_common_header(data);
        update_content(data);
    }
    ImGui::End();
}

void BaseModule::render_common_header(MarketData& data) 
{
    m_header_top_y = ImGui::GetCursorPosY(); // original line top, before any items
    float group_h   = module_header_height();
    float regular_h = ImGui::GetFrameHeight();
    float v_off     = std::max(0.0f, (group_h - regular_h) * 0.5f);
    auto vertical_center = [&]() { ImGui::SetCursorPosY(m_header_top_y + v_off); };

    vertical_center();
    ImGui::SetNextItemWidth(100);
    if (ImGui::InputText("##Sym", symbol_input, sizeof(symbol_input), ImGuiInputTextFlags_EnterReturnsTrue)) 
    {
        std::string input_str = symbol_input;
        std::transform(input_str.begin(), input_str.end(), input_str.begin(), ::tolower);
        current_symbol = input_str;
    }
    ImGui::SameLine(0, 2);
    vertical_center();
    if (ImGui::Button(ICON_FA_SEARCH)) 
    {
        std::string input_str = symbol_input;
        std::transform(input_str.begin(), input_str.end(), input_str.begin(), ::tolower);
        current_symbol = input_str;
    }

    ImGui::SameLine();
    vertical_center();
    std::string upper = current_symbol;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    ImGui::TextDisabled("| %s", upper.c_str());

    // module-specific buttons
    ImGui::SameLine();
    render_module_specific_header(data);

    // Pin settings cog to the far right
    float right_edge = ImGui::GetWindowWidth() - 40.0f;
    if (ImGui::GetScrollMaxY() > 0) right_edge -= ImGui::GetStyle().ScrollbarSize;
    
    ImGui::SameLine(right_edge);
    vertical_center();
    if (ImGui::Button(ICON_FA_COG, ImVec2(30, 0))) 
    {
        open_settings = !open_settings;
    }

    ImGui::Separator();
}

void BaseModule::render_settings_window(MarketData& data) 
{
    if (!open_settings) return;

    std::string modal_id = "Settings: " + window_name;
    ImGui::OpenPopup(modal_id.c_str());

    ImGuiWindowFlags settings_flags = ImGuiWindowFlags_NoDocking;

    ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal(modal_id.c_str(), &open_settings)) 
    {
        draw_settings_content(data);
        ImGui::EndPopup();
    }
}
