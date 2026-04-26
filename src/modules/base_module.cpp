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
        render_common_header();
        update_content(data);
    }
    ImGui::End();
}

void BaseModule::render_common_header() 
{
    ImGui::SetNextItemWidth(100);
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
        
    bool enter = ImGui::InputText("##Sym", symbol_input, sizeof(symbol_input), flags);
    ImGui::SameLine(0, 2);
    bool btn = ImGui::Button(ICON_FA_SEARCH);
    if (enter || btn) 
    {
        std::string input_str = symbol_input;
        std::transform(input_str.begin(), input_str.end(), input_str.begin(), ::tolower);
        ImGui::SetWindowFocus(nullptr);
        current_symbol = input_str;
    }

    ImGui::SameLine();
    std::string upper = current_symbol;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    ImGui::TextDisabled("| %s", upper.c_str());

    ImGui::SameLine(ImGui::GetWindowWidth() - 40);
    if (ImGui::Button(ICON_FA_COG)) 
    {
        open_settings = !open_settings;
        if (open_settings) 
        {
            // Force ImGui to bring this specific window ID to the front next frame
            std::string settings_title = "Settings: " + window_name;
            ImGui::SetWindowFocus(settings_title.c_str());
        }
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
