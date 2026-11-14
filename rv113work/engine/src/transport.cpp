#include "rv/transport.hpp"
#include <fstream>
#include <vector>
#include <random>
#include <algorithm>
#include <cmath>
namespace rv{
struct Ray{Vec3 o,d;};struct Hit{float t=1e30f;Vec3 p,n;int mat=-1;bool frontFace=true;int triIndex=-1;float baryU{},baryV{};};
static bool hitTri(const Ray&r,const Triangle&t,float&d,Vec3&n,bool&frontFace,float&outU,float&outV){Vec3 e1=t.b-t.a,e2=t.c-t.a,p=cross(r.d,e2);float det=dot(e1,p);if(std::abs(det)<1e-8f)return false;float inv=1/det;Vec3 s=r.o-t.a;float u=dot(s,p)*inv;if(u<0||u>1)return false;Vec3 q=cross(s,e1);float v=dot(r.d,q)*inv;if(v<0||u+v>1)return false;d=dot(e2,q)*inv;if(d<1e-4f)return false;outU=u;outV=v;Vec3 outward=normalize(cross(e1,e2));frontFace=dot(r.d,outward)<0.f;n=frontFace?outward:outward*-1.f;return true;}
struct Aabb { Vec3 lo{1e30f,1e30f,1e30f}, hi{-1e30f,-1e30f,-1e30f};
 void expand(Vec3 p){lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};}
 void expand(const Aabb& b){expand(b.lo);expand(b.hi);}
};
struct BvhNode { Aabb bounds; int left{-1},right{-1},start{},count{}; };
struct SceneBvh {
 std::vector<BvhNode> nodes; std::vector<int> indices; const Scene* scene{};
 static Aabb triangleBounds(const Triangle&t){Aabb b;b.expand(t.a);b.expand(t.b);b.expand(t.c);return b;}
 static Vec3 centroid(const Triangle&t){return (t.a+t.b+t.c)/3.f;}
 int build(int begin,int end){
   int id=(int)nodes.size();nodes.emplace_back();Aabb bounds,centers;
   for(int i=begin;i<end;i++){const auto&t=scene->triangles[(size_t)indices[i]];bounds.expand(triangleBounds(t));centers.expand(centroid(t));}
   nodes[id].bounds=bounds;int count=end-begin;
   if(count<=4){nodes[id].start=begin;nodes[id].count=count;return id;}
   Vec3 extent=centers.hi-centers.lo;int axis=extent.y>extent.x?1:0;if((axis==0?extent.x:extent.y)<extent.z)axis=2;
   int mid=begin+count/2;
   std::nth_element(indices.begin()+begin,indices.begin()+mid,indices.begin()+end,[&](int a,int b){Vec3 x=centroid(scene->triangles[(size_t)a]),y=centroid(scene->triangles[(size_t)b]);return axis==0?x.x<y.x:(axis==1?x.y<y.y:x.z<y.z);});
   int l=build(begin,mid),r=build(mid,end);nodes[id].left=l;nodes[id].right=r;return id;
 }
 explicit SceneBvh(const Scene&s):scene(&s){indices.resize(s.triangles.size());for(size_t i=0;i<indices.size();++i)indices[i]=(int)i;if(!indices.empty())build(0,(int)indices.size());}
 static bool boxHit(const Aabb&b,const Ray&r,float maxT){float tmin=1e-4f,tmax=maxT;for(int a=0;a<3;a++){float o=a==0?r.o.x:(a==1?r.o.y:r.o.z),d=a==0?r.d.x:(a==1?r.d.y:r.d.z),mn=a==0?b.lo.x:(a==1?b.lo.y:b.lo.z),mx=a==0?b.hi.x:(a==1?b.hi.y:b.hi.z);if(std::abs(d)<1e-9f){if(o<mn||o>mx)return false;continue;}float inv=1.f/d,t0=(mn-o)*inv,t1=(mx-o)*inv;if(t0>t1)std::swap(t0,t1);tmin=std::max(tmin,t0);tmax=std::min(tmax,t1);if(tmax<tmin)return false;}return true;}
 bool intersect(const Ray&r,Hit&h)const{if(nodes.empty())return false;bool ok=false;std::vector<int> stack;stack.push_back(0);while(!stack.empty()){int ni=stack.back();stack.pop_back();const auto&node=nodes[(size_t)ni];if(!boxHit(node.bounds,r,h.t))continue;if(node.count){for(int k=0;k<node.count;k++){int ti=indices[(size_t)(node.start+k)];const auto&t=scene->triangles[(size_t)ti];float d,u,v;Vec3 n;bool front;if(hitTri(r,t,d,n,front,u,v)&&d<h.t){h.t=d;h.p=r.o+r.d*d;h.n=n;h.mat=t.material;h.frontFace=front;h.triIndex=ti;h.baryU=u;h.baryV=v;ok=true;}}}else{stack.push_back(node.left);stack.push_back(node.right);}}return ok;}
};
static bool intersect(const SceneBvh&bvh,const Ray&r,Hit&h){return bvh.intersect(r,h);}
static Vec3 sampleDir(Vec3 n,std::mt19937&g){std::uniform_real_distribution<float>u(0,1);float a=6.2831853f*u(g),z=std::sqrt(u(g)),r=std::sqrt(1-z*z);Vec3 h=std::abs(n.z)<.999f?Vec3{0,0,1}:Vec3{0,1,0};Vec3 x=normalize(cross(h,n)),y=cross(n,x);return normalize(x*(r*std::cos(a))+y*(r*std::sin(a))+n*z);}
static Vec3 environmentRadiance(const Scene&s,Vec3 d){
 if(!s.hdri.valid()) {
   // Procedural daylight sky: zenith-to-horizon gradient with a warm horizon band.
   float h=std::clamp(d.z*0.5f+0.5f,0.f,1.f);
   Vec3 horizon{0.72f,0.82f,1.00f},zenith{0.12f,0.32f,0.72f};
   float t=std::pow(h,0.65f);
   Vec3 sky=horizon*(1.f-t)+zenith*t;
   if(d.z<0.f){float g=std::clamp(-d.z*2.f,0.f,1.f);sky=sky*(1.f-g)+s.environment*g;}
   return sky*0.65f;
 }
 const float pi=3.14159265358979323846f;
 float u=std::atan2(d.z,d.x)/(2*pi)+0.5f; float v=std::acos(std::clamp(d.y,-1.f,1.f))/pi;
 int x=std::clamp((int)(u*s.hdri.width),0,s.hdri.width-1), y=std::clamp((int)(v*s.hdri.height),0,s.hdri.height-1);
 return s.hdri.pixels[(size_t)y*s.hdri.width+x];
}
static Vec3 fresnelSchlick(float cosTheta,Vec3 f0){float x=std::pow(std::clamp(1.f-cosTheta,0.f,1.f),5.f);return f0+(Vec3{1,1,1}-f0)*x;}
static Vec3 evaluatePBR(const Material&m,Vec3 n,Vec3 v,Vec3 l){
 const float pi=3.14159265358979323846f;
 float ndl=std::max(0.f,dot(n,l)),ndv=std::max(1e-4f,dot(n,v));if(ndl<=0)return{};
 Vec3 h=normalize(v+l);float ndh=std::max(0.f,dot(n,h)),vdh=std::max(0.f,dot(v,h));
 float rough=std::clamp(m.roughness,.045f,1.f),a=rough*rough,a2=a*a;
 float denom=ndh*ndh*(a2-1.f)+1.f;float D=a2/(pi*denom*denom+1e-7f);
 auto smith=[&](float nd){float k=(rough+1.f)*(rough+1.f)/8.f;return nd/(nd*(1.f-k)+k);};
 float G=smith(ndv)*smith(ndl);
 Vec3 f0=Vec3{.04f,.04f,.04f}*(1.f-std::clamp(m.metallic,0.f,1.f))+m.baseColor*std::clamp(m.metallic,0.f,1.f);
 Vec3 F=fresnelSchlick(vdh,f0);Vec3 spec=F*(D*G/(4.f*ndv*ndl+1e-6f));
 Vec3 kd=Vec3{1,1,1}-F;kd=kd*(1.f-std::clamp(m.metallic,0.f,1.f));
 Vec3 diffuse=hadamard(kd,m.baseColor)*(1.f/pi);
 return (diffuse+spec)*ndl;
}
static Vec3 directSun(const Scene&s,const SceneBvh&bvh,const Hit&h,const Material&m,Vec3 view){
 if(!s.sun.enabled||s.sun.intensity<=0.f)return{};
 const float rad=3.14159265358979323846f/180.f;
 float az=s.sun.azimuthDegrees*rad,el=s.sun.elevationDegrees*rad;
 Vec3 wi=normalize(Vec3{std::sin(az)*std::cos(el),std::cos(az)*std::cos(el),std::sin(el)});
 float ndl=std::max(0.f,dot(h.n,wi));if(ndl<=0.f)return{};
 Ray shadow{h.p+h.n*1e-4f,wi};Hit blocker;if(intersect(bvh,shadow,blocker))return{};
 return evaluatePBR(m,h.n,view,wi)*s.sun.intensity;
}
static Vec3 directIES(const Scene&s,const SceneBvh&bvh,const Hit&h,const Material&m,Vec3 view){
 Vec3 sum{};const float pi=3.14159265358979323846f,rad=pi/180.f;
 for(const auto& light:s.iesLights){Vec3 delta=light.position-h.p;float d2=dot(delta,delta);if(d2<1e-8f)continue;float dist=std::sqrt(d2);Vec3 wi=delta/dist;float ndl=std::max(0.f,dot(h.n,wi));if(ndl<=0)continue;
 Ray shadow{h.p+h.n*1e-4f,wi};Hit blocker;if(intersect(bvh,shadow,blocker)&&blocker.t<dist-2e-4f)continue;
 Vec3 fromLight=wi*-1.f;float vertical=std::acos(std::clamp(dot(fromLight,Vec3{0,-1,0}),-1.f,1.f))/rad;
 float horizontal=std::atan2(fromLight.x,fromLight.z)/rad;float cd=light.profile.sample(vertical,horizontal)*light.multiplier;
 float irradiance=cd/d2;sum+=evaluatePBR(m,h.n,view,wi)*irradiance;
 }return sum;
}
static Vec3 directEmissive(const Scene&s,const SceneBvh&bvh,const Hit&h,const Material&m,Vec3 view,std::mt19937&g){
 // One area-weighted next-event sample from all emissive triangles.
 float totalArea=0.f;
 for(const auto&t:s.triangles){if(t.material<0||t.material>=(int)s.materials.size())continue;const auto&e=s.materials[t.material].emission;if(e.x<=0&&e.y<=0&&e.z<=0)continue;totalArea+=0.5f*length(cross(t.b-t.a,t.c-t.a));}
 if(totalArea<=1e-8f)return{};
 std::uniform_real_distribution<float> u(0.f,1.f);float pick=u(g)*totalArea;const Triangle*chosen=nullptr;float area=0.f;
 for(const auto&t:s.triangles){if(t.material<0||t.material>=(int)s.materials.size())continue;const auto&e=s.materials[t.material].emission;if(e.x<=0&&e.y<=0&&e.z<=0)continue;float a=0.5f*length(cross(t.b-t.a,t.c-t.a));if(a<=1e-10f)continue;if(pick<=a){chosen=&t;area=a;break;}pick-=a;}
 if(!chosen)return{};
 float su=std::sqrt(u(g)),v=u(g);Vec3 lp=chosen->a*(1.f-su)+chosen->b*(su*(1.f-v))+chosen->c*(su*v);
 Vec3 ln=normalize(cross(chosen->b-chosen->a,chosen->c-chosen->a));Vec3 delta=lp-h.p;float d2=dot(delta,delta);if(d2<=1e-8f)return{};float dist=std::sqrt(d2);Vec3 wi=delta/dist;
 float cosSurface=std::max(0.f,dot(h.n,wi));float cosLight=std::max(0.f,dot(ln,wi*-1.f));if(cosSurface<=0||cosLight<=0)return{};
 Ray shadow{h.p+h.n*1e-4f,wi};Hit blocker;if(intersect(bvh,shadow,blocker)&&blocker.t<dist-2e-4f)return{};
 const Vec3 Le=s.materials[chosen->material].emission;Vec3 f=evaluatePBR(m,h.n,view,wi);
 return hadamard(Le,f)*(cosSurface*cosLight*totalArea/(d2+1e-8f));
}
static Vec3 reflectDir(Vec3 d,Vec3 n){return normalize(d-n*(2.f*dot(d,n)));}
static bool refractDir(Vec3 d,Vec3 n,float eta,Vec3&out){float cosI=std::clamp(-dot(d,n),-1.f,1.f);float sin2=eta*eta*(1.f-cosI*cosI);if(sin2>=1.f)return false;out=normalize(d*eta+n*(eta*cosI-std::sqrt(1.f-sin2)));return true;}
static float dielectricFresnel(float cosI,float etaI,float etaT){float r0=(etaI-etaT)/(etaI+etaT);r0*=r0;return r0+(1.f-r0)*std::pow(1.f-std::clamp(cosI,0.f,1.f),5.f);}
static Vec3 trace(const Scene&s,const SceneBvh&bvh,Ray ray,int bounces,std::mt19937&g){
 Vec3 L{},beta{1,1,1};std::uniform_real_distribution<float>u(0.f,1.f);
 for(int b=0;b<bounces;b++){
   Hit h;if(!intersect(bvh,ray,h)){L+=hadamard(beta,environmentRadiance(s,ray.d));break;}
   auto&m=s.materials[h.mat];Material shaded;shaded.name=m.name;shaded.baseColor=m.baseColor;shaded.emission=m.emission;shaded.roughness=m.roughness;shaded.metallic=m.metallic;shaded.transmission=m.transmission;shaded.ior=m.ior;
   if(m.texture.valid()&&h.triIndex>=0){const Triangle& face=s.triangles[(size_t)h.triIndex];if(face.hasUV){float bw=1.f-h.baryU-h.baryV;Vec3 uv=face.uvA*bw+face.uvB*h.baryU+face.uvC*h.baryV;shaded.baseColor=hadamard(m.baseColor,m.texture.sample(uv.x,uv.y));}}
   const Material& mat=shaded;Vec3 view=ray.d*-1.f;
   L+=hadamard(beta,m.emission+directSun(s,bvh,h,mat,view)+directIES(s,bvh,h,mat,view)+directEmissive(s,bvh,h,mat,view,g));
   float transmission=std::clamp(mat.transmission,0.f,1.f);
   if(transmission>0.f){
     float etaI=h.frontFace?1.f:std::max(1.0001f,mat.ior),etaT=h.frontFace?std::max(1.0001f,mat.ior):1.f;
     float eta=etaI/etaT,cosI=std::clamp(-dot(ray.d,h.n),0.f,1.f);
     float F=dielectricFresnel(cosI,etaI,etaT);Vec3 refracted;
     bool canRefract=refractDir(ray.d,h.n,eta,refracted);
     float reflectProbability=canRefract?F:1.f;
     float transmitProbability=transmission*(1.f-reflectProbability);
     float choice=u(g);
     if(choice<transmitProbability&&canRefract){
       beta=hadamard(beta,mat.baseColor)*((1.f-F)*transmission/std::max(transmitProbability,1e-5f));
       ray={h.p+refracted*2e-4f,refracted};
     }else if(choice<transmitProbability+reflectProbability){
       Vec3 reflected=reflectDir(ray.d,h.n);ray={h.p+reflected*2e-4f,reflected};
     }else{
       float diffuseProbability=std::max(1e-5f,1.f-transmission);
       beta=hadamard(beta,mat.baseColor)*(diffuseProbability/std::max(1e-5f,1.f-transmitProbability-reflectProbability));
       ray={h.p+h.n*1e-4f,sampleDir(h.n,g)};
     }
   }else{
     beta=hadamard(beta,mat.baseColor);ray={h.p+h.n*1e-4f,sampleDir(h.n,g)};
   }
   if(b>1){float p=std::clamp(std::max({beta.x,beta.y,beta.z}),.05f,.95f);if(u(g)>p)break;beta=beta/p;}
 }
 return L;
}
bool renderCpuReference(const Scene&s,const RenderSettings&c,const std::string&out,std::string&err){
 if(s.triangles.empty()){err="Scene contains no triangles";return false;}SceneBvh bvh(s);int w=c.width,h=c.height;std::vector<unsigned char>pix((size_t)w*h*3);
 Vec3 f=normalize(s.camera.target-s.camera.origin),r=normalize(cross(f,s.camera.up)),u=cross(r,f);float scale=std::tan(s.camera.fovDegrees*3.14159265f/360),aspect=float(w)/h;
 const float exposure=std::exp2(std::clamp(c.exposureEV,-10.f,10.f));
 auto aces=[&](float x){x=std::max(0.f,x*exposure);float a=2.51f,b=.03f,cc=2.43f,d=.59f,e=.14f;return std::clamp((x*(a*x+b))/(x*(cc*x+d)+e),0.f,1.f);};
 auto encode=[&](float x){return (unsigned char)(std::pow(aces(x),1.f/2.2f)*255.f+.5f);};
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){Vec3 sum{};std::mt19937 rng((unsigned)(x+y*w+41));std::uniform_real_distribution<float> jitter(-.5f,.5f);for(int k=0;k<std::max(1,c.samples);k++){float jx=(c.samples>1?jitter(rng):0.f),jy=(c.samples>1?jitter(rng):0.f);float sx=(2*((x+.5f+jx)/w)-1)*aspect*scale,sy=(1-2*((y+.5f+jy)/h))*scale;sum+=trace(s,bvh,{s.camera.origin,normalize(f+r*sx+u*sy)},c.maxBounces,rng);}Vec3 v=sum/float(std::max(1,c.samples));size_t i=((size_t)y*w+x)*3;pix[i]=encode(v.x);pix[i+1]=encode(v.y);pix[i+2]=encode(v.z);}
 // Edge-aware 3x3 CPU denoiser. It smooths small sampling noise while preserving strong color edges.
 if(c.denoise){
   std::vector<unsigned char> filtered=pix;
   for(int y=1;y<h-1;y++)for(int x=1;x<w-1;x++){
     const size_t center=((size_t)y*w+x)*3; float sums[3]={0,0,0}, weights=0;
     for(int oy=-1;oy<=1;oy++)for(int ox=-1;ox<=1;ox++){
       size_t j=((size_t)(y+oy)*w+(x+ox))*3;
       float dr=float(pix[j])-pix[center], dg=float(pix[j+1])-pix[center+1], db=float(pix[j+2])-pix[center+2];
       float spatial=(ox==0&&oy==0)?1.f:((ox==0||oy==0)?0.7f:0.5f);
       float range=std::exp(-(dr*dr+dg*dg+db*db)/(2.f*22.f*22.f)); float wt=spatial*range; weights+=wt;
       for(int ch=0;ch<3;ch++)sums[ch]+=float(pix[j+ch])*wt;
     }
     for(int ch=0;ch<3;ch++)filtered[center+ch]=(unsigned char)std::clamp(sums[ch]/weights,0.f,255.f);
   }
   pix.swap(filtered);
 }
 // Screen-space bloom: threshold bright pixels, apply separable box blur, then add a restrained glow.
 if(c.bloom>0.001f){
   std::vector<float> bright((size_t)w*h*3,0.f),tmp((size_t)w*h*3,0.f);
   for(size_t i=0;i<(size_t)w*h;i++){float lum=.2126f*pix[i*3]+.7152f*pix[i*3+1]+.0722f*pix[i*3+2];float gate=std::clamp((lum-190.f)/65.f,0.f,1.f);for(int ch=0;ch<3;ch++)bright[i*3+ch]=pix[i*3+ch]*gate;}
   const int radius=5;
   for(int y=0;y<h;y++)for(int x=0;x<w;x++)for(int ch=0;ch<3;ch++){float sum=0;int n=0;for(int dx=-radius;dx<=radius;dx++){int xx=std::clamp(x+dx,0,w-1);sum+=bright[((size_t)y*w+xx)*3+ch];n++;}tmp[((size_t)y*w+x)*3+ch]=sum/n;}
   for(int y=0;y<h;y++)for(int x=0;x<w;x++)for(int ch=0;ch<3;ch++){float sum=0;int n=0;for(int dy=-radius;dy<=radius;dy++){int yy=std::clamp(y+dy,0,h-1);sum+=tmp[((size_t)yy*w+x)*3+ch];n++;}size_t i=((size_t)y*w+x)*3+ch;float glow=sum/n;pix[i]=(unsigned char)std::clamp(float(pix[i])+glow*std::clamp(c.bloom,0.f,2.f)*.45f,0.f,255.f);}
 }
 std::ofstream fimg(out,std::ios::binary);if(!fimg){err="Cannot write output";return false;}
 auto dotPos=out.find_last_of('.');
 std::string ext=dotPos==std::string::npos?std::string():out.substr(dotPos+1);
 if(ext=="bmp"||ext=="BMP"){
   const uint32_t row=((uint32_t)w*3u+3u)&~3u, dataSize=row*(uint32_t)h;
   auto w16=[&](uint16_t v){fimg.put((char)(v&255));fimg.put((char)(v>>8));};
   auto w32=[&](uint32_t v){w16((uint16_t)(v&65535));w16((uint16_t)(v>>16));};
   fimg.put('B');fimg.put('M');w32(54u+dataSize);w16(0);w16(0);w32(54);
   w32(40);w32((uint32_t)w);w32((uint32_t)h);w16(1);w16(24);w32(0);w32(dataSize);
   w32(2835);w32(2835);w32(0);w32(0);
   std::vector<unsigned char> rowbuf(row,0);
   for(int y=h-1;y>=0;y--){
     for(int x=0;x<w;x++){size_t i=((size_t)y*w+x)*3;rowbuf[x*3]=pix[i+2];rowbuf[x*3+1]=pix[i+1];rowbuf[x*3+2]=pix[i];}
     fimg.write((char*)rowbuf.data(),row);
   }
 } else {
   fimg<<"P6\n"<<w<<" "<<h<<"\n255\n";fimg.write((char*)pix.data(),pix.size());
 }
 if(!fimg){err="Failed while writing image data";return false;}return true;
}}
