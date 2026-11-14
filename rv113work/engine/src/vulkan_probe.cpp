#include "rv/vulkan_probe.hpp"
#include <sstream>
#include <vector>
#ifdef RV_HAS_VULKAN
#include <vulkan/vulkan.h>
#endif
namespace rv{
std::string vulkanStatus(){
#ifdef RV_HAS_VULKAN
 VkApplicationInfo ai{VK_STRUCTURE_TYPE_APPLICATION_INFO};ai.pApplicationName="REALVISION RENDER";ai.apiVersion=VK_API_VERSION_1_2;
 VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ci.pApplicationInfo=&ai;VkInstance inst{};
 if(vkCreateInstance(&ci,nullptr,&inst)!=VK_SUCCESS)return "Vulkan loader found, instance creation failed";
 uint32_t n=0;vkEnumeratePhysicalDevices(inst,&n,nullptr);std::ostringstream o;o<<"Vulkan available; physical devices: "<<n;
 if(n){std::vector<VkPhysicalDevice> ds(n);vkEnumeratePhysicalDevices(inst,&n,ds.data());for(auto d:ds){VkPhysicalDeviceProperties p{};vkGetPhysicalDeviceProperties(d,&p);o<<" | "<<p.deviceName;}}
 vkDestroyInstance(inst,nullptr);return o.str();
#else
 return "Vulkan SDK/loader not detected at build time; CPU reference mode only.";
#endif
}}
