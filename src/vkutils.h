#pragma once
#include "volk.h"

#define VK_CHECK(x)                                                 \
	do                                                              \
	{                                                               \
		VkResult err = x;                                           \
		if (err)                                                    \
		{                                                           \
			spdlog::error("Detected Vulkan error: {}\n", (int32_t)err);      \
			abort();                                                \
		}                                                           \
	} while (0)

// VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment);
