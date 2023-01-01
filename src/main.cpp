#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_EXCEPTIONS

#include <fmt/core.h>

#include <exception>
#include <vulkan/vulkan.hpp>

using fmt::print;
using std::terminate;

// NOLINTNEXTLINE
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

auto main() -> int
{
	vk::DynamicLoader loader;
	auto vkGetInstanceProcAddr =
			loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
	VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

	auto [result, instance] = vk::createInstanceUnique({});
	if (result != vk::Result::eSuccess) {
		print(stderr, "Failed to create instance\n");
		terminate();
	};
	VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);

	print("Hello, world!\n");
	return 0;
}
