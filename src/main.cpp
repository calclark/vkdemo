#define GLFW_INCLUDE_NONE
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_EXCEPTIONS

#include <fmt/core.h>
#include <GLFW/glfw3.h>

#include <exception>
#include <utility>
#include <vulkan/vulkan.hpp>

using fmt::print;
using std::terminate;

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

auto main() -> int
{
	glfwSetErrorCallback(glfw_error_callback);
	if (glfwInit() == GLFW_FALSE) {
		fail("Failed to initialize GLFW.");
	}

	if (glfwVulkanSupported() == GLFW_FALSE) {
		fail("Vulkan is not supported by the installed graphics drivers.");
	}

	auto count = uint32_t{};
	auto* t = glfwGetRequiredInstanceExtensions(&count);
	for (auto i = uint32_t{0}; i < count; ++i) {
		print("{}\n", t[i]);
	}

	auto loader = vk::DynamicLoader{};
	auto vkGetInstanceProcAddr =
			loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
	VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

	auto instance = check(
			vk::createInstanceUnique(vk::InstanceCreateInfo{}),
			"Failed to create a vulkan instance.");
	VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);

	print("Hello, world!\n");
	return 0;
}
