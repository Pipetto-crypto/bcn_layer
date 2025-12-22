#ifndef __FENCE_HPP
#define __FENCE_HPP

#include "bcn_layer.hpp"
#include "buffer.hpp"

struct fence {
	VkFence handle;
	struct device *device;
	const VkAllocationCallbacks *alloc;
	std::vector<std::unique_ptr<struct buffer>> staging_buffers;
};

struct fence *get_fence(VkFence);

#endif
