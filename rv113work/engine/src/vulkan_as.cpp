#include "rv/vulkan_as.hpp"
#include <vector>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <stdexcept>
#ifdef RV_HAS_VULKAN
#include <vulkan/vulkan.h>
#endif

namespace rv {
#ifdef RV_HAS_VULKAN
namespace {
struct Buffer {
  VkBuffer handle{VK_NULL_HANDLE};
  VkDeviceMemory memory{VK_NULL_HANDLE};
  VkDeviceAddress address{0};
  VkDeviceSize size{0};
};
struct AsObject {
  VkAccelerationStructureKHR handle{VK_NULL_HANDLE};
  Buffer storage;
};
static uint32_t memoryType(VkPhysicalDevice physical,uint32_t bits,VkMemoryPropertyFlags wanted) {
  VkPhysicalDeviceMemoryProperties p{}; vkGetPhysicalDeviceMemoryProperties(physical,&p);
  for(uint32_t i=0;i<p.memoryTypeCount;i++)
    if((bits&(1u<<i)) && (p.memoryTypes[i].propertyFlags&wanted)==wanted) return i;
  throw std::runtime_error("No Vulkan memory type satisfies requested flags.");
}
static Buffer makeBuffer(VkPhysicalDevice physical,VkDevice device,VkDeviceSize size,
                         VkBufferUsageFlags usage,VkMemoryPropertyFlags flags,
                         bool needsAddress) {
  Buffer b; b.size=size;
  VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bi.size=size; bi.usage=usage; bi.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
  if(vkCreateBuffer(device,&bi,nullptr,&b.handle)!=VK_SUCCESS)
    throw std::runtime_error("vkCreateBuffer failed.");
  VkMemoryRequirements req{};vkGetBufferMemoryRequirements(device,b.handle,&req);
  VkMemoryAllocateFlagsInfo afi{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
  afi.flags=needsAddress?VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT:0;
  VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  ai.pNext=needsAddress?&afi:nullptr;ai.allocationSize=req.size;
  ai.memoryTypeIndex=memoryType(physical,req.memoryTypeBits,flags);
  if(vkAllocateMemory(device,&ai,nullptr,&b.memory)!=VK_SUCCESS)
    throw std::runtime_error("vkAllocateMemory failed.");
  if(vkBindBufferMemory(device,b.handle,b.memory,0)!=VK_SUCCESS)
    throw std::runtime_error("vkBindBufferMemory failed.");
  if(needsAddress) {
    VkBufferDeviceAddressInfo dai{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    dai.buffer=b.handle;b.address=vkGetBufferDeviceAddress(device,&dai);
    if(!b.address) throw std::runtime_error("vkGetBufferDeviceAddress returned zero.");
  }
  return b;
}
static void destroyBuffer(VkDevice d,Buffer&b) {
  if(b.handle)vkDestroyBuffer(d,b.handle,nullptr);
  if(b.memory)vkFreeMemory(d,b.memory,nullptr);
  b={};
}
static void uploadHost(VkDevice d,const Buffer&b,const void*data,size_t bytes) {
  void* mapped=nullptr;
  if(vkMapMemory(d,b.memory,0,bytes,0,&mapped)!=VK_SUCCESS)
    throw std::runtime_error("vkMapMemory failed.");
  std::memcpy(mapped,data,bytes);vkUnmapMemory(d,b.memory);
}
static VkDeviceAddress addressOf(VkDevice d,VkBuffer b) {
  VkBufferDeviceAddressInfo i{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};i.buffer=b;
  return vkGetBufferDeviceAddress(d,&i);
}

}
#endif

AccelerationBuildReport buildSceneAccelerationStructures(const Scene& scene) {
  AccelerationBuildReport report;
  report.triangleCount=scene.triangles.size();
#ifndef RV_HAS_VULKAN
  report.message="This build has no Vulkan SDK; acceleration-structure build is unavailable.";
  return report;
#else
  if(scene.triangles.empty()){report.message="Scene has no triangles.";return report;}
  VkInstance instance=VK_NULL_HANDLE;VkDevice device=VK_NULL_HANDLE;
  VkBuffer vertexBuffer=VK_NULL_HANDLE,indexBuffer=VK_NULL_HANDLE;
  VkDeviceMemory vertexMemory=VK_NULL_HANDLE,indexMemory=VK_NULL_HANDLE;
  VkAccelerationStructureKHR blas=VK_NULL_HANDLE,tlas=VK_NULL_HANDLE;
  VkBuffer blasStorage=VK_NULL_HANDLE,tlasStorage=VK_NULL_HANDLE,scratch=VK_NULL_HANDLE,instanceBuffer=VK_NULL_HANDLE;
  VkDeviceMemory blasMem=VK_NULL_HANDLE,tlasMem=VK_NULL_HANDLE,scratchMem=VK_NULL_HANDLE,instanceMem=VK_NULL_HANDLE;
  VkCommandPool pool=VK_NULL_HANDLE;VkCommandBuffer cmd=VK_NULL_HANDLE;
  auto cleanup=[&](){
    if(device){
      vkDeviceWaitIdle(device);
      if(cmd && pool) vkFreeCommandBuffers(device,pool,1,&cmd);
      if(pool) vkDestroyCommandPool(device,pool,nullptr);
      auto destroyAS=(PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(device,"vkDestroyAccelerationStructureKHR");
      if(destroyAS){if(blas)destroyAS(device,blas,nullptr);if(tlas)destroyAS(device,tlas,nullptr);}
      VkBuffer bs[]={vertexBuffer,indexBuffer,blasStorage,tlasStorage,scratch,instanceBuffer};
      VkDeviceMemory ms[]={vertexMemory,indexMemory,blasMem,tlasMem,scratchMem,instanceMem};
      for(auto b:bs)if(b)vkDestroyBuffer(device,b,nullptr);
      for(auto m:ms)if(m)vkFreeMemory(device,m,nullptr);
      vkDestroyDevice(device,nullptr);
    }
    if(instance)vkDestroyInstance(instance,nullptr);
  };
  try {
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName="REALVISION RENDER";app.apiVersion=VK_API_VERSION_1_2;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ici.pApplicationInfo=&app;
    if(vkCreateInstance(&ici,nullptr,&instance)!=VK_SUCCESS)throw std::runtime_error("vkCreateInstance failed.");
    uint32_t n=0;vkEnumeratePhysicalDevices(instance,&n,nullptr);
    std::vector<VkPhysicalDevice> physicals(n);if(n)vkEnumeratePhysicalDevices(instance,&n,physicals.data());
    VkPhysicalDevice physical=VK_NULL_HANDLE;uint32_t qFamily=0;
    for(auto candidate:physicals){
      uint32_t ec=0;vkEnumerateDeviceExtensionProperties(candidate,nullptr,&ec,nullptr);
      std::vector<VkExtensionProperties> exts(ec);vkEnumerateDeviceExtensionProperties(candidate,nullptr,&ec,exts.data());
      auto has=[&](const char*name){return std::any_of(exts.begin(),exts.end(),[&](auto&e){return std::strcmp(e.extensionName,name)==0;});};
      if(!(has(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME)&&has(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME)&&has(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME)&&has(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)))continue;
      VkPhysicalDeviceAccelerationStructureFeaturesKHR asf{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
      VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtf{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
      VkPhysicalDeviceBufferDeviceAddressFeatures bda{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
      asf.pNext=&rtf;rtf.pNext=&bda;VkPhysicalDeviceFeatures2 f{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};f.pNext=&asf;vkGetPhysicalDeviceFeatures2(candidate,&f);
      if(!asf.accelerationStructure||!rtf.rayTracingPipeline||!bda.bufferDeviceAddress)continue;
      uint32_t qc=0;vkGetPhysicalDeviceQueueFamilyProperties(candidate,&qc,nullptr);std::vector<VkQueueFamilyProperties> q(qc);vkGetPhysicalDeviceQueueFamilyProperties(candidate,&qc,q.data());
      bool found=false;for(uint32_t i=0;i<qc;i++)if((q[i].queueFlags&VK_QUEUE_GRAPHICS_BIT)&&(q[i].queueFlags&VK_QUEUE_COMPUTE_BIT)){physical=candidate;qFamily=i;found=true;break;}if(found)break;
    }
    if(!physical)throw std::runtime_error("No Vulkan RT-capable GPU/driver found.");
    VkPhysicalDeviceProperties props{};vkGetPhysicalDeviceProperties(physical,&props);report.deviceName=props.deviceName;
    float priority=1.f;VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qci.queueFamilyIndex=qFamily;qci.queueCount=1;qci.pQueuePriorities=&priority;
    VkPhysicalDeviceAccelerationStructureFeaturesKHR asf{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};asf.accelerationStructure=VK_TRUE;
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtf{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};rtf.rayTracingPipeline=VK_TRUE;
    VkPhysicalDeviceBufferDeviceAddressFeatures bda{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};bda.bufferDeviceAddress=VK_TRUE;
    asf.pNext=&rtf;rtf.pNext=&bda;
    const char* exts[]={VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME};
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};dci.pNext=&asf;dci.queueCreateInfoCount=1;dci.pQueueCreateInfos=&qci;dci.enabledExtensionCount=4;dci.ppEnabledExtensionNames=exts;
    if(vkCreateDevice(physical,&dci,nullptr,&device)!=VK_SUCCESS)throw std::runtime_error("vkCreateDevice failed.");
    VkQueue queue{};vkGetDeviceQueue(device,qFamily,0,&queue);

    struct Vertex{float x,y,z;};std::vector<Vertex> vertices;std::vector<uint32_t> indices;
    vertices.reserve(scene.triangles.size()*3);indices.reserve(scene.triangles.size()*3);
    for(const auto&t:scene.triangles){for(Vec3 p:{t.a,t.b,t.c}){indices.push_back((uint32_t)vertices.size());vertices.push_back({p.x,p.y,p.z});}}
    auto createUpload=[&](VkDeviceSize bytes,VkBufferUsageFlags usage,VkBuffer&b,VkDeviceMemory&m,const void*data){
      VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=bytes;bi.usage=usage|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;bi.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
      if(vkCreateBuffer(device,&bi,nullptr,&b)!=VK_SUCCESS)throw std::runtime_error("Input buffer create failed.");
      VkMemoryRequirements mr{};vkGetBufferMemoryRequirements(device,b,&mr);
      VkMemoryAllocateFlagsInfo fi{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};fi.flags=VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
      VkPhysicalDeviceMemoryProperties mp{};vkGetPhysicalDeviceMemoryProperties(physical,&mp);uint32_t mt=UINT32_MAX;
      for(uint32_t i=0;i<mp.memoryTypeCount;i++)if((mr.memoryTypeBits&(1u<<i))&&(mp.memoryTypes[i].propertyFlags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)){mt=i;break;}
      if(mt==UINT32_MAX)throw std::runtime_error("No host-visible coherent memory for AS input.");
      VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.pNext=&fi;ai.allocationSize=mr.size;ai.memoryTypeIndex=mt;
      if(vkAllocateMemory(device,&ai,nullptr,&m)!=VK_SUCCESS||vkBindBufferMemory(device,b,m,0)!=VK_SUCCESS)throw std::runtime_error("Input buffer memory allocation failed.");
      void*ptr=nullptr;if(vkMapMemory(device,m,0,bytes,0,&ptr)!=VK_SUCCESS)throw std::runtime_error("Input buffer map failed.");std::memcpy(ptr,data,(size_t)bytes);vkUnmapMemory(device,m);
    };
    createUpload(vertices.size()*sizeof(Vertex),VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,vertexBuffer,vertexMemory,vertices.data());
    createUpload(indices.size()*sizeof(uint32_t),VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,indexBuffer,indexMemory,indices.data());
    VkBufferDeviceAddressInfo vaddr{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};vaddr.buffer=vertexBuffer;
    VkBufferDeviceAddressInfo iaddr{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};iaddr.buffer=indexBuffer;
    VkAccelerationStructureGeometryKHR geom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    geom.geometryType=VK_GEOMETRY_TYPE_TRIANGLES_KHR;geom.flags=VK_GEOMETRY_OPAQUE_BIT_KHR;
    geom.geometry.triangles.sType=VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    geom.geometry.triangles.vertexFormat=VK_FORMAT_R32G32B32_SFLOAT;geom.geometry.triangles.vertexStride=sizeof(Vertex);
    geom.geometry.triangles.maxVertex=(uint32_t)vertices.size()-1;geom.geometry.triangles.vertexData.deviceAddress=vkGetBufferDeviceAddress(device,&vaddr);
    geom.geometry.triangles.indexType=VK_INDEX_TYPE_UINT32;geom.geometry.triangles.indexData.deviceAddress=vkGetBufferDeviceAddress(device,&iaddr);
    uint32_t primCount=(uint32_t)scene.triangles.size();
    VkAccelerationStructureBuildGeometryInfoKHR bi{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    bi.type=VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;bi.flags=VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;bi.mode=VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;bi.geometryCount=1;bi.pGeometries=&geom;
    VkAccelerationStructureBuildSizesInfoKHR sizes{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    auto getSizes=(PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(device,"vkGetAccelerationStructureBuildSizesKHR");
    auto createAS=(PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(device,"vkCreateAccelerationStructureKHR");
    auto cmdBuild=(PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(device,"vkCmdBuildAccelerationStructuresKHR");
    auto getAddr=(PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(device,"vkGetAccelerationStructureDeviceAddressKHR");
    if(!getSizes||!createAS||!cmdBuild||!getAddr)throw std::runtime_error("Required acceleration-structure function pointer missing.");
    getSizes(device,VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,&bi,&primCount,&sizes);
    auto createStorage=[&](VkDeviceSize sz,VkBuffer&b,VkDeviceMemory&m,VkBufferUsageFlags usage,bool needsAddress=false){
      VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};ci.size=sz;ci.usage=usage;ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
      if(vkCreateBuffer(device,&ci,nullptr,&b)!=VK_SUCCESS)throw std::runtime_error("AS storage buffer create failed.");
      VkMemoryRequirements mr{};vkGetBufferMemoryRequirements(device,b,&mr);VkPhysicalDeviceMemoryProperties mp{};vkGetPhysicalDeviceMemoryProperties(physical,&mp);uint32_t mt=UINT32_MAX;
      for(uint32_t i=0;i<mp.memoryTypeCount;i++)if((mr.memoryTypeBits&(1u<<i))&&(mp.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)){mt=i;break;}
      if(mt==UINT32_MAX)throw std::runtime_error("No device-local memory for AS.");
      VkMemoryAllocateFlagsInfo addressFlags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
      addressFlags.flags=needsAddress?VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT:0;
      VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.pNext=needsAddress?&addressFlags:nullptr;ai.allocationSize=mr.size;ai.memoryTypeIndex=mt;
      if(vkAllocateMemory(device,&ai,nullptr,&m)!=VK_SUCCESS||vkBindBufferMemory(device,b,m,0)!=VK_SUCCESS)throw std::runtime_error("AS storage memory allocation failed.");
    };
    createStorage(sizes.accelerationStructureSize,blasStorage,blasMem,VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR);
    VkAccelerationStructureCreateInfoKHR aci{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};aci.buffer=blasStorage;aci.size=sizes.accelerationStructureSize;aci.type=VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    if(createAS(device,&aci,nullptr,&blas)!=VK_SUCCESS)throw std::runtime_error("BLAS create failed.");
    createStorage(sizes.buildScratchSize,scratch,scratchMem,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,true);
    VkBufferDeviceAddressInfo sca{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};sca.buffer=scratch;
    bi.dstAccelerationStructure=blas;bi.scratchData.deviceAddress=vkGetBufferDeviceAddress(device,&sca);
    VkAccelerationStructureBuildRangeInfoKHR range{};range.primitiveCount=primCount;
    const VkAccelerationStructureBuildRangeInfoKHR* ranges[]={&range};

    VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pci.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;pci.queueFamilyIndex=qFamily;
    if(vkCreateCommandPool(device,&pci,nullptr,&pool)!=VK_SUCCESS)throw std::runtime_error("Command pool create failed.");
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};cai.commandPool=pool;cai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;cai.commandBufferCount=1;
    if(vkAllocateCommandBuffers(device,&cai,&cmd)!=VK_SUCCESS)throw std::runtime_error("Command buffer allocation failed.");
    VkCommandBufferBeginInfo cbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};cbi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd,&cbi);cmdBuild(cmd,1,&bi,ranges);vkEndCommandBuffer(cmd);
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};si.commandBufferCount=1;si.pCommandBuffers=&cmd;
    if(vkQueueSubmit(queue,1,&si,VK_NULL_HANDLE)!=VK_SUCCESS||vkQueueWaitIdle(queue)!=VK_SUCCESS)throw std::runtime_error("BLAS build submission failed.");

    VkAccelerationStructureDeviceAddressInfoKHR blasi{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};blasi.accelerationStructure=blas;
    VkAccelerationStructureInstanceKHR inst{};inst.transform.matrix[0][0]=1;inst.transform.matrix[1][1]=1;inst.transform.matrix[2][2]=1;
    inst.instanceCustomIndex=0;inst.mask=0xff;inst.instanceShaderBindingTableRecordOffset=0;inst.flags=VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;inst.accelerationStructureReference=getAddr(device,&blasi);
    createUpload(sizeof(inst),VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,instanceBuffer,instanceMem,&inst);
    VkBufferDeviceAddressInfo insa{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};insa.buffer=instanceBuffer;
    VkAccelerationStructureGeometryKHR tgeom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};tgeom.geometryType=VK_GEOMETRY_TYPE_INSTANCES_KHR;
    tgeom.geometry.instances.sType=VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;tgeom.geometry.instances.arrayOfPointers=VK_FALSE;tgeom.geometry.instances.data.deviceAddress=vkGetBufferDeviceAddress(device,&insa);
    VkAccelerationStructureBuildGeometryInfoKHR tbi{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};tbi.type=VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;tbi.flags=VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;tbi.mode=VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;tbi.geometryCount=1;tbi.pGeometries=&tgeom;
    uint32_t one=1;VkAccelerationStructureBuildSizesInfoKHR ts{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};getSizes(device,VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,&tbi,&one,&ts);
    createStorage(ts.accelerationStructureSize,tlasStorage,tlasMem,VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR);
    aci.buffer=tlasStorage;aci.size=ts.accelerationStructureSize;aci.type=VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    if(createAS(device,&aci,nullptr,&tlas)!=VK_SUCCESS)throw std::runtime_error("TLAS create failed.");
    if(ts.buildScratchSize>sizes.buildScratchSize){
      if(scratch)vkDestroyBuffer(device,scratch,nullptr);
      if(scratchMem)vkFreeMemory(device,scratchMem,nullptr);
      scratch=VK_NULL_HANDLE;scratchMem=VK_NULL_HANDLE;
      createStorage(ts.buildScratchSize,scratch,scratchMem,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,true);
    }
    VkBufferDeviceAddressInfo tsca{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};tsca.buffer=scratch;
    tbi.dstAccelerationStructure=tlas;tbi.scratchData.deviceAddress=vkGetBufferDeviceAddress(device,&tsca);
    VkAccelerationStructureBuildRangeInfoKHR trange{};trange.primitiveCount=1;const VkAccelerationStructureBuildRangeInfoKHR* tranges[]={&trange};
    vkResetCommandBuffer(cmd,0);vkBeginCommandBuffer(cmd,&cbi);cmdBuild(cmd,1,&tbi,tranges);vkEndCommandBuffer(cmd);
    if(vkQueueSubmit(queue,1,&si,VK_NULL_HANDLE)!=VK_SUCCESS||vkQueueWaitIdle(queue)!=VK_SUCCESS)throw std::runtime_error("TLAS build submission failed.");

    report.success=true;
    report.message="Built BLAS and TLAS for "+std::to_string(report.triangleCount)+" triangles on "+report.deviceName+". Ray tracing pipeline and dispatch are the next stage.";
    cleanup();return report;
  } catch(const std::exception&e) {
    report.message=std::string("Acceleration structure build failed: ")+e.what();
    cleanup();return report;
  }
#endif
}
}
