#include "command_buffer.hpp"
#include "image.hpp"
#include "bcn.hpp"

std::unordered_map<VkCommandBuffer, std::shared_ptr<struct command_buffer>> commandBuffersMap;

struct command_buffer *
get_command_buffer(VkCommandBuffer commandbuffer)
{
	auto it = commandBuffersMap.find(commandbuffer);

	if (it == commandBuffersMap.end())
		return nullptr;

	return it->second.get();
}

VK_LAYER_EXPORT VkResult VKAPI_CALL
BCnLayer_AllocateCommandBuffers(VkDevice device,
								const VkCommandBufferAllocateInfo *pAllocateInfo,
								VkCommandBuffer *pCommandBuffers)
{
	VkResult result;
	VkLayerDispatchTable table;

	struct device *dev = get_device(device);
	if (!dev)
		return VK_ERROR_INITIALIZATION_FAILED;

	table = dev->table;
	
	result = table.AllocateCommandBuffers(device, pAllocateInfo, pCommandBuffers);
	if (result != VK_SUCCESS) {
		Logger::log("error", "Failed to allocate command buffers, res %d", result);
		return result;
	}

	for (uint32_t i = 0; i < pAllocateInfo->commandBufferCount; i++) {
		auto cmd = std::make_shared<struct command_buffer>();
		cmd->handle = pCommandBuffers[i];
		cmd->device = dev;
		cmd->pool = pAllocateInfo->commandPool;
		{
			scoped_lock l(global_lock);
			commandBuffersMap[pCommandBuffers[i]] = cmd;
		}
	}
	
	return VK_SUCCESS;
}

VK_LAYER_EXPORT void VKAPI_CALL
BCnLayer_FreeCommandBuffers(VkDevice device,
							VkCommandPool commandPool,
							uint32_t commandBufferCount,
							const VkCommandBuffer *pCommandBuffers)
{
	scoped_lock l(global_lock);

	struct device *dev = get_device(device);
	if (!dev)
		return;
	
	for (uint32_t i = 0; i < commandBufferCount; i++) {
		struct command_buffer *cb = get_command_buffer(pCommandBuffers[i]);
		if (!cb)
			continue;
			
	    dev->table.FreeCommandBuffers(dev->handle, commandPool, 1, &cb->handle);
		commandBuffersMap.erase(pCommandBuffers[i]);
	}
}

static struct device *
device_for_command_buffer(struct command_buffer *cb, VkCommandBuffer commandBuffer)
{
	if (cb)
		return cb->device;

	return get_device(reinterpret_cast<VkDevice>(commandBuffer));
}

static bool
bcn_copy_buffer_to_image(VkCommandBuffer commandBuffer,
						 VkBuffer srcBuffer,
						 VkImage dstImage,
						 VkImageLayout dstImageLayout,
						 uint32_t regionCount,
						 const VkBufferImageCopy *pRegions)
{
	struct command_buffer *cb = get_command_buffer(commandBuffer);
	struct image *img = find_image(dstImage);
	struct buffer *buf = find_buffer(srcBuffer);

	if (!cb || !img || !buf)
		return false;

	struct device *dev = cb->device;
	VkFormat format = img->format;

	if (!is_supported_bcn_format(dev, format))
		return false;

	int texel_size = is_bc6(format) ? 8 : 4;
	VkLayerDispatchTable table = dev->table;

	for (uint32_t i = 0; i < regionCount; i++) {
		VkBufferImageCopy copy_region = pRegions[i];

		if (dev->use_image_view) {
			decompress_bcn_compute(dev, commandBuffer, format, &copy_region, buf, nullptr, img, dstImageLayout);
		}
        else {
        	int w = copy_region.imageExtent.width;                                                         
        	int h = copy_region.imageExtent.height;
        	int size = w * h *texel_size;
        	auto staging_buf = create_staging_buffer(dev, size);

        	decompress_bcn_compute(dev, commandBuffer, format, &copy_region, buf, staging_buf.get(), img, dstImageLayout);
        	
			VkBufferMemoryBarrier bufferBarrier = {
				.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
				.pNext = nullptr,
		    	.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
		    	.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
		    	.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		    	.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		    	.buffer = staging_buf->handle,
		    	.offset = 0,
		    	.size = VK_WHOLE_SIZE
			};
		
			table.CmdPipelineBarrier(commandBuffer,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 
				0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);
	    
			copy_region.bufferOffset = 0;
			copy_region.bufferRowLength = 0;
			copy_region.bufferImageHeight = 0;

			table.CmdCopyBufferToImage(commandBuffer,
				staging_buf->handle, dstImage, dstImageLayout, 1, &copy_region);

			cb->fence->staging_buffers.push_back(std::move(staging_buf));
		}
	}

	return true;
}

VK_LAYER_EXPORT void VKAPI_CALL
BCnLayer_CmdCopyBufferToImage(VkCommandBuffer commandBuffer,
						      VkBuffer srcBuffer,
						      VkImage dstImage,
						      VkImageLayout dstImageLayout,
						      uint32_t regionCount,
						      const VkBufferImageCopy *pRegions)
{
	if (bcn_copy_buffer_to_image(commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions))
		return;

	struct device *dev = device_for_command_buffer(get_command_buffer(commandBuffer), commandBuffer);
	if (!dev)
		return;

	dev->table.CmdCopyBufferToImage(commandBuffer,
		srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
}

VK_LAYER_EXPORT void VKAPI_CALL
BCnLayer_CmdCopyBufferToImage2(VkCommandBuffer commandBuffer,
							   const VkCopyBufferToImageInfo2 *pCopyBufferToImageInfo)
{
	std::vector<VkBufferImageCopy> regions(pCopyBufferToImageInfo->regionCount);

	for (uint32_t i = 0; i < pCopyBufferToImageInfo->regionCount; i++) {
		const VkBufferImageCopy2 &r = pCopyBufferToImageInfo->pRegions[i];
		regions[i] = {
			.bufferOffset = r.bufferOffset,
			.bufferRowLength = r.bufferRowLength,
			.bufferImageHeight = r.bufferImageHeight,
			.imageSubresource = r.imageSubresource,
			.imageOffset = r.imageOffset,
			.imageExtent = r.imageExtent
		};
	}

	if (bcn_copy_buffer_to_image(commandBuffer,
			pCopyBufferToImageInfo->srcBuffer, pCopyBufferToImageInfo->dstImage,
			pCopyBufferToImageInfo->dstImageLayout, pCopyBufferToImageInfo->regionCount, regions.data()))
		return;

	struct device *dev = device_for_command_buffer(get_command_buffer(commandBuffer), commandBuffer);
	if (!dev)
		return;

	dev->table.CmdCopyBufferToImage2(commandBuffer, pCopyBufferToImageInfo);
}
