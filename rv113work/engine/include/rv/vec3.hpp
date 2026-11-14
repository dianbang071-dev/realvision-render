#pragma once
#include <cmath>
#include <algorithm>
namespace rv {
struct Vec3{float x{},y{},z{};Vec3()=default;Vec3(float a,float b,float c):x(a),y(b),z(c){}
Vec3 operator+(Vec3 b)const{return{x+b.x,y+b.y,z+b.z};} Vec3 operator-(Vec3 b)const{return{x-b.x,y-b.y,z-b.z};}
Vec3 operator*(float s)const{return{x*s,y*s,z*s};} Vec3 operator/(float s)const{return{x/s,y/s,z/s};}
Vec3& operator+=(Vec3 b){x+=b.x;y+=b.y;z+=b.z;return *this;}};
inline Vec3 operator*(float s,Vec3 v){return v*s;} inline float dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec3 cross(Vec3 a,Vec3 b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float length(Vec3 v){return std::sqrt(dot(v,v));} inline Vec3 normalize(Vec3 v){float l=length(v);return l>1e-20f?v/l:Vec3{};}
inline Vec3 hadamard(Vec3 a,Vec3 b){return{a.x*b.x,a.y*b.y,a.z*b.z};}
}
