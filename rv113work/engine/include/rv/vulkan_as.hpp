#pragma once
#include "rv/scene.hpp"
#include <string>
namespace rv {
struct AccelerationBuildReport {
  bool success{false};
  std::size_t triangleCount{0};
  std::string deviceName;
  std::string message;
};
// Creates a Vulkan RT device and builds one bottom-level and one top-level
// acceleration structure from the triangles in the supplied RVSCENE.
AccelerationBuildReport buildSceneAccelerationStructures(const Scene& scene);
}
