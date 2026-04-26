#pragma once

#include <GLFW/glfw3.h>
#include <imgui_layer.h>
#include <deque>
#include <set>
#include <string>
#include <network_layer.h>
#include <modules/tape_module.h>
#include <modules/candle_chart_module.h>
#include <modules/cvd_module.h>
#include <modules/dom_module.h>

class Application
{
public:
	Application();
	~Application();

	void init();
	void run();
	void manage_connections(NetworkLayer& provider);

private:
	void control_panel(NetworkLayer& provider);

private:
	GLFWwindow* window = nullptr;
	int window_width = 1280;
	int window_height = 720;
	ImGuiLayer m_ImGuiLayer;

	double m_last_time = 0.0;
    float  m_delta_time = 0.0f;
	float m_current_FPS = 0.0f;
    int   m_frame_count = 0;
    double m_fps_timer = 0.0;

	int m_force_tab_index = -1;
	bool m_show_control_panel = true;

	std::recursive_mutex m_data_mtx;
	MarketData m_market_data;
	std::set<std::string> m_last_subscribed_symbols;
	
    TapeModule m_tape_module;
	CandleChartModule m_candle_chart_module;
	CVDModule m_cvd_module;
	DOMModule m_dom_module;
};