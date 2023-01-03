#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_EXCEPTIONS

#include <fmt/core.h>
#include <GLFW/glfw3.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <span>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>

using fmt::print;
using std::array;
using std::move;
using std::span;
using std::terminate;
using std::vector;

auto const window_width = 800;
auto const window_height = 600;
auto const application_name = "vkdemo";
auto const validation_layers =
		array<char const*, 1>{"VK_LAYER_KHRONOS_validation"};

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
	return move(result.value);
}

auto check(VkResult const& status, char const* message = "") -> void
{
	if (status != VK_SUCCESS) {
		fail(message);
	}
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
		init_vulkan();
		cleanup();
	}

	Application(bool enable_layers) : _layers_enabled{enable_layers} {};

 private:
	bool _layers_enabled;
	GLFWwindow* _window = nullptr;
	vk::DynamicLoader _loader{};
	vk::UniqueInstance _instance;
	vk::UniqueSurfaceKHR _surface;

	auto init_glfw() -> void
	{
		glfwSetErrorCallback(glfw_error_callback);
		if (glfwInit() == GLFW_FALSE) {
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

	auto init_vulkan() -> void
	{
		init_loader();
		create_instance();
		create_surface();
	}

	auto init_loader() -> void
	{
		auto vkGetInstanceProcAddr =
				_loader.getProcAddress<PFN_vkGetInstanceProcAddr>(
						"vkGetInstanceProcAddr");
		VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
	}

	auto create_instance() -> void
	{
		auto app_info = vk::ApplicationInfo{
				.pApplicationName = application_name,
				.applicationVersion = VK_MAKE_VERSION(0, 1, 0),
				.pEngineName = "no engine",
				.engineVersion = VK_MAKE_VERSION(0, 0, 0),
				.apiVersion = VK_API_VERSION_1_3,
		};
		auto count = uint32_t{};
		auto* extensions = glfwGetRequiredInstanceExtensions(&count);
		auto layers = supported_layers();
		auto instance_ci = vk::InstanceCreateInfo{
				.pApplicationInfo = &app_info,
				.enabledLayerCount = static_cast<uint32_t>(layers.size()),
				.ppEnabledLayerNames = layers.data(),
				.enabledExtensionCount = count,
				.ppEnabledExtensionNames = extensions,
		};
		_instance = check(
				vk::createInstanceUnique(instance_ci),
				"Failed to create a vulkan instance.");
		VULKAN_HPP_DEFAULT_DISPATCHER.init(*_instance);
	}

	[[nodiscard]] auto supported_layers() const -> vector<char const*>
	{
		auto supported_layers = vector<char const*>();
		if (!_layers_enabled) {
			return supported_layers;
		}
		auto properties = check(vk::enumerateInstanceLayerProperties());
		for (auto const* layer : validation_layers) {
			auto layer_found = false;
			for (auto& property : properties) {
				if (strcmp(layer, property.layerName) == 0) {
					supported_layers.emplace_back(layer);
					layer_found = true;
				}
			}
			if (!layer_found) {
				print(
						stderr,
						"WARNING: Requested validation layer not found: {}\n",
						layer);
			}
		}
		return supported_layers;
	}

	auto create_surface() -> void
	{
		auto* surface = VkSurfaceKHR{};
		check(
				glfwCreateWindowSurface(_instance.get(), _window, nullptr, &surface),
				"Failed to create a surface.");
		_surface = vk::UniqueSurfaceKHR(surface, _instance.get());
	}

	auto cleanup() -> void
	{
		glfwTerminate();
	}
};

auto main(int argc, char** argv) -> int
{
	auto args = span(argv, static_cast<size_t>(argc));
	auto enable_layers = true;
	for (auto& arg : args) {
		if (strcmp(arg, "--disable-layers") == 0) {
			enable_layers = false;
		}
	}
	auto app = Application{enable_layers};
	app.run();
	return EXIT_SUCCESS;
}
