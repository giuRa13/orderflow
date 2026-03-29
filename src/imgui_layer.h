#pragma once

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

class ImGuiLayer
{
public:
	ImGuiLayer() = default;
    ~ImGuiLayer() = default;

	void init(GLFWwindow* window);
	void begin();
	void end();
    void shutdown();
    void set_theme();

private:
    GLFWwindow* m_window = nullptr;
    bool show_demo = false;
    bool show_plot_demo = false;
};
