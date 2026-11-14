#include "rv/vulkan_rt_caps.hpp"
#include <sstream>
#ifdef RV_HAS_VULKAN
#include <vulkan/vulkan.h>
#include <vector>
#include <cstring>
#endif
namespace rv {
RayTracingCaps queryRayTracingCaps(){
 RayTracingCaps r;
#ifdef RV_HAS_VULKAN
 VkApplicationInfo ai{VK_STRUCTURE_TYPE_APPLICATION_INFO};
 ai.pApplicationName="REALVISION RENDER"; ai.apiVersion=VK_API_VERSION_1_2;
 VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ci.pApplicationInfo=&ai;
 VkInstance instance=VK_NULL_HANDLE;
 if(vkCreateInstance(&ci,nullptr,&instance)!=VK_SUCCESS){r.report="Vulkan loader exists but VkInstance creation failed.";return r;}
 r.loaderAvailable=true;
 uint32_t count=0;vkEnumeratePhysicalDevices(instance,&count,nullptr);
 if(!count){r.report="Vulkan loader available, but no Vulkan physical device was found.";vkDestroyInstance(instance,nullptr);return r;}
 std::vector<VkPhysicalDevice> devices(count);vkEnumeratePhysicalDevices(instance,&count,devices.data());
 // Prefer a physical device exposing the complete Vulkan RT extension set.
 for(auto dev:devices){
   uint32_t ec=0;vkEnumerateDeviceExtensionProperties(dev,nullptr,&ec,nullptr);
   std::vector<VkExtensionProperties> exts(ec);vkEnumerateDeviceExtensionProperties(dev,nullptr,&ec,exts.data());
   auto has=[&](const char*n){for(auto&e:exts)if(std::strcmp(e.extensionName,n)==0)return true;return false;};
   bool as=has(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
   bool rt=has(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
   bool dh=has(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
   bool bda=has(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
   if(as&&rt&&dh&&bda){
     VkPhysicalDeviceProperties props{};vkGetPhysicalDeviceProperties(dev,&props);
     r.deviceAvailable=true;r.accelerationStructure=as;r.rayTracingPipeline=rt;
     r.deferredHostOperations=dh;r.bufferDeviceAddress=bda;r.deviceName=props.deviceName;break;
   }
 }
 std::ostringstream o;
 o<<"Vulkan loader: yes\n";
 o<<"RT-capable device: "<<(r.deviceAvailable?"yes":"no")<<"\n";
 if(r.deviceAvailable)o<<"Device: "<<r.deviceName<<"\n";
 o<<"VK_KHR_acceleration_structure: "<<(r.accelerationStructure?"yes":"no")<<"\n";
 o<<"VK_KHR_ray_tracing_pipeline: "<<(r.rayTracingPipeline?"yes":"no")<<"\n";
 o<<"VK_KHR_deferred_host_operations: "<<(r.deferredHostOperations?"yes":"no")<<"\n";
 o<<"VK_KHR_buffer_device_address: "<<(r.bufferDeviceAddress?"yes":"no")<<"\n";
 r.report=o.str();
 vkDestroyInstance(instance,nullptr);
#else
 r.report="This build has no Vulkan SDK linkage. Reconfigure with Vulkan SDK installed.";
#endif
 return r;
}
}
