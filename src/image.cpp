#include "image.hpp"

std::unordered_map<VkImage, std::unique_ptr<struct image>> imagesMap;

struct image *
find_image(VkImage image) {
	auto it = imagesMap.find(image);

	if (it == imagesMap.end())
		return nullptr;

	return it->second.get();
}

VK_LAYER_EXPORT VkResult VKAPI_CALL
BCnLayer_CreateImage(VkDevice device,
					 const VkImageCreateInfo *pCreateInfo,
					 const VkAllocationCallbacks *pAllocator,
					 VkImage *pImage)
{
	VkResult result;
	VkLayerDispatchTable table;
	VkImageCreateInfo create_info = *pCreateInfo;

	struct device *dev = get_device(device);
	if (!dev)
		return VK_ERROR_INITIALIZATION_FAILED;

	table = dev->table;

	if (is_supported_bcn_format(pCreateInfo->format)) {
	    create_info.format = get_format_for_bcn(pCreateInfo->format);
	    create_info.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
	    create_info.flags &= ~VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
	}

	result = table.CreateImage(device, &create_info, pAllocator, pImage);

	if (result != VK_SUCCESS) {
		Logger::log("error", "Failed to create image, res %d", result);
		return result;
	}

    auto image = std::make_unique<struct image>();
    image->handle = *pImage,
    image->format = pCreateInfo->format;
    image->device = dev;
    image->alloc = pAllocator;

    {
    	scoped_lock l(global_lock);
    	imagesMap[*pImage] = std::move(image);
    }

	return VK_SUCCESS;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL
BCnLayer_CreateImageView(VkDevice device,
						 const VkImageViewCreateInfo *pCreateInfo,
						 const VkAllocationCallbacks *pAllocator,
						 VkImageView *pImageView)
{
	VkResult result;
	VkLayerDispatchTable table;
	VkImageViewCreateInfo create_info = *pCreateInfo;

	struct device *dev = get_device(device);
	if (!dev)
		return VK_ERROR_INITIALIZATION_FAILED;

	table = dev->table;

	if (is_supported_bcn_format(pCreateInfo->format)) {
		create_info.format = get_format_for_bcn(pCreateInfo->format);
	}

	result = table.CreateImageView(device, &create_info, pAllocator, pImageView);
	if (result != VK_SUCCESS) {
		Logger::log("error", "Failed to create image view, res %d", result);
		return result;
	}

	return VK_SUCCESS;
}

VK_LAYER_EXPORT void VKAPI_CALL
BCnLayer_DestroyImage(VkDevice device,
					  VkImage image,
					  const VkAllocationCallbacks *pAllocator)
{
	scoped_lock l(global_lock);

	struct device *dev = get_device(device);
	struct image *img = find_image(image);
	if (!dev || !img)
		return;

	dev->table.DestroyImage(device, image, pAllocator);	
	imagesMap.erase(image);
}
