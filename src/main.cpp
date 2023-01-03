#define GLFW_INCLUDE_NONE
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_EXCEPTIONS

#include <fmt/core.h>
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <exception>
#include <utility>
#include <vulkan/vulkan.hpp>

using fmt::print;
using std::terminate;

auto const window_width = 800;
auto const window_height = 600;
auto const application_name = "vkdemo";

// NOLINTNEXTLINE
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

auto fail(char const* message = "") -> void
{
	print(stderr, "FATAL: {}\n", message);
	terminate();
}

template <typename T>
auto check(vk::ResultValue<T> result, char const* message = "") -> T
{
	if (result.result != vk::Result::eSuccess) {
		fail(message);
	}
	return std::move(result.value);
}

auto glfw_error_callback(int error_code, char const* description) -> void
{
	print(stderr, "GLFW error {:#80X}: {}\n", error_code, description);
}

void glfw_key_callback(
		GLFWwindow* window,
		int key,
		int /*scancode*/,
		int /*action*/,
		int /*mods*/)
{
	switch (key) {
		case GLFW_KEY_Q:
		case GLFW_KEY_ESCAPE:
			glfwSetWindowShouldClose(window, GLFW_TRUE);
	}
}

class Application
{
 public:
	void run()
	{
		init_glfw();
		init_window();
		cleanup();
	}

 private:
	GLFWwindow* _window;

	auto init_glfw() -> void
	{
		glfwSetErrorCallback(glfw_error_callback);
		if (glfwInit() != GLFW_TRUE) {
			fail("Failed to initialize GLFW.");
		}
		if (glfwVulkanSupported() == GLFW_FALSE) {
			fail("Vulkan is not supported by the installed graphics drivers.");
		}
	}

	auto init_window() -> void
	{
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
		_window = glfwCreateWindow(
				window_width,
				window_height,
				application_name,
				nullptr,
				nullptr);
		glfwSetKeyCallback(_window, glfw_key_callback);
		glfwSetWindowUserPointer(_window, this);
	}

	auto cleanup() -> void
	{
		glfwTerminate();
	}
};

auto main() -> int
{
	auto loader = vk::DynamicLoader{};
	auto vkGetInstanceProcAddr =
			loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
	VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

	auto instance = check(
			vk::createInstanceUnique(vk::InstanceCreateInfo{}),
			"Failed to create a vulkan instance.");
	VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);

	auto app = Application{};
	app.run();
	return EXIT_SUCCESS;
}
