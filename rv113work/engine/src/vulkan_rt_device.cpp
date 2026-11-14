#include "rv/vulkan_rt_device.hpp"
#include <vector>
#include <cstring>
#include <sstream>
#ifdef RV_HAS_VULKAN
#include <vulkan/vulkan.h>
#endif

namespace rv {
#ifdef RV_HAS_VULKAN
static bool hasExt(VkPhysicalDevice d,const char* wanted) {
  uint32_t n=0;
  if(vkEnumerateDeviceExtensionProperties(d,nullptr,&n,nullptr)!=VK_SUCCESS) return false;
  std::vector<VkExtensionProperties> props(n);
  if(vkEnumerateDeviceExtensionProperties(d,nullptr,&n,props.data())!=VK_SUCCESS) return false;
  for(const auto& p:props) if(std::strcmp(p.extensionName,wanted)==0) return true;
  return false;
}
static std::string vendorName(uint32_t id) {
  switch(id){case 0x10DE:return "NVIDIA";case 0x1002:return "AMD";case 0x8086:return "Intel";default:return "Unknown";}
}
#endif

RtDeviceReport initializeRayTracingDevice() {
  RtDeviceReport report;
#ifndef RV_HAS_VULKAN
  report.message="Executable dibuat tanpa Vulkan SDK. Pasang Vulkan SDK lalu build ulang di Windows.";
  return report;
#else
  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app.pApplicationName="REALVISION RENDER";
  app.applicationVersion=VK_MAKE_VERSION(1,11,0);
  app.pEngineName="REALVISION RENDER";
  app.engineVersion=VK_MAKE_VERSION(1,11,0);
  app.apiVersion=VK_API_VERSION_1_2;
  VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  ici.pApplicationInfo=&app;
  VkInstance instance=VK_NULL_HANDLE;
  VkResult vr=vkCreateInstance(&ici,nullptr,&instance);
  if(vr!=VK_SUCCESS){report.message="vkCreateInstance gagal: "+std::to_string(vr);return report;}

  uint32_t count=0;
  vr=vkEnumeratePhysicalDevices(instance,&count,nullptr);
  if(vr!=VK_SUCCESS||count==0){report.message="Tidak ada Vulkan physical device. Periksa driver NVIDIA.";vkDestroyInstance(instance,nullptr);return report;}
  std::vector<VkPhysicalDevice> devices(count);
  vkEnumeratePhysicalDevices(instance,&count,devices.data());
  std::ostringstream candidates;
  VkPhysicalDevice selected=VK_NULL_HANDLE;uint32_t selectedQueue=0;
  VkPhysicalDeviceProperties selectedProps{};
  for(auto physical:devices) {
    VkPhysicalDeviceProperties props{};vkGetPhysicalDeviceProperties(physical,&props);
    const bool extAS=hasExt(physical,VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    const bool extRT=hasExt(physical,VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
    const bool extDHO=hasExt(physical,VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
    const bool extBDA=hasExt(physical,VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    candidates<<props.deviceName<<" | "<<vendorName(props.vendorID)<<" | Vulkan "
      <<VK_VERSION_MAJOR(props.apiVersion)<<"."<<VK_VERSION_MINOR(props.apiVersion)
      <<" | AS "<<(extAS?"yes":"no")<<" | RT pipeline "<<(extRT?"yes":"no")<<"\n";
    if(!(extAS&&extRT&&extDHO&&extBDA)) continue;
    VkPhysicalDeviceAccelerationStructureFeaturesKHR asf{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtf{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
    VkPhysicalDeviceBufferDeviceAddressFeatures bda{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
    asf.pNext=&rtf;rtf.pNext=&bda;
    VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};f2.pNext=&asf;
    vkGetPhysicalDeviceFeatures2(physical,&f2);
    if(!asf.accelerationStructure||!rtf.rayTracingPipeline||!bda.bufferDeviceAddress)continue;
    uint32_t qcount=0;vkGetPhysicalDeviceQueueFamilyProperties(physical,&qcount,nullptr);
    std::vector<VkQueueFamilyProperties> qp(qcount);vkGetPhysicalDeviceQueueFamilyProperties(physical,&qcount,qp.data());
    for(uint32_t qi=0;qi<qcount;qi++)if((qp[qi].queueFlags&VK_QUEUE_GRAPHICS_BIT)&&(qp[qi].queueFlags&VK_QUEUE_COMPUTE_BIT)){
      selected=physical;selectedQueue=qi;selectedProps=props;break;
    }
    if(selected)break;
  }
  report.details="Perangkat Vulkan terdeteksi:\n"+candidates.str();
  if(!selected){report.message="Tidak ditemukan GPU dengan Vulkan ray tracing pipeline + acceleration structure. Pastikan driver NVIDIA terbaru dan GPU mendukung DXR/Vulkan RT.";vkDestroyInstance(instance,nullptr);return report;}

  const float priority=1.f;
  VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qci.queueFamilyIndex=selectedQueue;qci.queueCount=1;qci.pQueuePriorities=&priority;
  VkPhysicalDeviceAccelerationStructureFeaturesKHR asf{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};asf.accelerationStructure=VK_TRUE;
  VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtf{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};rtf.rayTracingPipeline=VK_TRUE;
  VkPhysicalDeviceBufferDeviceAddressFeatures bda{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};bda.bufferDeviceAddress=VK_TRUE;
  asf.pNext=&rtf;rtf.pNext=&bda;
  const char* extensions[]={VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME};
  VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};dci.pNext=&asf;dci.queueCreateInfoCount=1;dci.pQueueCreateInfos=&qci;dci.enabledExtensionCount=4;dci.ppEnabledExtensionNames=extensions;
  VkDevice device=VK_NULL_HANDLE;vr=vkCreateDevice(selected,&dci,nullptr,&device);
  if(vr!=VK_SUCCESS){report.message="vkCreateDevice gagal saat mengaktifkan fitur RT: "+std::to_string(vr);vkDestroyInstance(instance,nullptr);return report;}
  VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtprops{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
  VkPhysicalDeviceProperties2 p2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};p2.pNext=&rtprops;vkGetPhysicalDeviceProperties2(selected,&p2);
  VkQueue queue=VK_NULL_HANDLE;vkGetDeviceQueue(device,selectedQueue,0,&queue);
  std::ostringstream d;d<<"GPU: "<<selectedProps.deviceName<<"\nVendor: "<<vendorName(selectedProps.vendorID)<<"\nDevice ID: 0x"<<std::hex<<selectedProps.deviceID<<std::dec
    <<"\nVulkan API: "<<VK_VERSION_MAJOR(selectedProps.apiVersion)<<"."<<VK_VERSION_MINOR(selectedProps.apiVersion)
    <<"\nRT shader group handle size: "<<rtprops.shaderGroupHandleSize<<" bytes"
    <<"\nRT max recursion depth: "<<rtprops.maxRayRecursionDepth
    <<"\nQueue family: "<<selectedQueue<<"\nLogical device: created\n";
  report.success=true;report.name=selectedProps.deviceName;report.details+=d.str();
  report.message="GPU ray-tracing logical device berhasil dibuat. Ini baru validasi perangkat; pipeline shader, BLAS/TLAS, SBT, dan vkCmdTraceRaysKHR belum dijalankan.";
  vkDestroyDevice(device,nullptr);vkDestroyInstance(instance,nullptr);return report;
#endif
}
}
