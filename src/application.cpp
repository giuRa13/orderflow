#include <application.h>
#include <iostream>
#include <stdio.h>
#include <mutex>
#include <vector>
#include <nlohmann/json.hpp>
#include <implot.h>
#include <implot_internal.h>
#include <IconsFontAwesome5.h>


static void glfw_error_callback(int error, const char* description)
{
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

/*static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse)
    {
    }
}

static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse)
    {
    }
}*/

static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureKeyboard)
    {
    }

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}


Application::Application() 
{
}

void Application::init()
{
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit())
		return std::exit(1);

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);            // 3.0+ only

	window = glfwCreateWindow(window_width, window_height, "GLFW window", nullptr, nullptr);
	if (window == nullptr)
		return std::exit(1);
	glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    //glfwSetMouseButtonCallback(window, &MouseButtonCallback);
    //glfwSetCursorPosCallback(window, &CursorPosCallback);
    glfwSetKeyCallback(window, &KeyCallback);    

    m_ImGuiLayer.init(window);
}

Application::~Application()
{
    m_ImGuiLayer.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
}

void Application::run()
{
    m_last_time = glfwGetTime();

    NetworkLayer provider(m_market_data);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        double current_time = glfwGetTime();
        m_delta_time = (float)(current_time - m_last_time);
        m_last_time = current_time;

        m_frame_count++;
        m_fps_timer += m_delta_time;
        if (m_fps_timer >= 1.0) 
        { 
            m_current_FPS = (float)m_frame_count;
            m_frame_count = 0;
            m_fps_timer = 0.0;
        }

        manage_connections(provider);

        //m_ImGuiLayer.begin(provider.connection_status);
        int requested_tab = m_ImGuiLayer.begin(provider.connection_status, m_show_control_panel);
        {
            if (requested_tab != -1) 
            {
                m_show_control_panel = true;
                m_force_tab_index = requested_tab; 
                ImGui::SetWindowFocus("Control Panel"); 
            }

            // --- THE MASTER FRAME LOCK ---
            // lock the data for the ENTIRE duration of the UI draw calls.
            // This ensures the Chart, Tape, and CVD all see the EXACT same state for this specific frame.
            std::lock_guard<std::recursive_mutex> lock(m_market_data.mtx);

            if (m_show_control_panel) 
            {
                control_panel(provider);
            }

            if (m_candle_chart_module.is_open && m_cvd_module.is_open) 
            {
                ImGui::Begin("Candle + CVD", &m_candle_chart_module.is_open);

                // --- SHARED HEADER LOGIC ---
                ImGui::SetNextItemWidth(100);
                if (ImGui::InputText("##Sym", m_candle_chart_module.symbol_input, sizeof(m_candle_chart_module.symbol_input), ImGuiInputTextFlags_EnterReturnsTrue)) 
                {
                    std::string input = m_candle_chart_module.symbol_input;
                    std::transform(input.begin(), input.end(), input.begin(), ::tolower);
                    m_candle_chart_module.current_symbol = input;
                    // sync the input buffers too
                    strcpy(m_cvd_module.symbol_input, m_candle_chart_module.symbol_input);
                }
                ImGui::SameLine();
                std::string upper = m_candle_chart_module.current_symbol;
                std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
                ImGui::TextDisabled("| %s", upper.c_str());

                ImGui::SameLine(ImGui::GetWindowWidth() - 140); 
                if (ImGui::Button(ICON_FA_COG " Chart")) 
                    m_candle_chart_module.open_settings = !m_candle_chart_module.open_settings;
                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_COG " CVD")) 
                    m_cvd_module.open_settings = !m_cvd_module.open_settings;
                ImGui::Separator();

                m_cvd_module.current_symbol = m_candle_chart_module.current_symbol;

                static float rows[] = {2, 1};
                if (ImPlot::BeginSubplots("##Linked", 2, 1, ImVec2(-1, -1), ImPlotSubplotFlags_LinkAllX, rows)) 
                {
                    m_candle_chart_module.update_content(m_market_data);
                    m_cvd_module.update_content(m_market_data);
                    ImPlot::EndSubplots();
                }
                ImGui::End();

                // Ensure the CVD open state stays synced with the master window
                m_cvd_module.is_open = m_candle_chart_module.is_open;
            } 
            else 
            {
                if (m_candle_chart_module.is_open) m_candle_chart_module.render_standalone(m_market_data);
                if (m_cvd_module.is_open) m_cvd_module.render_standalone(m_market_data);
            }

            if (m_tape_module.is_open) m_tape_module.render_standalone(m_market_data);
            if (m_dom_module.is_open) m_dom_module.render_standalone(m_market_data);

            if (m_candle_chart_module.is_open) m_candle_chart_module.render_settings_window(m_market_data);
            if (m_cvd_module.is_open)         m_cvd_module.render_settings_window(m_market_data);
            if (m_tape_module.is_open)        m_tape_module.render_settings_window(m_market_data);
            if (m_dom_module.is_open)        m_dom_module.render_settings_window(m_market_data);
        }
        m_ImGuiLayer.end();

        glfwSwapBuffers(window);
    }
    provider.end();
}

/*
If something genuinely breaks (connection drops, data goes silent):
 - 30 second grace period before declaring stuck on first connect
 - 60 second grace period if data was previously flowing
 - Then exponential backoff: 5s → 10s → 20s → 40s → 80s → 120s max between retries
 - At worst that's ~6 reconnects to reach max backoff, then one reconnect every 2 minutes — well within Binance's 300/5min limit

 -Reconnects that don't count against rate limits — symbol changes and Spot↔Futures switches (user-initiated) 
 reset backoff to 5s and bypass the backoff timer entirely, since those are deliberate user actions not connection failures.
 -Each reconnect makes exactly 1 REST request (the depth snapshot) plus opens 2 WebSocket connections for Futures. At the max rate of one reconnect per 2 minutes
*/
void Application::manage_connections(NetworkLayer& provider) 
{
    double now = glfwGetTime();
    std::set<std::string> symbols;
 
    static double last_reconnect_time = 0;
    static double reconnect_backoff   = 5.0;  // doubles on each watchdog retry, max 120s
    static double stuck_since         = 0;
    static bool   last_is_futures     = m_market_data.m_is_futures;
 
    if (m_candle_chart_module.is_open) symbols.insert(m_candle_chart_module.current_symbol);
    if (m_cvd_module.is_open)          symbols.insert(m_cvd_module.current_symbol);
    if (m_tape_module.is_open)         symbols.insert(m_tape_module.current_symbol);
    if (m_dom_module.is_open)          symbols.insert(m_dom_module.current_symbol);
 
    if (symbols.empty()) return;
 
     // --- STATUS (green only when data is actually flowing) ---
    bool all_ok = true;
    for (const auto& sym : symbols)
    {
        auto& sData = m_market_data.get(sym);
        bool dom_open   = m_dom_module.is_open          && m_dom_module.current_symbol          == sym;
        bool trade_open = (m_tape_module.is_open         && m_tape_module.current_symbol         == sym)
                       || (m_candle_chart_module.is_open && m_candle_chart_module.current_symbol == sym);
        if (dom_open   && !sData.snapshot_loaded)                      all_ok = false;
        if (trade_open && sData.tape.empty() && sData.candles.empty()) all_ok = false;
    }
    provider.connection_status = all_ok ? 2 : 1;
 
    // --- WATCHDOG: reconnect only after an extended silence ---
    // 30s grace on first connect (data needs time to flow after connect).
    // 60s grace after data was previously flowing (longer = fewer reconnects).
    // Exponential backoff: 5 -> 10 -> 20 -> 40 -> 80 -> 120s max.
    bool watchdog_fire = false;
    if (!all_ok)
    {
        if (stuck_since == 0) stuck_since = now;
        double grace = (reconnect_backoff <= 5.0) ? 30.0 : 60.0;
        if (now - stuck_since > grace) watchdog_fire = true;
    }
    else
    {
        stuck_since       = 0;
        reconnect_backoff = 5.0;  // healthy connection resets backoff
    }

    bool symbols_changed = (symbols != m_last_subscribed_symbols);
    bool market_changed  = (m_market_data.m_is_futures != last_is_futures);
    bool force           = m_market_data.m_reconnect_requested;
 
    if (!symbols_changed && !market_changed && !force && !watchdog_fire) return;
 
    // Respect backoff for watchdog-triggered reconnects
    if (watchdog_fire && !force && !market_changed && !symbols_changed)
    {
        if (now - last_reconnect_time < reconnect_backoff) return;
        reconnect_backoff = std::min(reconnect_backoff * 2.0, 120.0);
    }
    else
    {
        reconnect_backoff = 5.0;
    }
 
    if (force)              std::cout << "[Network] Manual reconnect." << std::endl;
    else if (watchdog_fire) std::cout << "[Watchdog] No data for " << (int)(now - stuck_since)
                                      << "s, reconnecting (next backoff=" << (int)reconnect_backoff << "s)..." << std::endl;
    else                    std::cout << "[Network] Config changed, reconnecting." << std::endl;
 
    last_reconnect_time = now;
    last_is_futures     = m_market_data.m_is_futures;
    stuck_since         = 0;
    m_market_data.m_reconnect_requested = false;
 
    provider.end();
    {
        std::lock_guard<std::recursive_mutex> lock(m_market_data.mtx);
        for (const auto& s : symbols)
        {
            auto& sData = m_market_data.get(s);
            sData.candles.clear();     sData.tape.clear();
            sData.full_asks.clear();   sData.full_bids.clear();
            sData.ask_sums.clear();    sData.bid_sums.clear();
            sData.market_buys.clear(); sData.market_sells.clear();
            sData.snapshot_loaded = false;
            sData.dom_dirty       = true;
            sData.running_cvd     = 0;
            sData.max_market_vol  = 1.0;
            sData.m_sell_gen = 0;     
            sData.m_buy_gen = 0;
            sData.m_last_sell_bucket = -1e30; 
            sData.m_last_buy_bucket = -1e30;
            sData.m_market_sells_gen.clear(); 
            sData.m_market_buys_gen.clear();
        }
    }
 
    for (const auto& s : symbols)
        provider.fetch_dom_snapshot(s, m_market_data.m_is_futures);
 
    provider.start_multi(symbols, m_market_data.m_is_futures);
    m_last_subscribed_symbols = symbols;
 
    std::cout << "[WS] Subscriptions restarted for: ";
    for (auto& s : symbols) std::cout << s << " ";
    std::cout << std::endl;
}

void Application::control_panel(NetworkLayer& provider)
{
    if (!m_show_control_panel) return;

    auto AddSelectionRow = [&](const char* label, bool isActive, std::function<void()> onClick) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        
        // clickable label spanning the whole row
        if (ImGui::Selectable(label, false, ImGuiSelectableFlags_SpanAllColumns)) 
            onClick();

        // centered checkmark 
        ImGui::TableSetColumnIndex(1);
        if (isActive) 
        {
            float width = ImGui::GetContentRegionAvail().x;
            float icon_width = ImGui::CalcTextSize(ICON_FA_CHECK).x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - icon_width) * 0.5f);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 1.0f); 
            ImGui::TextColored(ImVec4(0.988f, 0.196f, 0.368f, 1.000f), ICON_FA_CHECK);
        }
    };

    if (!ImGui::Begin("Control Panel", &m_show_control_panel))
    {
        ImGui::End();
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.988f, 0.196f, 0.368f, 0.4f));
    ImGui::PushStyleColor(ImGuiCol_TabActive, ImVec4(0.988f, 0.196f, 0.368f, 1.000f));
    ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.988f, 0.196f, 0.368f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 0.0f);
    if (ImGui::BeginTabBar("ControlTabs"))
    {
        float table_width = 300.0f;
        static ImGuiTableFlags table_flags = 
            ImGuiTableFlags_Borders | 
            ImGuiTableFlags_RowBg | 
            ImGuiTableFlags_NoHostExtendX;

        // --- TAB COMMONS ---
        ImGuiTabItemFlags commons_flags = (m_force_tab_index == 0) ? ImGuiTabItemFlags_SetSelected : 0;
        if (ImGui::BeginTabItem("Commons", nullptr, commons_flags))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 3.0f));
            ImGui::Spacing();
            ImGui::TextDisabled("Performance");
            ImGui::Text("FPS: %.1f (%.4f s)", m_current_FPS, m_delta_time);
            ImGui::Spacing();
            ImGui::Separator();

            ImGui::Spacing();
            ImGui::Checkbox("Follow Live Price", &m_market_data.follow_live);

            if (ImGui::Button("Clear Chart Data")) 
            {
                std::lock_guard<std::recursive_mutex> lock(m_data_mtx);
                //m_market_data.candles.clear();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("Colors");
            ImGui::ColorEdit4("Crosshair", &m_market_data.crosshair_color.x);
            ImGui::ColorEdit4("Ask Color", &m_market_data.ask_color.x);
            ImGui::ColorEdit4("Bid Color", &m_market_data.bid_color.x);
            if (ImGui::Button("Reset Colors")) 
            {
                m_market_data.ask_color = ImVec4(0.454f, 0.65f, 0.886f, 1.0f);
                m_market_data.bid_color = ImVec4(0.666f, 0.227f, 0.215f, 1.0f);
                m_market_data.crosshair_color = ImVec4(0.7f, 0.7f, 0.7f, 0.8f);
            }

            ImGui::PopStyleVar();
            ImGui::EndTabItem();
        }

        // --- TAB MODULES ---
        ImGuiTabItemFlags modules_flags = (m_force_tab_index == 1) ? ImGuiTabItemFlags_SetSelected : 0;
        if (ImGui::BeginTabItem("Modules", nullptr, modules_flags))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 3.0f));
            ImGui::Spacing();
            ImGui::TextDisabled("Available Modules");
            //ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::BeginTable("ModulesTable", 2, table_flags, ImVec2(table_width, 0)))
            {
                ImGui::TableSetupColumn("Module Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("##Status", ImGuiTableColumnFlags_WidthFixed, 30.0f);

                AddSelectionRow("Tick Chart", m_candle_chart_module.is_open, 
                                [&](){
                                    m_candle_chart_module.is_open = !m_candle_chart_module.is_open;
                                    if(m_candle_chart_module.is_open) ImGui::SetWindowFocus("Tick Chart");
                                }
                );
                AddSelectionRow("CVD Chart", m_cvd_module.is_open,
                                [&](){
                                    m_cvd_module.is_open = !m_cvd_module.is_open;
                                    if(m_cvd_module.is_open) ImGui::SetWindowFocus("CVD Chart");
                                }
                );
                AddSelectionRow("Time & Sales", m_tape_module.is_open,
                                [&](){
                                    m_tape_module.is_open = !m_tape_module.is_open;
                                    if(m_tape_module.is_open) ImGui::SetWindowFocus("Time & Sales");
                                }
                );
                AddSelectionRow("DOM", m_dom_module.is_open,
                                [&](){
                                    m_dom_module.is_open = !m_dom_module.is_open;
                                    if(m_dom_module.is_open) ImGui::SetWindowFocus("DOM");
                                }
                );
                ImGui::EndTable();
            }

            ImGui::Spacing();
            ImGui::Separator();
            if (ImGui::Button("Close All Windows", ImVec2(table_width, 0))) 
                m_candle_chart_module.is_open = m_cvd_module.is_open = m_tape_module.is_open = false;
    
            ImGui::PopStyleVar(); 
            ImGui::EndTabItem();
        }

        // --- TAB CONNECTIONS ---
        ImGuiTabItemFlags conn_flags = (m_force_tab_index == 2) ? ImGuiTabItemFlags_SetSelected : 0;
        if (ImGui::BeginTabItem("Connections", nullptr, conn_flags))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 3.0f));
            ImGui::Spacing();
            ImGui::TextDisabled("Market Data Source");
            ImGui::Spacing();

            float table_width = 300.0f;
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8, 8));

            if (ImGui::BeginTable("ConnectionsTable", 2, table_flags, ImVec2(table_width, 0)))
            {
                ImGui::TableSetupColumn("Stream Type", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("##Status", ImGuiTableColumnFlags_WidthFixed, 30.0f);
            
                 AddSelectionRow("Binance Spot", !m_market_data.m_is_futures, 
                                [&](){m_market_data.m_is_futures = false;}
                );

                AddSelectionRow("Binance Futures", m_market_data.m_is_futures, 
                                [&](){m_market_data.m_is_futures = true;}
                );

                ImGui::EndTable();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
 
            ImVec4 pending_col = ImVec4(0.9f, 0.45f, 0.0f, 1.0f);
            ImVec4 normal_btn_col = ImGui::GetStyle().Colors[ImGuiCol_Button];
            ImVec4 active_col = m_market_data.m_reconnect_requested ? pending_col : normal_btn_col;
 
            ImGui::PushStyleColor(ImGuiCol_Button, active_col);
 
            // Apply & Connect
            if (ImGui::Button("Apply & Connect", ImVec2(table_width, 0))) 
            {
                m_market_data.m_reconnect_requested = true; 
            }
            ImGui::Spacing();
            // Force Reconnect (Uses the same flag)
            if (ImGui::Button(ICON_FA_SYNC " Force Reconnect", ImVec2(table_width, 0))) 
            {
                m_market_data.m_reconnect_requested = true; 
            }
 
            ImGui::PopStyleColor();
 
            ImGui::Spacing();
            ImGui::TextDisabled("Note: Reconnecting will clear current buffers.");
            
            // Status Info
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("Status:");
            ImGui::SameLine();
            if (provider.connection_status == 2) ImGui::TextColored(ImVec4(0,1,0,1), "HEALTHY");
            else ImGui::TextColored(ImVec4(1,0.5f,0,1), "STALLED/CONNECTING");
 
            ImGui::PopStyleVar(2);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
    // Reset the force index to -1 after the frame is drawn
    // so it doesn't "lock" the tab and allow user to click other tabs again.
    m_force_tab_index = -1;
    ImGui::End();
}