#include "rv/scene.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <cstdio>

#ifdef RV_HAS_LIBPNG
#include <png.h>
#endif
#ifdef RV_HAS_LIBJPEG
extern "C" {
#include <jpeglib.h>
}
#include <csetjmp>
#endif
namespace rv {


Vec3 ImageTexture::sample(float u,float v)const{
 if(!valid())return {1,1,1};
 u-=std::floor(u);v-=std::floor(v);float fx=u*(width-1),fy=(1.f-v)*(height-1);
 int x0=(int)std::floor(fx),y0=(int)std::floor(fy),x1=std::min(x0+1,width-1),y1=std::min(y0+1,height-1);
 float tx=fx-x0,ty=fy-y0;const Vec3&a=pixels[(size_t)y0*width+x0],&b=pixels[(size_t)y0*width+x1],&c=pixels[(size_t)y1*width+x0],&d=pixels[(size_t)y1*width+x1];
 return (a*(1-tx)+b*tx)*(1-ty)+(c*(1-tx)+d*tx)*ty;
}
static uint16_t rd16(std::istream&f){unsigned char b[2]{};f.read((char*)b,2);return (uint16_t)(b[0]|(b[1]<<8));}
static uint32_t rd32(std::istream&f){unsigned char b[4]{};f.read((char*)b,4);return (uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);}
bool loadBmpTexture(const std::string&path,ImageTexture&out,std::string&err){
 std::ifstream f(path,std::ios::binary);if(!f){err="Cannot open bitmap texture: "+path;return false;}
 if(f.get()!='B'||f.get()!='M'){err="Texture must be BMP (24/32-bit uncompressed)";return false;}
 (void)rd32(f);(void)rd16(f);(void)rd16(f);uint32_t offset=rd32(f),dib=rd32(f);
 if(dib<40){err="Unsupported BMP DIB header";return false;}int32_t w=(int32_t)rd32(f),sh=(int32_t)rd32(f);uint16_t planes=rd16(f),bits=rd16(f);uint32_t compression=rd32(f);
 if(w<1||sh==0||w>16384||std::abs(sh)>16384||planes!=1||(bits!=24&&bits!=32)||compression!=0){err="BMP must be uncompressed 24/32-bit";return false;}
 (void)rd32(f);(void)rd32(f);(void)rd32(f);(void)rd32(f);(void)rd32(f);(void)rd32(f);
 out.width=w;out.height=std::abs(sh);out.pixels.assign((size_t)out.width*out.height,Vec3{});int bytes=bits/8;uint32_t stride=((uint32_t)w*bytes+3u)&~3u;std::vector<unsigned char> row(stride);f.seekg(offset);
 for(int y=0;y<out.height;y++){if(!f.read((char*)row.data(),stride)){err="Truncated BMP texture";return false;}int dst=sh>0?out.height-1-y:y;for(int x=0;x<w;x++){auto*q=&row[(size_t)x*bytes];out.pixels[(size_t)dst*w+x]={q[2]/255.f,q[1]/255.f,q[0]/255.f};}}
 return true;
}

static bool ppmToken(std::istream& f,std::string& token){
 token.clear();char c=0;
 while(f.get(c)){if(c=='#'){std::string ignored;std::getline(f,ignored);continue;}if(!std::isspace((unsigned char)c)){token.push_back(c);break;}}
 if(token.empty())return false;
 while(f.get(c)){if(std::isspace((unsigned char)c))break;if(c=='#'){std::string ignored;std::getline(f,ignored);break;}token.push_back(c);}
 return true;
}
bool loadPpmTexture(const std::string&path,ImageTexture&out,std::string&err){
 std::ifstream f(path,std::ios::binary);if(!f){err="Cannot open PPM texture: "+path;return false;}
 std::string magic,sw,sh,sm;if(!ppmToken(f,magic)||!ppmToken(f,sw)||!ppmToken(f,sh)||!ppmToken(f,sm)||(magic!="P3"&&magic!="P6")){err="PPM must be P3 or P6";return false;}
 int w=0,h=0,maxv=0;try{w=std::stoi(sw);h=std::stoi(sh);maxv=std::stoi(sm);}catch(...){err="Invalid PPM header";return false;}
 if(w<1||h<1||w>16384||h>16384||maxv<1||maxv>255){err="PPM dimensions/max value unsupported (max value 255)";return false;}
 out.width=w;out.height=h;out.pixels.assign((size_t)w*h,Vec3{});
 if(magic=="P3"){
  for(auto&px:out.pixels){std::string a,b,c;if(!ppmToken(f,a)||!ppmToken(f,b)||!ppmToken(f,c)){err="Truncated P3 texture";return false;}try{px={std::clamp(std::stoi(a),0,maxv)/(float)maxv,std::clamp(std::stoi(b),0,maxv)/(float)maxv,std::clamp(std::stoi(c),0,maxv)/(float)maxv};}catch(...){err="Invalid P3 pixel";return false;}}
 }else{
  std::vector<unsigned char> raw((size_t)w*h*3);if(!f.read((char*)raw.data(),(std::streamsize)raw.size())){err="Truncated P6 texture";return false;}
  for(size_t i=0;i<out.pixels.size();++i)out.pixels[i]={(raw[i*3]/(float)maxv),(raw[i*3+1]/(float)maxv),(raw[i*3+2]/(float)maxv)};
 }
 return true;
}


#ifdef RV_HAS_LIBPNG
bool loadPngTexture(const std::string& path,ImageTexture& out,std::string& err){
 png_image image{}; image.version=PNG_IMAGE_VERSION;
 if(!png_image_begin_read_from_file(&image,path.c_str())){err="Cannot read PNG texture: "+path;return false;}
 image.format=PNG_FORMAT_RGB;
 if(image.width==0||image.height==0||image.width>16384||image.height>16384){
  png_image_free(&image);err="PNG dimensions unsupported (maximum 16384 x 16384)";return false;
 }
 std::vector<unsigned char> bytes(PNG_IMAGE_SIZE(image));
 if(!png_image_finish_read(&image,nullptr,bytes.data(),0,nullptr)){
  err=std::string("PNG decode failed: ")+image.message;png_image_free(&image);return false;
 }
 out.width=(int)image.width;out.height=(int)image.height;
 out.pixels.resize((size_t)out.width*out.height);
 for(size_t i=0;i<out.pixels.size();++i)out.pixels[i]={bytes[i*3]/255.f,bytes[i*3+1]/255.f,bytes[i*3+2]/255.f};
 png_image_free(&image);return true;
}
#endif

#ifdef RV_HAS_LIBJPEG
struct RvJpegError{jpeg_error_mgr base;jmp_buf jump;char message[JMSG_LENGTH_MAX]{};};
static void rvJpegErrorExit(j_common_ptr cinfo){
 auto* e=(RvJpegError*)cinfo->err;
 (*cinfo->err->format_message)(cinfo,e->message);
 longjmp(e->jump,1);
}
bool loadJpegTexture(const std::string& path,ImageTexture& out,std::string& err){
 FILE* file=std::fopen(path.c_str(),"rb");if(!file){err="Cannot open JPEG texture: "+path;return false;}
 jpeg_decompress_struct cinfo{};RvJpegError jerr{};
 cinfo.err=jpeg_std_error(&jerr.base);jerr.base.error_exit=rvJpegErrorExit;
 if(setjmp(jerr.jump)){jpeg_destroy_decompress(&cinfo);std::fclose(file);err=std::string("JPEG decode failed: ")+jerr.message;return false;}
 jpeg_create_decompress(&cinfo);jpeg_stdio_src(&cinfo,file);jpeg_read_header(&cinfo,TRUE);
 cinfo.out_color_space=JCS_RGB;jpeg_start_decompress(&cinfo);
 if(cinfo.output_width==0||cinfo.output_height==0||cinfo.output_width>16384||cinfo.output_height>16384||cinfo.output_components!=3){
  jpeg_destroy_decompress(&cinfo);std::fclose(file);err="JPEG dimensions/color format unsupported";return false;
 }
 out.width=(int)cinfo.output_width;out.height=(int)cinfo.output_height;
 out.pixels.resize((size_t)out.width*out.height);
 std::vector<unsigned char> row((size_t)out.width*3);
 while(cinfo.output_scanline<cinfo.output_height){
  JSAMPROW rows[1]={row.data()};int y=(int)cinfo.output_scanline;
  jpeg_read_scanlines(&cinfo,rows,1);
  for(int x=0;x<out.width;x++)out.pixels[(size_t)y*out.width+x]={row[(size_t)x*3]/255.f,row[(size_t)x*3+1]/255.f,row[(size_t)x*3+2]/255.f};
 }
 jpeg_finish_decompress(&cinfo);jpeg_destroy_decompress(&cinfo);std::fclose(file);return true;
}
#endif

static bool readRGBE(std::istream& in,unsigned char out[4]) { char b[4]; if(!in.read(b,4)) return false; for(int i=0;i<4;i++)out[i]=(unsigned char)b[i]; return true; }

static float interp1(const std::vector<float>& a,const std::vector<float>& vals,float x){
 if(a.empty()||vals.size()!=a.size())return 0.f;if(a.size()==1)return vals[0];
 if(x<=a.front())return vals.front();if(x>=a.back())return vals.back();
 auto it=std::upper_bound(a.begin(),a.end(),x);size_t hi=(size_t)(it-a.begin()),lo=hi-1;float t=(x-a[lo])/(a[hi]-a[lo]);return vals[lo]*(1.f-t)+vals[hi]*t;
}
float IESProfile::sample(float v,float h)const{
 if(!valid())return 0.f;v=std::clamp(v,verticalAngles.front(),verticalAngles.back());
 float maxH=horizontalAngles.back();h=std::abs(h);
 if(maxH<=90.f)h=std::min(h,90.f);else if(maxH<=180.f)h=std::min(h,180.f);else {h=std::fmod(h,360.f);if(h>180.f)h=360.f-h;}
 std::vector<float> column(verticalAngles.size());
 for(size_t j=0;j<verticalAngles.size();++j){std::vector<float> row(horizontalAngles.size());for(size_t i=0;i<horizontalAngles.size();++i)row[i]=candela[i*verticalAngles.size()+j];column[j]=interp1(horizontalAngles,row,h);}
 return std::max(0.f,interp1(verticalAngles,column,v));
}
bool loadIESProfile(const std::string& path,IESProfile& out,std::string& err){
 std::ifstream in(path);if(!in){err="Cannot open IES profile: "+path;return false;}
 std::string line;bool found=false;std::vector<double> n;
 while(std::getline(in,line)){if(line.find("TILT=")!=std::string::npos){if(line.find("TILT=NONE")==std::string::npos){err="IES currently supports TILT=NONE only";return false;}found=true;break;}}
 if(!found){err="Invalid IES: TILT=NONE section not found";return false;}
 double v;while(in>>v)n.push_back(v);
 if(n.size()<13){err="IES photometric header is incomplete";return false;}
 int nv=(int)n[3],nh=(int)n[4];if(nv<1||nh<1||nv>4096||nh>4096){err="IES angle table dimensions are invalid";return false;}
 size_t need=13u+(size_t)nv+(size_t)nh+(size_t)nv*(size_t)nh;if(n.size()<need){err="IES candela table is truncated";return false;}
 double multiplier=n[2];(void)multiplier;size_t k=13;out.verticalAngles.clear();out.horizontalAngles.clear();out.candela.clear();
 for(int i=0;i<nv;i++)out.verticalAngles.push_back((float)n[k++]);
 for(int i=0;i<nh;i++)out.horizontalAngles.push_back((float)n[k++]);
 for(int i=0;i<nh*nv;i++)out.candela.push_back((float)(n[k++]*n[2]));
 if(!std::is_sorted(out.verticalAngles.begin(),out.verticalAngles.end())||!std::is_sorted(out.horizontalAngles.begin(),out.horizontalAngles.end())){err="IES angle arrays must be ascending";return false;}
 if(!out.valid()){err="IES profile data is invalid";return false;}return true;
}

bool loadRadianceHdr(const std::string& path,EnvironmentMap& env,std::string& err){
 std::ifstream in(path,std::ios::binary); if(!in){err="Cannot open HDRI: "+path;return false;}
 std::string line; if(!std::getline(in,line) || (line.rfind("#?RADIANCE",0)!=0 && line.rfind("#?RGBE",0)!=0)){err="Unsupported HDRI: expected Radiance RGBE .hdr";return false;}
 bool format=false; while(std::getline(in,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty())break;if(line.find("FORMAT=32-bit_rle_rgbe")!=std::string::npos)format=true;}
 if(!format){err="HDRI format must be FORMAT=32-bit_rle_rgbe";return false;}
 if(!std::getline(in,line)){err="HDRI resolution line missing";return false;}
 int h=0,w=0; char sy=0,sx=0; std::istringstream rs(line); std::string ytok,xtok; rs>>ytok>>h>>xtok>>w;
 if(!rs||ytok.size()!=2||xtok.size()!=2||ytok[1]!='Y'||xtok[1]!='X'||h<1||w<1||w>32768||h>32768){err="Unsupported HDRI orientation/resolution; expected -Y height +X width";return false;}
 env.width=w;env.height=h;env.pixels.assign((size_t)w*h,Vec3{});
 for(int y=0;y<h;y++){
   unsigned char head[4]; if(!readRGBE(in,head)){err="Unexpected end of HDRI scanline";return false;}
   std::vector<unsigned char> scan((size_t)w*4);
   if(w>=8&&w<=32767&&head[0]==2&&head[1]==2&&!(head[2]&128)&&((int(head[2])<<8)|head[3])==w){
     for(int c=0;c<4;c++){int x=0;while(x<w){unsigned char code;if(!in.read((char*)&code,1)){err="Truncated HDRI RLE";return false;}if(code>128){int n=code-128;unsigned char v;if(n==0||x+n>w||!in.read((char*)&v,1)){err="Invalid HDRI RLE run";return false;}for(int k=0;k<n;k++)scan[(size_t)(x+k)*4+c]=v;x+=n;}else{int n=code;if(n==0||x+n>w||!in.read((char*)&scan[(size_t)x*4+c],1)){err="Invalid HDRI RLE literal";return false;}for(int k=1;k<n;k++)if(!in.read((char*)&scan[(size_t)(x+k)*4+c],1)){err="Truncated HDRI literal";return false;}x+=n;}}}
   }else{for(int x=0;x<w;x++){unsigned char rgbe[4];if(x==0)std::copy(head,head+4,rgbe);else if(!readRGBE(in,rgbe)){err="Truncated flat HDRI data";return false;}for(int c=0;c<4;c++)scan[(size_t)x*4+c]=rgbe[c];}}
   for(int x=0;x<w;x++){auto* q=&scan[(size_t)x*4];Vec3 v{};if(q[3]){float scale=std::ldexp(1.0f,(int)q[3]-(128+8));v={q[0]*scale,q[1]*scale,q[2]*scale};}env.pixels[(size_t)y*w+x]=v;}
 }
 return true;
}

bool loadRvScene(const std::string&path,Scene&s,std::string&err){
 std::ifstream in(path);if(!in){err="Cannot open scene: "+path;return false;}
 std::string line;int ln=0;
 while(std::getline(in,line)){++ln;if(line.empty()||line[0]=='#')continue;std::istringstream q(line);std::string tag;q>>tag;
 if(tag=="mat"){Material m;q>>std::quoted(m.name)>>m.baseColor.x>>m.baseColor.y>>m.baseColor.z>>m.roughness>>m.metallic>>m.transmission>>m.ior>>m.emission.x>>m.emission.y>>m.emission.z;if(!q){err="Bad mat line "+std::to_string(ln);return false;}s.materials.push_back(m);}
 else if(tag=="tri"||tag=="triuv"){Triangle t;q>>t.a.x>>t.a.y>>t.a.z>>t.b.x>>t.b.y>>t.b.z>>t.c.x>>t.c.y>>t.c.z;if(tag=="triuv"){q>>t.uvA.x>>t.uvA.y>>t.uvB.x>>t.uvB.y>>t.uvC.x>>t.uvC.y;t.hasUV=true;}q>>t.material;if(!q){err="Bad tri line "+std::to_string(ln);return false;}s.triangles.push_back(t);}
 else if(tag=="camera"){q>>s.camera.origin.x>>s.camera.origin.y>>s.camera.origin.z>>s.camera.target.x>>s.camera.target.y>>s.camera.target.z>>s.camera.up.x>>s.camera.up.y>>s.camera.up.z>>s.camera.fovDegrees;}
 else if(tag=="tex"){int mi;std::string path;q>>mi>>std::quoted(path);if(!q||mi<0||mi>=4096){err="Bad texture line "+std::to_string(ln);return false;}if((int)s.materials.size()<=mi)s.materials.resize(mi+1);s.materials[mi].texturePath=path;}
 else if(tag=="world"){q>>s.environment.x>>s.environment.y>>s.environment.z;}
 else if(tag=="sun"){int enabled=1;q>>s.sun.azimuthDegrees>>s.sun.elevationDegrees>>s.sun.intensity>>enabled;if(!q){err="Bad sun line "+std::to_string(ln);return false;}s.sun.enabled=(enabled!=0);s.sun.azimuthDegrees=std::fmod(s.sun.azimuthDegrees,360.f);s.sun.elevationDegrees=std::clamp(s.sun.elevationDegrees,-5.f,89.f);s.sun.intensity=std::max(0.f,s.sun.intensity);}
 else if(tag=="hdri"){q>>std::quoted(s.hdriPath);}
 else if(tag=="ies"){PhotometricLight l;std::string p;q>>std::quoted(p)>>l.position.x>>l.position.y>>l.position.z>>l.multiplier;if(!q){err="Bad ies line "+std::to_string(ln);return false;}std::string ie;if(!loadIESProfile(p,l.profile,ie)){err=ie+" (scene line "+std::to_string(ln)+")";return false;}s.iesLights.push_back(std::move(l));}
 else {err="Unknown token '"+tag+"' at line "+std::to_string(ln);return false;}
 }
 if(!s.hdriPath.empty()){std::string he;if(!loadRadianceHdr(s.hdriPath,s.hdri,he)){err=he;return false;}}
 if(s.materials.empty())s.materials.push_back(Material{});
 for(auto&t:s.triangles)if(t.material<0||t.material>=(int)s.materials.size())t.material=0;
 for(auto&m:s.materials)if(!m.texturePath.empty()){std::string te;auto dot=m.texturePath.find_last_of('.');std::string ext=dot==std::string::npos?"":m.texturePath.substr(dot);std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return (char)std::tolower(c);});bool ok=false;
  if(ext==".ppm")ok=loadPpmTexture(m.texturePath,m.texture,te);
  else if(ext==".bmp")ok=loadBmpTexture(m.texturePath,m.texture,te);
#ifdef RV_HAS_LIBPNG
  else if(ext==".png")ok=loadPngTexture(m.texturePath,m.texture,te);
#endif
#ifdef RV_HAS_LIBJPEG
  else if(ext==".jpg"||ext==".jpeg")ok=loadJpegTexture(m.texturePath,m.texture,te);
#endif
  else te="Unsupported texture extension: "+ext+" (supported formats depend on build)";
  if(!ok){err=te;return false;}}
 return true;
}}
