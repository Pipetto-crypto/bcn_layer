#ifndef __QUEUE_HPP
#define __QUEUE_HPP

#include "bcn_layer.hpp"
#include "fence.hpp"

struct queue {
	VkQueue handle;
	struct device *device;
};

struct queue *get_queue(VkQueue queue);

#endif
