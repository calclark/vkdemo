#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_EXPLICIT_CTOR
#define GLM_FORCE_MESSAGES
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_EXCEPTIONS

#include <fmt/core.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <optional>
#include <span>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>

using fmt::print;
using std::array;
using std::clamp;
using std::ifstream;
using std::ios;
using std::optional;
using std::span;
using std::terminate;
using std::vector;
using std::filesystem::path;

auto const window_width = 800;
auto const window_height = 600;
auto const application_name = "vkdemo";
auto const validation_layers =
		array<char const*, 1>{"VK_LAYER_KHRONOS_validation"};
auto const device_extensions =
		array<char const*, 1>{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

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

auto check(VkResult const& status, char const* message = "") -> void
{
	if (status != VK_SUCCESS) {
		fail(message);
	}
}

auto check(vk::Result const& status, char const* message = "") -> void
{
	if (status != vk::Result::eSuccess) {
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

auto read_file(path const& file_name) -> vector<char>
{
	auto file = ifstream(file_name, ios::ate | ios::binary);
	auto file_size = file.tellg();
	auto buffer = vector<char>(file_size);
	file.seekg(0);
	file.read(buffer.data(), file_size);
	return buffer;
}

class QueueFamilyIndices
{
 public:
	optional<uint32_t> graphics_family;
	optional<uint32_t> present_family;

	[[nodiscard]] auto is_complete() const -> bool
	{
		return graphics_family.has_value() && present_family.has_value();
	}
};

class SwapChainSupportDetails
{
 public:
	vk::SurfaceCapabilitiesKHR capabilities;
	vector<vk::SurfaceFormatKHR> formats;
	vector<vk::PresentModeKHR> present_modes;
};

class Vertex
{
 public:
	glm::vec2 position;
	glm::vec3 color;

	static auto binding_description() -> vk::VertexInputBindingDescription
	{
		return vk::VertexInputBindingDescription{
				.binding = 0,
				.stride = sizeof(Vertex),
				.inputRate = vk::VertexInputRate::eVertex,
		};
	}

	static auto attribute_descriptions()
			-> array<vk::VertexInputAttributeDescription, 2>
	{
		return array<vk::VertexInputAttributeDescription, 2>{
				vk::VertexInputAttributeDescription{
						.location = 0,
						.binding = 0,
						.format = vk::Format::eR32G32Sfloat,
						.offset = offsetof(Vertex, position),
				},
				vk::VertexInputAttributeDescription{
						.location = 1,
						.binding = 0,
						.format = vk::Format::eR32G32B32Sfloat,
						.offset = offsetof(Vertex, color),
				},
		};
	}
};

auto const vertices = array<Vertex, 4>{
		Vertex{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
		Vertex{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
		Vertex{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
		Vertex{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}};

auto const indices = array<uint16_t, 6>{0, 1, 2, 2, 3, 0};

class BufferMemory
{
 public:
	vk::UniqueBuffer buffer;
	vk::UniqueDeviceMemory memory;
};

struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

class GLFWWrapper
{
 public:
	static auto instance() -> GLFWWrapper&
	{
		static auto instance = GLFWWrapper{};
		return instance;
	}

	GLFWWrapper(GLFWWrapper const&) = delete;
	GLFWWrapper(GLFWWrapper&&) = delete;
	auto operator=(GLFWWrapper const&) -> GLFWWrapper& = delete;
	auto operator=(GLFWWrapper&&) -> GLFWWrapper& = delete;

 private:
	GLFWWrapper()
	{
		glfwSetErrorCallback(glfw_error_callback);
		if (glfwInit() == GLFW_FALSE) {
			fail("Failed to initialize GLFW.");
		}
		if (glfwVulkanSupported() == GLFW_FALSE) {
			fail("Vulkan is not supported by the installed graphics drivers.");
		}
	};

	~GLFWWrapper()
	{
		glfwTerminate();
	};
};

class Application
{
 public:
	void run()
	{
		init_window();
		init_vulkan();
		loop();
	}

	Application(bool enable_layers, vk::PresentModeKHR present_mode)
			: _layers_enabled{enable_layers}, _present_mode{present_mode} {};

 private:
	bool _layers_enabled;
	vk::PresentModeKHR _present_mode;
	GLFWWrapper& _glfw = GLFWWrapper::instance();
	GLFWwindow* _window = nullptr;
	vk::DynamicLoader _loader{};
	vk::UniqueInstance _instance;
	vk::UniqueSurfaceKHR _surface;
	vk::PhysicalDevice _physical_device;
	QueueFamilyIndices _queue_familes;
	SwapChainSupportDetails _swapchain_details;
	vk::UniqueDevice _device;
	vk::Queue _graphics_queue;
	vk::Queue _present_queue;
	vk::UniqueSwapchainKHR _swapchain;
	vector<vk::Image> _swapchain_images;
	vk::Format _swapchain_image_format{vk::Format::eUndefined};
	vk::Extent2D _swapchain_extent;
	vector<vk::UniqueImageView> _image_views;
	vk::UniqueDescriptorSetLayout _descriptor_set_layout;
	vk::UniquePipelineLayout _pipeline_layout;
	vk::UniquePipeline _graphics_pipeline;
	vk::UniqueCommandPool _command_pool;
	vk::UniqueCommandBuffer _command_buffer;
	BufferMemory _vertex_buffer;
	BufferMemory _index_buffer;
	BufferMemory _uniform_buffer;
	void* _uniform_data{};
	vk::UniqueDescriptorPool _descriptor_pool;
	vk::DescriptorSet _descriptor_set;
	vk::UniqueSemaphore _image_free;
	vk::UniqueSemaphore _render_done_sem;
	vk::UniqueFence _render_done_fence;

	auto init_window() -> void
	{
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
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
		pick_physical_device();
		create_logical_device();
		create_swapchain();
		create_image_views();
		create_descriptor_set_layout();
		create_graphics_pipeline();
		create_command_pool();
		create_command_buffers();
		create_vertex_buffer();
		create_index_buffer();
		create_uniform_buffer();
		create_descriptor_pool();
		create_descriptor_sets();
		create_sync_objects();
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

	auto pick_physical_device() -> void
	{
		auto devices = check(_instance->enumeratePhysicalDevices());
		auto best = uint8_t{};
		for (auto const& device : devices) {
			auto queue_families = find_queue_families(device);
			auto swapchain_details = swapchain_support(device);
			auto score =
					device_suitability(device, queue_families, swapchain_details);
			if (score > best) {
				_physical_device = device;
				_queue_familes = queue_families;
				_swapchain_details = swapchain_details;
				best = score;
			}
		}
		if (_physical_device == vk::PhysicalDevice{nullptr}) {
			fail("Failed to find a suitable physical device.");
		}
	}

	auto find_queue_families(vk::PhysicalDevice const& device)
			-> QueueFamilyIndices
	{
		auto indices = QueueFamilyIndices{};
		auto families = device.getQueueFamilyProperties();
		auto idx = uint32_t{};
		for (auto const& family : families) {
			if (family.queueFlags & vk::QueueFlagBits::eGraphics) {
				indices.graphics_family.emplace(idx);
			}
			auto present_support =
					check(device.getSurfaceSupportKHR(idx, _surface.get()));
			if (present_support == VK_TRUE) {
				indices.present_family.emplace(idx);
			}
			if (indices.is_complete()) {
				break;
			}
			idx += 1;
		}
		return indices;
	}

	auto swapchain_support(vk::PhysicalDevice const& device)
			-> SwapChainSupportDetails
	{
		auto details = SwapChainSupportDetails{
				check(device.getSurfaceCapabilitiesKHR(_surface.get())),
				check(device.getSurfaceFormatsKHR(_surface.get())),
				check(device.getSurfacePresentModesKHR(_surface.get())),
		};
		return details;
	}

	auto device_suitability(
			vk::PhysicalDevice const& device,
			QueueFamilyIndices const& queue_families,
			SwapChainSupportDetails const& swapchain_details) -> uint8_t
	{
		if (!(queue_families.is_complete() && device_extensions_supported(device) &&
					swapchain_adequate(swapchain_details))) {
			return 0;
		}
		auto properties = device.getProperties();
		switch (properties.deviceType) {
			case vk::PhysicalDeviceType::eDiscreteGpu:
				return 3;
			case vk::PhysicalDeviceType::eIntegratedGpu:
				return 2;
			default:
				return 1;
		}
	}

	auto device_extensions_supported(vk::PhysicalDevice const& device) -> bool
	{
		auto available_extensions =
				check(device.enumerateDeviceExtensionProperties());
		for (auto const* required : device_extensions) {
			auto found = false;
			for (auto const& available : available_extensions) {
				if (strcmp(available.extensionName, required) == 0) {
					found = true;
				}
			}
			if (!found) {
				return false;
			}
		}
		return true;
	}

	auto swapchain_adequate(SwapChainSupportDetails const& details) -> bool
	{
		return !(details.formats.empty() && details.present_modes.empty());
	}

	auto create_logical_device() -> void
	{
		auto queue_priority = 1.0f;
		auto queue_cis = array<vk::DeviceQueueCreateInfo, 2>{
				vk::DeviceQueueCreateInfo{
						.queueFamilyIndex = _queue_familes.graphics_family.value(),
						.queueCount = 1,
						.pQueuePriorities = &queue_priority,
				},
				vk::DeviceQueueCreateInfo{
						.queueFamilyIndex = _queue_familes.present_family.value(),
						.queueCount = 1,
						.pQueuePriorities = &queue_priority,
				},
		};
		auto device_ci = vk::StructureChain<
				vk::DeviceCreateInfo,
				vk::PhysicalDeviceDynamicRenderingFeatures>{
				vk::DeviceCreateInfo{
						.queueCreateInfoCount =
								_queue_familes.graphics_family == _queue_familes.present_family
								? uint32_t{1}
								: uint32_t{2},
						.pQueueCreateInfos = queue_cis.data(),
						.enabledLayerCount = 0,
						.ppEnabledLayerNames = VK_NULL_HANDLE,
						.enabledExtensionCount = device_extensions.size(),
						.ppEnabledExtensionNames = device_extensions.data(),
						.pEnabledFeatures = VK_NULL_HANDLE,
				},
				vk::PhysicalDeviceDynamicRenderingFeatures{
						.dynamicRendering = VK_TRUE,
				},
		};
		_device = check(
				_physical_device.createDeviceUnique(device_ci.get()),
				"Failed to create a logical device.");
		VULKAN_HPP_DEFAULT_DISPATCHER.init(_device.get());
		_graphics_queue =
				_device->getQueue(_queue_familes.graphics_family.value(), 0);
		_present_queue =
				_device->getQueue(_queue_familes.present_family.value(), 0);
	}

	auto create_swapchain() -> void
	{
		auto format = choose_swapchain_surface_format(_swapchain_details.formats);
		auto extent = choose_swapchain_extent(_swapchain_details.capabilities);
		auto indices = array<uint32_t, 2>{
				_queue_familes.graphics_family.value(),
				_queue_familes.present_family.value()};
		auto same_family =
				_queue_familes.graphics_family == _queue_familes.present_family;
		auto swapchain_ci = vk::SwapchainCreateInfoKHR{
				.surface = _surface.get(),
				.minImageCount = _swapchain_details.capabilities.minImageCount,
				.imageFormat = format.format,
				.imageColorSpace = format.colorSpace,
				.imageExtent = extent,
				.imageArrayLayers = 1,
				.imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
				.imageSharingMode = same_family ? vk::SharingMode::eExclusive
																				: vk::SharingMode::eConcurrent,
				.queueFamilyIndexCount = indices.size(),
				.pQueueFamilyIndices = indices.data(),
				.preTransform = _swapchain_details.capabilities.currentTransform,
				.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
				.presentMode = _present_mode,
				.clipped = VK_TRUE,
				.oldSwapchain = VK_NULL_HANDLE,
		};
		_swapchain = check(
				_device->createSwapchainKHRUnique(swapchain_ci),
				"Failed to create a swapchain.");
		_swapchain_images = check(_device->getSwapchainImagesKHR(_swapchain.get()));
		_swapchain_image_format = format.format;
		_swapchain_extent = extent;
	}

	auto choose_swapchain_surface_format(span<vk::SurfaceFormatKHR> formats)
			-> vk::SurfaceFormatKHR
	{
		for (auto const& format : formats) {
			if (format.format == vk::Format::eB8G8R8A8Srgb &&
					format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
				return format;
			}
		}
		return formats[0];
	}

	auto choose_swapchain_extent(vk::SurfaceCapabilitiesKHR const& capabilities)
			-> vk::Extent2D
	{
		if (capabilities.currentExtent != vk::Extent2D{0xFFFFFFFF, 0xFFFFFFFF}) {
			return capabilities.currentExtent;
		}
		auto width = int{};
		auto height = int{};
		glfwGetFramebufferSize(_window, &width, &height);
		auto extent = vk::Extent2D{
				.width = clamp(
						static_cast<uint32_t>(width),
						capabilities.minImageExtent.width,
						capabilities.maxImageExtent.width),
				.height = clamp(
						static_cast<uint32_t>(height),
						capabilities.minImageExtent.height,
						capabilities.maxImageExtent.height),
		};
		return extent;
	}

	auto create_image_views() -> void
	{
		_image_views.resize(_swapchain_images.size());
		for (auto i = size_t{}; i < _swapchain_images.size(); ++i) {
			auto view_ci = vk::ImageViewCreateInfo{
					.image = _swapchain_images[i],
					.viewType = vk::ImageViewType::e2D,
					.format = _swapchain_image_format,
					.components =
							vk::ComponentMapping{
									.r = vk::ComponentSwizzle::eIdentity,
									.g = vk::ComponentSwizzle::eIdentity,
									.b = vk::ComponentSwizzle::eIdentity,
									.a = vk::ComponentSwizzle::eIdentity,
							},
					.subresourceRange =
							vk::ImageSubresourceRange{
									.aspectMask = vk::ImageAspectFlagBits::eColor,
									.baseMipLevel = 0,
									.levelCount = 1,
									.baseArrayLayer = 0,
									.layerCount = 1,
							},
			};
			_image_views[i] = check(
					_device->createImageViewUnique(view_ci),
					"Failed to create an image view.");
		}
	}

	auto create_descriptor_set_layout() -> void
	{
		auto binding = vk::DescriptorSetLayoutBinding{
				.binding = 0,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eVertex,
				.pImmutableSamplers = VK_NULL_HANDLE,
		};
		auto layout_ci = vk::DescriptorSetLayoutCreateInfo{
				.bindingCount = 1,
				.pBindings = &binding,
		};
		_descriptor_set_layout = check(
				_device->createDescriptorSetLayoutUnique(layout_ci),
				"Failed to create a descriptor set layout.");
	}

	auto create_graphics_pipeline() -> void
	{
		auto vert_shader_code = read_file("shaders/shader.vert.spv");
		auto frag_shader_code = read_file("shaders/shader.frag.spv");
		auto vert_shader_module = create_shader_module(vert_shader_code);
		auto frag_shader_module = create_shader_module(frag_shader_code);
		auto shader_stages = array<vk::PipelineShaderStageCreateInfo, 2>{
				create_pipeline_shader_info(
						vert_shader_module.get(),
						vk::ShaderStageFlagBits::eVertex),
				create_pipeline_shader_info(
						frag_shader_module.get(),
						vk::ShaderStageFlagBits::eFragment)};

		auto vertex_binding_description = Vertex::binding_description();
		auto vertex_attribute_descriptions = Vertex::attribute_descriptions();
		auto vertex_input_ci = vk::PipelineVertexInputStateCreateInfo{
				.vertexBindingDescriptionCount = 1,
				.pVertexBindingDescriptions = &vertex_binding_description,
				.vertexAttributeDescriptionCount = vertex_attribute_descriptions.size(),
				.pVertexAttributeDescriptions = vertex_attribute_descriptions.data(),
		};

		auto input_assembly_ci = vk::PipelineInputAssemblyStateCreateInfo{
				.topology = vk::PrimitiveTopology::eTriangleList,
				.primitiveRestartEnable = VK_FALSE,
		};

		auto viewport = vk::Viewport{
				.x = 0,
				.y = 0,
				.width = static_cast<float>(_swapchain_extent.width),
				.height = static_cast<float>(_swapchain_extent.height),
				.minDepth = 0,
				.maxDepth = 1,
		};
		auto scissor = vk::Rect2D{
				.offset =
						vk::Offset2D{
								.x = 0,
								.y = 0,
						},
				.extent = _swapchain_extent,
		};
		auto viewport_ci = vk::PipelineViewportStateCreateInfo{
				.viewportCount = 1,
				.pViewports = &viewport,
				.scissorCount = 1,
				.pScissors = &scissor,
		};

		auto rasterizer_ci = vk::PipelineRasterizationStateCreateInfo{
				.depthClampEnable = VK_FALSE,
				.rasterizerDiscardEnable = VK_FALSE,
				.polygonMode = vk::PolygonMode::eFill,
				.cullMode = vk::CullModeFlagBits::eBack,
				.frontFace = vk::FrontFace::eCounterClockwise,
				.depthBiasEnable = VK_FALSE,
				.depthBiasConstantFactor = 0,
				.depthBiasClamp = 0,
				.depthBiasSlopeFactor = 0,
				.lineWidth = 1.0,
		};

		auto multisample_ci = vk::PipelineMultisampleStateCreateInfo{
				.rasterizationSamples = vk::SampleCountFlagBits::e1,
				.sampleShadingEnable = VK_FALSE,
				.minSampleShading = 0,
				.pSampleMask = VK_NULL_HANDLE,
				.alphaToCoverageEnable = VK_FALSE,
				.alphaToOneEnable = VK_FALSE,
		};

		auto color_blend_attachment = vk::PipelineColorBlendAttachmentState{
				.blendEnable = VK_FALSE,
				.srcColorBlendFactor = vk::BlendFactor::eZero,
				.dstColorBlendFactor = vk::BlendFactor::eZero,
				.colorBlendOp = vk::BlendOp::eAdd,
				.srcAlphaBlendFactor = vk::BlendFactor::eZero,
				.dstAlphaBlendFactor = vk::BlendFactor::eZero,
				.alphaBlendOp = vk::BlendOp::eAdd,
				.colorWriteMask = vk::ColorComponentFlagBits::eR |
						vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB |
						vk::ColorComponentFlagBits::eA,
		};
		auto color_blend_ci = vk::PipelineColorBlendStateCreateInfo{
				.logicOpEnable = VK_FALSE,
				.logicOp = vk::LogicOp::eClear,
				.attachmentCount = 1,
				.pAttachments = &color_blend_attachment,
				.blendConstants = array<float, 4>{0, 0, 0, 0},
		};

		auto pipeline_layout_ci = vk::PipelineLayoutCreateInfo{
				.setLayoutCount = 1,
				.pSetLayouts = &_descriptor_set_layout.get(),
				.pushConstantRangeCount = 0,
				.pPushConstantRanges = VK_NULL_HANDLE,
		};
		_pipeline_layout = check(
				_device->createPipelineLayoutUnique(pipeline_layout_ci),
				"Failed to create a pipeline layout.");

		auto pipeline_ci = vk::StructureChain<
				vk::GraphicsPipelineCreateInfo,
				vk::PipelineRenderingCreateInfoKHR>{
				vk::GraphicsPipelineCreateInfo{
						.stageCount = 2,
						.pStages = shader_stages.data(),
						.pVertexInputState = &vertex_input_ci,
						.pInputAssemblyState = &input_assembly_ci,
						.pTessellationState = VK_NULL_HANDLE,
						.pViewportState = &viewport_ci,
						.pRasterizationState = &rasterizer_ci,
						.pMultisampleState = &multisample_ci,
						.pDepthStencilState = nullptr,
						.pColorBlendState = &color_blend_ci,
						.pDynamicState = nullptr,
						.layout = _pipeline_layout.get(),
						.renderPass = VK_NULL_HANDLE,
						.subpass = 0,
						.basePipelineHandle = VK_NULL_HANDLE,
						.basePipelineIndex = 0,
				},
				vk::PipelineRenderingCreateInfo{
						.viewMask = 0,
						.colorAttachmentCount = 1,
						.pColorAttachmentFormats = &_swapchain_image_format,
						.depthAttachmentFormat = {},
						.stencilAttachmentFormat = {},
				},
		};
		_graphics_pipeline = check(
				_device->createGraphicsPipelineUnique(nullptr, pipeline_ci.get()),
				"Failed to create a graphics pipeline.");
	}

	auto create_pipeline_shader_info(
			vk::ShaderModule const& module,
			vk::ShaderStageFlagBits const& stage) -> vk::PipelineShaderStageCreateInfo
	{
		auto stage_ci = vk::PipelineShaderStageCreateInfo{
				.stage = stage,
				.module = module,
				.pName = "main",
				.pSpecializationInfo = VK_NULL_HANDLE,
		};
		return stage_ci;
	}

	auto create_shader_module(span<char> code) -> vk::UniqueShaderModule
	{
		auto module_ci = vk::ShaderModuleCreateInfo{
				.codeSize = code.size_bytes(),
				.pCode = reinterpret_cast<uint32_t*>(code.data()),
		};
		auto module = check(
				_device->createShaderModuleUnique(module_ci),
				"Failed to create shader module.");
		return module;
	}

	auto create_command_pool() -> void
	{
		auto pool_ci = vk::CommandPoolCreateInfo{
				.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
				.queueFamilyIndex = _queue_familes.graphics_family.value(),
		};
		_command_pool = check(
				_device->createCommandPoolUnique(pool_ci),
				"Failed to create a command pool.");
	}

	auto create_command_buffers() -> void
	{
		auto command_buffer_ai = vk::CommandBufferAllocateInfo{
				.commandPool = _command_pool.get(),
				.level = vk::CommandBufferLevel::ePrimary,
				.commandBufferCount = 1,
		};
		auto buffers = check(
				_device->allocateCommandBuffersUnique(command_buffer_ai),
				"Failed to allocate command buffers.");
		_command_buffer = std::move(buffers[0]);
	}

	auto create_vertex_buffer() -> void
	{
		auto size = sizeof(Vertex) * vertices.size();
		auto stage = create_buffer(
				size,
				vk::BufferUsageFlagBits::eTransferSrc,
				vk::MemoryPropertyFlagBits::eHostVisible |
						vk::MemoryPropertyFlagBits::eHostCoherent);
		void* data = nullptr;
		check(_device->mapMemory(
				stage.memory.get(),
				0,
				size,
				vk::MemoryMapFlags{},
				&data));
		memcpy(data, vertices.data(), size);
		_device->unmapMemory(stage.memory.get());
		_vertex_buffer = create_buffer(
				size,
				vk::BufferUsageFlagBits::eVertexBuffer |
						vk::BufferUsageFlagBits::eTransferDst,
				vk::MemoryPropertyFlagBits::eDeviceLocal);
		copy_buffer(stage.buffer.get(), _vertex_buffer.buffer.get(), size);
	}

	auto create_index_buffer() -> void
	{
		auto size = sizeof(indices[0]) * indices.size();
		auto stage = create_buffer(
				size,
				vk::BufferUsageFlagBits::eTransferSrc,
				vk::MemoryPropertyFlagBits::eHostVisible |
						vk::MemoryPropertyFlagBits::eHostCoherent);
		void* data = nullptr;
		check(_device->mapMemory(
				stage.memory.get(),
				0,
				size,
				vk::MemoryMapFlags{},
				&data));
		memcpy(data, indices.data(), size);
		_device->unmapMemory(stage.memory.get());
		_index_buffer = create_buffer(
				size,
				vk::BufferUsageFlagBits::eIndexBuffer |
						vk::BufferUsageFlagBits::eTransferDst,
				vk::MemoryPropertyFlagBits::eDeviceLocal);
		copy_buffer(stage.buffer.get(), _index_buffer.buffer.get(), size);
	}

	auto create_uniform_buffer() -> void
	{
		auto size = sizeof(UniformBufferObject);
		_uniform_buffer = create_buffer(
				size,
				vk::BufferUsageFlagBits::eUniformBuffer,
				vk::MemoryPropertyFlagBits::eHostVisible |
						vk::MemoryPropertyFlagBits::eHostCoherent);
		check(_device->mapMemory(
				_uniform_buffer.memory.get(),
				0,
				size,
				vk::MemoryMapFlags{},
				&_uniform_data));
	};

	auto create_buffer(
			vk::DeviceSize size,
			vk::BufferUsageFlags flags,
			vk::MemoryPropertyFlags properties) -> BufferMemory
	{
		auto buffer_memory = BufferMemory{};
		auto buffer_ci = vk::BufferCreateInfo{
				.size = size,
				.usage = flags,
				.sharingMode = vk::SharingMode::eExclusive,
				.queueFamilyIndexCount = 0,
				.pQueueFamilyIndices = VK_NULL_HANDLE,
		};
		buffer_memory.buffer = check(
				_device->createBufferUnique(buffer_ci),
				"Failed to create a buffer.");
		auto memory_requirements =
				_device->getBufferMemoryRequirements(buffer_memory.buffer.get());
		auto allocate_info = vk::MemoryAllocateInfo{
				.allocationSize = memory_requirements.size,
				.memoryTypeIndex =
						find_memory_type(memory_requirements.memoryTypeBits, properties),
		};
		buffer_memory.memory = check(
				_device->allocateMemoryUnique(allocate_info),
				"Failed to allocate buffer memory.");
		check(_device->bindBufferMemory(
				buffer_memory.buffer.get(),
				buffer_memory.memory.get(),
				0));
		return buffer_memory;
	}

	auto find_memory_type(
			uint32_t type_filter,
			vk::MemoryPropertyFlags const& properties) -> uint32_t
	{
		auto memory_properties = _physical_device.getMemoryProperties();
		for (auto i = uint32_t{0}; i < memory_properties.memoryTypeCount; i++) {
			if (((type_filter & (1 << i)) != 0u) &&
					(memory_properties.memoryTypes[i].propertyFlags & properties) ==
							properties) {
				return i;
			}
		}
		fail("Failed to find a suitable memory type.");
		return 0;  // unreachable
	}

	auto copy_buffer(
			vk::Buffer const& src,
			vk::Buffer const& dst,
			vk::DeviceSize size) -> void
	{
		auto begin_info = vk::CommandBufferBeginInfo{
				.pInheritanceInfo = VK_NULL_HANDLE,
		};
		check(_command_buffer->reset());
		check(_command_buffer->begin(begin_info));
		auto copy = vk::BufferCopy{
				.srcOffset = 0,
				.dstOffset = 0,
				.size = size,
		};
		_command_buffer->copyBuffer(src, dst, 1, &copy);
		check(_command_buffer->end());
		auto submit_info = vk::SubmitInfo{
				.waitSemaphoreCount = 0,
				.pWaitSemaphores = VK_NULL_HANDLE,
				.pWaitDstStageMask = VK_NULL_HANDLE,
				.commandBufferCount = 1,
				.pCommandBuffers = &_command_buffer.get(),
				.signalSemaphoreCount = 0,
				.pSignalSemaphores = VK_NULL_HANDLE,
		};
		check(
				_graphics_queue.submit(1, &submit_info, VK_NULL_HANDLE),
				"Failed to submit a copy command buffer.");
		check(_graphics_queue.waitIdle());
	}

	auto create_descriptor_pool() -> void
	{
		auto pool_size = vk::DescriptorPoolSize{
				.type = vk::DescriptorType::eUniformBuffer,
				.descriptorCount = 1,
		};
		auto pool_ci = vk::DescriptorPoolCreateInfo{
				.maxSets = 1,
				.poolSizeCount = 1,
				.pPoolSizes = &pool_size,
		};
		_descriptor_pool = check(
				_device->createDescriptorPoolUnique(pool_ci),
				"Failed to create a descriptor pool.");
	}

	auto create_descriptor_sets() -> void
	{
		auto alloc_info = vk::DescriptorSetAllocateInfo{
				.descriptorPool = _descriptor_pool.get(),
				.descriptorSetCount = 1,
				.pSetLayouts = &_descriptor_set_layout.get(),
		};
		auto sets = check(
				_device->allocateDescriptorSets(alloc_info),
				"Failed to allocate descriptor sets.");
		_descriptor_set = sets[0];
		auto buffer_info = vk::DescriptorBufferInfo{
				.buffer = _uniform_buffer.buffer.get(),
				.offset = 0,
				.range = sizeof(UniformBufferObject),
		};
		auto descriptor_write = vk::WriteDescriptorSet{
				.dstSet = _descriptor_set,
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.pImageInfo = VK_NULL_HANDLE,
				.pBufferInfo = &buffer_info,
				.pTexelBufferView = VK_NULL_HANDLE,
		};
		_device->updateDescriptorSets(1, &descriptor_write, 0, VK_NULL_HANDLE);
	}

	auto create_sync_objects() -> void
	{
		auto semaphore_ci = vk::SemaphoreCreateInfo{};
		auto fence_ci = vk::FenceCreateInfo{
				.flags = vk::FenceCreateFlagBits::eSignaled,
		};
		_image_free = check(
				_device->createSemaphoreUnique(semaphore_ci),
				"Failed to create an image semaphore.");
		_render_done_sem = check(
				_device->createSemaphoreUnique(semaphore_ci),
				"Failed to create an image semaphore.");
		_render_done_fence = check(
				_device->createFenceUnique(fence_ci),
				"Failed to create a frame fence.");
	}

	auto loop() -> void
	{
		auto base_time = glfwGetTime();
		auto frame_count = 0;
		while (glfwWindowShouldClose(_window) == GLFW_FALSE) {
			frame_count += 1;
			auto curr_time = glfwGetTime();
			if (curr_time > base_time + 1) {
				print("FPS: {}\n", frame_count);
				base_time = curr_time;
				frame_count = 0;
			}
			glfwPollEvents();
			draw_frame();
		}
		check(_device->waitIdle());
	}

	auto draw_frame() -> void
	{
		check(
				_device->waitForFences(_render_done_fence.get(), VK_TRUE, UINT64_MAX));
		check(_device->resetFences(1, &_render_done_fence.get()));
		update_uniform();
		auto image_index = check(
				_device->acquireNextImageKHR(
						_swapchain.get(),
						UINT64_MAX,
						_image_free.get(),
						VK_NULL_HANDLE),
				"Failed to acquire next image.");
		check(_command_buffer->reset());
		record_command_buffer(_command_buffer.get(), image_index);
		auto signal_semaphores = array<vk::Semaphore, 1>{_render_done_sem.get()};
		auto wait_semaphores = array<vk::Semaphore, 1>{_image_free.get()};
		auto wait_staged = array<vk::PipelineStageFlags, 1>{
				vk::PipelineStageFlagBits::eColorAttachmentOutput};
		auto submit_info = vk::SubmitInfo{
				.waitSemaphoreCount = wait_semaphores.size(),
				.pWaitSemaphores = wait_semaphores.data(),
				.pWaitDstStageMask = wait_staged.data(),
				.commandBufferCount = 1,
				.pCommandBuffers = &_command_buffer.get(),
				.signalSemaphoreCount = signal_semaphores.size(),
				.pSignalSemaphores = signal_semaphores.data(),
		};
		check(
				_graphics_queue.submit(1, &submit_info, _render_done_fence.get()),
				"Failed to submit a draw command buffer.");
		auto swapchains = array<vk::SwapchainKHR, 1>{_swapchain.get()};
		auto present_info = vk::PresentInfoKHR{
				.waitSemaphoreCount = signal_semaphores.size(),
				.pWaitSemaphores = signal_semaphores.data(),
				.swapchainCount = swapchains.size(),
				.pSwapchains = swapchains.data(),
				.pImageIndices = &image_index,
				.pResults = VK_NULL_HANDLE,
		};
		check(_present_queue.presentKHR(present_info));
	}

	auto update_uniform() -> void
	{
		static auto start = glfwGetTime();
		auto now = glfwGetTime();
		auto time = static_cast<float>(now - start);
		auto ubo = UniformBufferObject{
				.model = glm::rotate(
						glm::mat4{1.0f},
						time * glm::radians(90.0f),
						glm::vec3{0.0f, 0.0f, 1.0f}),
				.view = glm::lookAt(
						glm::vec3{2.0f, 2.0f, 2.0f},
						glm::vec3{0.0f, 0.0f, 0.0f},
						glm::vec3{0.0f, 0.0f, 1.0f}),
				.proj = glm::perspective(
						glm::radians(45.0f),
						static_cast<float>(_swapchain_extent.width) /
								static_cast<float>(_swapchain_extent.height),
						0.1f,
						10.0f),
		};
		ubo.proj[1][1] *= -1;
		memcpy(_uniform_data, &ubo, sizeof(ubo));
	}

	auto record_command_buffer(
			vk::CommandBuffer const& buffer,
			uint32_t image_index) -> void
	{
		auto command_buffer_bi = vk::CommandBufferBeginInfo{
				.pInheritanceInfo = VK_NULL_HANDLE,
		};
		auto color_attachment = vk::RenderingAttachmentInfo{
				.imageView = _image_views[image_index].get(),
				.imageLayout = vk::ImageLayout::eAttachmentOptimal,
				.resolveMode = vk::ResolveModeFlagBits::eNone,
				.resolveImageLayout = vk::ImageLayout::eUndefined,
				.loadOp = vk::AttachmentLoadOp::eClear,
				.storeOp = vk::AttachmentStoreOp::eStore,
				.clearValue = vk::ClearValue{{array<float, 4>{0.0, 0.0, 0.0, 1.0}}},
		};
		auto render_info = vk::RenderingInfo{
				.renderArea =
						vk::Rect2D{
								.offset = {0, 0},
								.extent = _swapchain_extent,
						},
				.layerCount = 1,
				.viewMask = 0,
				.colorAttachmentCount = 1,
				.pColorAttachments = &color_attachment,
				.pDepthAttachment = {},
				.pStencilAttachment = {},
		};
		auto foo = vk::ImageMemoryBarrier{
				.srcAccessMask = vk::AccessFlagBits::eNone,
				.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
				.oldLayout = vk::ImageLayout::eUndefined,
				.newLayout = vk::ImageLayout::eColorAttachmentOptimal,
				.srcQueueFamilyIndex = {},
				.dstQueueFamilyIndex = {},
				.image = _swapchain_images[image_index],
				.subresourceRange =
						vk::ImageSubresourceRange{
								.aspectMask = vk::ImageAspectFlagBits::eColor,
								.baseMipLevel = 0,
								.levelCount = 1,
								.baseArrayLayer = 0,
								.layerCount = 1,
						},
		};
		auto bar = vk::ImageMemoryBarrier{
				.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
				.dstAccessMask = vk::AccessFlagBits::eNone,
				.oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
				.newLayout = vk::ImageLayout::ePresentSrcKHR,
				.srcQueueFamilyIndex = {},
				.dstQueueFamilyIndex = {},
				.image = _swapchain_images[image_index],
				.subresourceRange =
						vk::ImageSubresourceRange{
								.aspectMask = vk::ImageAspectFlagBits::eColor,
								.baseMipLevel = 0,
								.levelCount = 1,
								.baseArrayLayer = 0,
								.layerCount = 1,
						},
		};
		check(
				buffer.begin(command_buffer_bi),
				"Failed to begin recording a command buffer.");
		buffer.pipelineBarrier(
				vk::PipelineStageFlagBits::eTopOfPipe,
				vk::PipelineStageFlagBits::eColorAttachmentOutput,
				vk::DependencyFlagBits{},
				0,
				nullptr,
				0,
				nullptr,
				1,
				&foo);
		buffer.beginRendering(&render_info);
		buffer.bindPipeline(
				vk::PipelineBindPoint::eGraphics,
				_graphics_pipeline.get());
		buffer.bindVertexBuffers(0, _vertex_buffer.buffer.get(), 0ul);
		buffer.bindIndexBuffer(
				_index_buffer.buffer.get(),
				0,
				vk::IndexType::eUint16);
		buffer.bindDescriptorSets(
				vk::PipelineBindPoint::eGraphics,
				_pipeline_layout.get(),
				0,
				1,
				&_descriptor_set,
				0,
				VK_NULL_HANDLE);
		buffer.drawIndexed(indices.size(), 1, 0, 0, 0);
		buffer.endRendering();
		buffer.pipelineBarrier(
				vk::PipelineStageFlagBits::eColorAttachmentOutput,
				vk::PipelineStageFlagBits::eBottomOfPipe,
				vk::DependencyFlagBits{},
				0,
				nullptr,
				0,
				nullptr,
				1,
				&bar);
		check(buffer.end(), "Failed to record a command buffer.");
	}
};

auto main(int argc, char** argv) -> int
{
	auto args = span(argv, static_cast<size_t>(argc));
	auto enable_layers = true;
	auto present_mode = vk::PresentModeKHR::eFifo;
	for (auto& arg : args) {
		if (strcmp(arg, "--disable-layers") == 0) {
			enable_layers = false;
		}
		if (strcmp(arg, "--mailbox") == 0) {
			present_mode = vk::PresentModeKHR::eMailbox;
		}
	}
	auto app = Application{enable_layers, present_mode};
	app.run();
	return EXIT_SUCCESS;
}
