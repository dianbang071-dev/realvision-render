#pragma once
#include "rv/scene.hpp"
#include <string>
namespace rv {struct RenderSettings{int width{1280},height{720},samples{16},maxBounces{4};float exposureEV{0.0f},bloom{0.18f};bool denoise{true};};bool renderCpuReference(const Scene&,const RenderSettings&,const std::string&,std::string&);}
