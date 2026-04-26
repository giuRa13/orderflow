#include <imgui_layer.h>
#include <imgui_internal.h>
#include <implot.h>
#include <IconsFontAwesome5.h>

void ImGuiLayer::init(GLFWwindow* window)
{
    m_window = window;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;

    // load Primary Font (Standard Text)
    io.Fonts->AddFontDefault();
    // setup Icon Font Configuration
    ImFontConfig config;
    config.MergeMode = true;           // merge with the primary font
    config.PixelSnapH = true;
    config.GlyphMinAdvanceX = 15.0f;   // set a minimum width for icons
    config.GlyphOffset.y = 1.f;
    // define the Icon Range (Font Awesome range)
    static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    io.Fonts->AddFontFromFileTTF(RESOURCES_PATH "fa-solid-900.ttf", 13.0f, &config, icon_ranges);
    //io.Fonts->Build();

    set_theme();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    style.TabRounding = 0;
    style.ScrollbarRounding = 8;
    style.WindowRounding = 0;
    style.GrabRounding = 4;
    style.FrameRounding = 0;
    style.PopupRounding = 0;

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

int ImGuiLayer::begin(int connection_status, bool& show_control_panel)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    float statusBarHeight =32.0f;
    int tab_to_open = -1;

    // 1. DRAW THE BIG MENU BAR FIRST
    //ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20, 20)); 
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(15.0f, 14.0f)); 
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 12.0f));

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Exit")) glfwSetWindowShouldClose(m_window, true);

            if (ImGui::MenuItem("ImGui Demo")) show_demo = !show_demo;

            if (ImGui::MenuItem("ImPlot Demo")) show_plot_demo = !show_plot_demo;

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Theme"))
        {
            if (ImGui::MenuItem("Dark")) ImGui::StyleColorsDark();

            if (ImGui::MenuItem("Light")) ImGui::StyleColorsLight();

            if (ImGui::MenuItem("Custom")) set_theme();

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Connections"))
        {
            if (ImGui::MenuItem("Manage Connections", "Ctrl+K")) 
                tab_to_open = 2; // the 3rd tab (index 2)
    
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Controls"))
        {
            // The &show_control_panel automatically handles the toggle AND the checkmark
            ImGui::MenuItem("Open Control Panel", nullptr, &show_control_panel);
            ImGui::Separator();
            ImGui::TextDisabled("Layout Options");
            if (ImGui::MenuItem("Reset Layout")) 
            {
                // Future logic for resetting docking
            }
            
            ImGui::EndMenu();
        }
        
        ImGui::EndMainMenuBar();
    }
    ImGui::PopStyleVar(2); 

    // 2. SETUP THE DOCKSPACE WINDOW
    // Important: We use WorkPos and WorkSize. 
    // ImGui automatically subtracts the MainMenuBar height from these!
    ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImVec2 dockSpacePos = viewport->WorkPos;
    ImVec2 dockSpaceSize = viewport->WorkSize;
    dockSpaceSize.y -= statusBarHeight;
    ImGui::SetNextWindowPos(dockSpacePos); 
    ImGui::SetNextWindowSize(dockSpaceSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | 
                                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | 
                                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | 
                                   ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    //static bool p_open = true;
    //ImGui::Begin("MasterDockSpace", &p_open, window_flags);
    ImGui::Begin("MasterDockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    // 3. CREATE THE DOCKSPACE
    // Since we popped the (20,20) padding already, 
    // the Docking Tabs (Headers) will the normal theme padding.
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_NoWindowMenuButton | ImGuiDockNodeFlags_NoCloseButton);

    if (show_demo)
        ImGui::ShowDemoWindow(&show_demo);

    if (show_plot_demo)
        ImPlot::ShowDemoWindow(&show_plot_demo);
    
    ImGui::End(); // End MasterDockSpace

    render_status_bar(connection_status, statusBarHeight);
    return tab_to_open;
}

void ImGuiLayer::end()
{
    ImGuiIO& io = ImGui::GetIO();
    int w, h;
    glfwGetFramebufferSize(m_window, &w, &h);
    io.DisplaySize = ImVec2((float)w, (float)h);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }
}

void ImGuiLayer::shutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
}

void ImGuiLayer::render_status_bar(int status, float height)
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    
    // Position at the very bottom of the screen
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - height));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, height));
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImVec4 menuBarCol = ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg];
    ImGui::PushStyleColor(ImGuiCol_WindowBg, menuBarCol);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    
    if (ImGui::Begin("##StatusBar", nullptr, flags))
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 screen_pos = ImGui::GetWindowPos();
        
        float text_height = ImGui::GetTextLineHeight();
        float vertical_pad = (height - text_height) * 0.5f;

        ImU32 color;
        const char* status_text;
        if (status == 2) 
        {
            color = IM_COL32(0, 255, 120, 255);
            status_text = "CONNECTED";
        } else if (status == 1) 
        {
            color = IM_COL32(255, 180, 0, 255);
            status_text = "RECONNECTING...";
        } else 
        {
            color = IM_COL32(255, 60, 60, 255);
            status_text = "DISCONNECTED";
        }

        float radius = 6.0f;
        ImVec2 ball_center = ImVec2(screen_pos.x + 15, screen_pos.y + (height * 0.5f));
        draw_list->AddCircleFilled(ball_center, radius, color);
        draw_list->AddCircleFilled(ball_center, radius + 2.0f, (color & 0x00FFFFFF) | 0x33000000); // Glow

        ImGui::SetCursorPos(ImVec2(30, vertical_pad)); 
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "%s", status_text);
        if (ImGui::IsItemHovered() && status == 0) ImGui::SetTooltip("Open a Module to Connect");
        if (ImGui::IsItemHovered() && status == 2) ImGui::SetTooltip("BINANCE FUTURES");

        const char* app_version = "v_0.1.0";
        float text_width = ImGui::CalcTextSize(app_version).x;
        
        ImGui::SetCursorPos(ImVec2(viewport->Size.x - text_width - 15, vertical_pad));
        ImGui::TextDisabled("%s", app_version);
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void ImGuiLayer::set_theme()
{
    ImGuiStyle& style = ImGui::GetStyle();

    ImGui::StyleColorsDark();

    ImVec4 accent = ImVec4(0.988f, 0.196f, 0.368f, 1.000f);        
    ImVec4 accent_faded = ImVec4(0.988f, 0.196f, 0.368f, 0.500f);  
    ImVec4 accent_light = ImVec4(0.988f, 0.196f, 0.368f, 0.400f); 
    ImVec4 dark_bg = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);  // ImVec4(0.176f, 0.176f, 0.188f, 1.000f);
    ImVec4 plot_bg = ImVec4(0.074f, 0.078f, 0.078f, 1.00f);
    ImVec4 black = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);  

    style.Colors[ImGuiCol_ResizeGripActive]       = accent;
    style.Colors[ImGuiCol_ButtonActive]           = accent;
    style.Colors[ImGuiCol_FrameBgActive]           = accent;
    style.Colors[ImGuiCol_HeaderActive]             = accent;
    style.Colors[ImGuiCol_CheckMark]               = accent;
    style.Colors[ImGuiCol_ButtonHovered]           = accent;
    style.Colors[ImGuiCol_SliderGrab]              = accent;
    style.Colors[ImGuiCol_ResizeGrip]              = accent;
    style.Colors[ImGuiCol_ResizeGripActive]        = accent;
    style.Colors[ImGuiCol_ResizeGripHovered]       = accent;
    style.Colors[ImGuiCol_TextLink]                = accent;
    style.Colors[ImGuiCol_ScrollbarGrabActive]     = accent;

    style.Colors[ImGuiCol_HeaderHovered]             = accent_faded;
    style.Colors[ImGuiCol_Button]           = accent_faded;
    style.Colors[ImGuiCol_SeparatorHovered]      = accent_faded;
    style.Colors[ImGuiCol_FrameBgHovered]           = accent_light;
    style.Colors[ImGuiCol_Header]             = accent_light;
    style.Colors[ImGuiCol_SliderGrabActive]        = accent_light;
    style.Colors[ImGuiCol_ScrollbarGrabHovered]    = accent_light;
    style.Colors[ImGuiCol_TextSelectedBg]         = accent_faded;
    style.Colors[ImGuiCol_NavCursor]               = accent_faded;

    // FLOATING WINDOWS (Title Bar)
    style.Colors[ImGuiCol_TitleBg]          = plot_bg;      // Black when inactive
    style.Colors[ImGuiCol_TitleBgActive]    = plot_bg;       
    style.Colors[ImGuiCol_TitleBgCollapsed] = plot_bg;       // Black when collapsed

    // FOR DOCKED WINDOWS (Tabs)
    /*style.Colors[ImGuiCol_Tab]                = plot_bg;  // Inactive tab
    style.Colors[ImGuiCol_TabHovered]         = accent_faded; // Hovering
    style.Colors[ImGuiCol_TabActive]          = plot_bg;      // Active tab background matches window bg
    style.Colors[ImGuiCol_TabUnfocused]       = plot_bg;  
    style.Colors[ImGuiCol_TabUnfocusedActive] = plot_bg;*/
    style.Colors[ImGuiCol_Tab]                = accent_light;  // Inactive tab
    style.Colors[ImGuiCol_TabHovered]         = accent_faded; // Hovering
    style.Colors[ImGuiCol_TabActive]          = accent_light;      // Active tab background matches window bg
    style.Colors[ImGuiCol_TabUnfocused]       = accent_light;  
    style.Colors[ImGuiCol_TabUnfocusedActive] = accent_light;  

    // ACCENT LINE
    style.Colors[ImGuiCol_TabSelectedOverline]       = accent;
    style.Colors[ImGuiCol_TabDimmedSelectedOverline] = plot_bg;  

    style.Colors[ImGuiCol_Border]         = ImVec4(0.3f, 0.3f, 0.3f, 0.5f); 
    style.Colors[ImGuiCol_SeparatorActive] = accent; // The line between title and content
    style.WindowBorderSize = 1.0f; 
    style.TabBarOverlineSize = 2.0f; 

    style.Colors[ImGuiCol_WindowBg]               = dark_bg;
    style.Colors[ImGuiCol_FrameBg]            = ImVec4(0.25f, 0.25f, 0.25f, 0.54f);
    style.Colors[ImGuiCol_Separator]               = style.Colors[ImGuiCol_Border];
    style.Colors[ImGuiCol_PopupBg]                 = ImVec4(0.10f, 0.10f, 0.10f, 0.96f);
    style.Colors[ImGuiCol_ModalWindowDimBg]        = ImVec4(0.12f, 0.12f, 0.12f, 0.586f);
    style.Colors[ImGuiCol_DockingPreview]          = accent_light;
    style.Colors[ImGuiCol_DockingEmptyBg]  = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
    style.Colors[ImGuiCol_NavWindowingHighlight]   =  accent;
    //style.Colors[ImGuiCol_MenuBarBg]       = dark_bg;

    // ImGui plot
    ImVec4 plot_color = ImVec4(0.898f, 0.698f, 0.0f, 1.0f);
    ImVec4 plot_hover = ImVec4(1.0f, 0.6f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_PlotHistogram]           = plot_color;
    style.Colors[ImGuiCol_PlotHistogramHovered]    = plot_hover;
    style.Colors[ImGuiCol_PlotLines]               = plot_color;
    style.Colors[ImGuiCol_PlotLinesHovered]        = plot_hover;
    style.Colors[ImGuiCol_TableBorderStrong]       = style.Colors[ImGuiCol_TableBorderLight];

    style.WindowMenuButtonPosition = ImGuiDir_Left;
    style.WindowMenuButtonPosition = ImGuiDir_Right;
    style.WindowRounding    = 0.0f;
    style.TabRounding       = 0.0f;
    style.FrameRounding     = 0.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabRounding      = 0.0f;
    style.PopupRounding     = 0.0f;

    // ImPlot
    ImVec4 *pcolors = ImPlot::GetStyle().Colors;
    pcolors[ImPlotCol_PlotBg] = dark_bg;
    pcolors[ImPlotCol_FrameBg] = dark_bg;
    pcolors[ImPlotCol_PlotBorder] = ImVec4(0,0,0,0);
    pcolors[ImPlotCol_Selection] = accent_faded;
    pcolors[ImPlotCol_AxisBgHovered] = accent_light;
    pcolors[ImPlotCol_AxisBgActive] = accent;
    pcolors[ImPlotCol_AxisBg] = dark_bg;
    pcolors[ImPlotCol_Crosshairs] = style.Colors[ImGuiCol_TextDisabled];
    pcolors[ImPlotCol_TitleText] = accent;

    pcolors[ImPlotCol_AxisText]   = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    pcolors[ImPlotCol_AxisTick]  = ImVec4(0.30f, 0.30f, 0.30f, 0.50f);
    pcolors[ImPlotCol_AxisGrid]   = ImVec4(0.30f, 0.30f, 0.30f, 0.50f);    

    auto &pstyle = ImPlot::GetStyle();
    pstyle.MinorAlpha = 1.0f; // Make Minor Grid lines just as bright as Major ones
    pstyle.PlotPadding = pstyle.LegendPadding = {12, 12};
    pstyle.LabelPadding = pstyle.LegendInnerPadding = {6, 6};
    pstyle.LegendSpacing = {10, 2};
    pstyle.AnnotationPadding = {4,2};
}

/*void ImGuiLayer::begin()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ////////////////////////////////////////////////////////////////////////////////
    static bool dockspaceOpen = true;
    static bool opt_fullscreen_persistant = true;
    bool opt_fullscreen = opt_fullscreen_persistant;
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
    // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
    //ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;

    if (opt_fullscreen)
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    }
    // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background and handle the pass-thru hole, so we ask Begin() to not render a background.
    if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
        window_flags |= ImGuiWindowFlags_NoBackground;
    // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
    // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive, 
    // all active windows docked into it will lose their parent and become undocked.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    //ImGui::Begin("DockSpace Demo", &dockspaceOpen, window_flags);
    ImGui::Begin("DockSpace Demo", nullptr, window_flags);
    ImGui::PopStyleVar();
    if (opt_fullscreen)
        ImGui::PopStyleVar(2);

    // DockSpace ////////////////////////////////////
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();
    float minWinSize = style.WindowMinSize.x;
    style.WindowMinSize.x = 320.0f; // if is a docking window
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    }
    style.WindowMinSize.x = minWinSize; // set back to default

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20, 20));
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Exit")) glfwSetWindowShouldClose(m_window, true);

            if (ImGui::MenuItem("ImGui Demo")) show_demo = !show_demo;

            if (ImGui::MenuItem("ImPlot Demo")) show_plot_demo = !show_plot_demo;

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Theme"))
        {
            if (ImGui::MenuItem("Dark")) ImGui::StyleColorsDark();

            if (ImGui::MenuItem("Light")) ImGui::StyleColorsLight();

            if (ImGui::MenuItem("Custom")) set_theme();

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Connections"))
        {

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
        ImGui::PopStyleVar();
    }

    if (show_demo)
        ImGui::ShowDemoWindow(&show_demo);

    if (show_plot_demo)
        ImPlot::ShowDemoWindow(&show_plot_demo);

    ImGui::End();
}*/