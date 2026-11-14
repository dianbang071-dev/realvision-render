#pragma once
#include "rv/vec3.hpp"
#include <string>
#include <vector>
#include <cstdint>
namespace rv {
struct ImageTexture{int width{},height{};std::vector<Vec3> pixels;bool valid()const{return width>0&&height>0&&pixels.size()==(size_t)width*(size_t)height;}Vec3 sample(float u,float v)const;};
struct Material{std::string name;Vec3 baseColor{.8f,.8f,.8f};Vec3 emission{};float roughness{.6f},metallic{},transmission{},ior{1.5f};std::string texturePath;ImageTexture texture;};
struct Triangle{Vec3 a,b,c;Vec3 uvA{},uvB{},uvC{};int material{};bool hasUV{};};
struct Camera{Vec3 origin{0,0,5},target{0,0,0},up{0,1,0};float fovDegrees{45};};
struct IESProfile{std::vector<float> verticalAngles,horizontalAngles,candela;bool valid()const{return !verticalAngles.empty()&&!horizontalAngles.empty()&&candela.size()==verticalAngles.size()*horizontalAngles.size();}float sample(float verticalDeg,float horizontalDeg)const;};
struct PhotometricLight{Vec3 position{};float multiplier{1.0f};IESProfile profile;};
struct EnvironmentMap{int width{},height{};std::vector<Vec3> pixels;bool valid()const{return width>0&&height>0&&pixels.size()==(size_t)width*(size_t)height;}};
struct SunLight{float azimuthDegrees{135.f},elevationDegrees{35.f},intensity{3.f};bool enabled{true};};
struct Scene{EnvironmentMap hdri;SunLight sun;std::vector<PhotometricLight> iesLights;std::vector<Material> materials;std::vector<Triangle> triangles;Camera camera;Vec3 environment{.18f,.18f,.18f};std::string hdriPath;};
bool loadIESProfile(const std::string&,IESProfile&,std::string&);
bool loadRadianceHdr(const std::string&,EnvironmentMap&,std::string&);
bool loadBmpTexture(const std::string&,ImageTexture&,std::string&);
bool loadPpmTexture(const std::string&,ImageTexture&,std::string&);
#ifdef RV_HAS_LIBPNG
bool loadPngTexture(const std::string&,ImageTexture&,std::string&);
#endif
#ifdef RV_HAS_LIBJPEG
bool loadJpegTexture(const std::string&,ImageTexture&,std::string&);
#endif
bool loadRvScene(const std::string&,Scene&,std::string&);
}
