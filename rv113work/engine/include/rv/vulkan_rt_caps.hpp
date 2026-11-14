#pragma once
#include <string>
namespace rv {
struct RayTracingCaps {
  bool loaderAvailable{false};
  bool deviceAvailable{false};
  bool accelerationStructure{false};
  bool rayTracingPipeline{false};
  bool deferredHostOperations{false};
  bool bufferDeviceAddress{false};
  std::string deviceName;
  std::string report;
};
RayTracingCaps queryRayTracingCaps();
}
