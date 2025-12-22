#ifndef __BCN_LAYER_HPP
#define __BCN_LAYER_HPP

#include "vulkan/vk_layer.h"
#include "vk_func.hpp"
#include "logger.hpp"

#include <vulkan/vulkan.h>
#include <unistd.h>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <memory>
#include <cstring>

#undef VK_LAYER_EXPORT
#if defined(WIN32)
#define VK_LAYER_EXPORT extern "C" __declspec(dllexport)
#else
#define VK_LAYER_EXPORT extern "C"
#endif

template <typename T>
void* GetKey(T item) {
    return *(void**) item;
}

extern std::mutex global_lock;
typedef std::lock_guard<std::mutex> scoped_lock;

struct device {
	VkDevice handle;
	VkPhysicalDevice physical;
	VkLayerDispatchTable table;
	VkPipeline s3tcPipeline;
	VkPipeline rgtcPipeline;
	VkPipeline bc6Pipeline;
	VkPipeline bc7Pipeline;
	VkPipelineLayout layout;
	VkQueue queue;
	uint32_t memoryIndex;
	int use_image_view;
	VkDescriptorSetLayout setLayout;
	std::vector<VkDescriptorPool> pools;
	const VkAllocationCallbacks *alloc;
};

struct device *get_device(VkDevice);

#endif
