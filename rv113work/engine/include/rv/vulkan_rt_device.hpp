#pragma once
#include <string>
namespace rv {
struct RtDeviceReport {
  bool success{false};
  std::string name;
  std::string message;
  std::string details;
};
RtDeviceReport initializeRayTracingDevice();
}
