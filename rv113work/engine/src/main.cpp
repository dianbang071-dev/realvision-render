#include "rv/scene.hpp"
#include "rv/transport.hpp"
#include "rv/rvlink.hpp"
#include "rv/vulkan_probe.hpp"
#include "rv/vulkan_rt_caps.hpp"
#include "rv/vulkan_rt_device.hpp"
#include "rv/vulkan_as.hpp"
#include <iostream>
#include <string>
int main(int argc,char**argv){
 if(argc>1&&std::string(argv[1])=="--bridge")return rv::runBridgeServer(argc>2?(unsigned short)std::stoi(argv[2]):47831);
 if(argc>1&&std::string(argv[1])=="--gpu-info"){std::cout<<rv::vulkanStatus()<<"\n"<<rv::queryRayTracingCaps().report;return 0;}
 if(argc>1&&std::string(argv[1])=="--rt-init"){auto r=rv::initializeRayTracingDevice();std::cout<<r.message<<"\n"<<r.details;return r.success?0:5;}
 if(argc>2&&std::string(argv[1])=="--build-as"){rv::Scene scene;std::string error;if(!rv::loadRvScene(argv[2],scene,error)){std::cerr<<error<<"\\n";return 2;}auto r=rv::buildSceneAccelerationStructures(scene);std::cout<<r.message<<"\nGPU: "<<r.deviceName<<"\nTriangles: "<<r.triangleCount<<"\n";return r.success?0:6;}
 std::string scene,out="rv_output.ppm",hdri,ies;float iesX=0.f,iesY=2.f,iesZ=0.f,iesPower=1.f;rv::RenderSettings cfg;
 for(int i=1;i<argc;i++){std::string a=argv[i];if(a=="--scene"&&i+1<argc)scene=argv[++i];else if(a=="--out"&&i+1<argc)out=argv[++i];else if(a=="--samples"&&i+1<argc)cfg.samples=std::stoi(argv[++i]);else if(a=="--width"&&i+1<argc)cfg.width=std::stoi(argv[++i]);else if(a=="--height"&&i+1<argc)cfg.height=std::stoi(argv[++i]);else if(a=="--no-denoise")cfg.denoise=false;else if(a=="--exposure"&&i+1<argc)cfg.exposureEV=std::stof(argv[++i]);else if(a=="--bloom"&&i+1<argc)cfg.bloom=std::stof(argv[++i]);else if(a=="--hdri"&&i+1<argc)hdri=argv[++i];else if(a=="--ies"&&i+1<argc)ies=argv[++i];else if(a=="--ies-x"&&i+1<argc)iesX=std::stof(argv[++i]);else if(a=="--ies-y"&&i+1<argc)iesY=std::stof(argv[++i]);else if(a=="--ies-z"&&i+1<argc)iesZ=std::stof(argv[++i]);else if(a=="--ies-power"&&i+1<argc)iesPower=std::stof(argv[++i]);}
 if(scene.empty()){std::cout<<"REALVISION RENDER 1.14\n--gpu-info | --rt-init | --build-as file.rvscene | --bridge [port] | --scene file.rvscene --out image.bmp|image.ppm [--hdri environment.hdr] [--ies luminaire.ies] [--exposure EV] [--bloom 0..2]\n";return 0;}
 rv::Scene s;std::string err;if(!rv::loadRvScene(scene,s,err)){std::cerr<<err<<"\n";return 2;}
 if(!hdri.empty()&&!rv::loadRadianceHdr(hdri,s.hdri,err)){std::cerr<<err<<"\n";return 4;}
 if(!ies.empty()){rv::PhotometricLight l;l.position={iesX,iesY,iesZ};l.multiplier=iesPower;if(!rv::loadIESProfile(ies,l.profile,err)){std::cerr<<err<<"\n";return 7;}s.iesLights.push_back(std::move(l));}
 if(!rv::renderCpuReference(s,cfg,out,err)){std::cerr<<err<<"\n";return 3;}std::cout<<"CPU reference output (ACES tone map, exposure, bloom, denoise): "<<out<<"\n";return 0;
}
