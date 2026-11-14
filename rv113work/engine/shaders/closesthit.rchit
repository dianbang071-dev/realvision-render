#version 460
#extension GL_EXT_ray_tracing : require
hitAttributeEXT vec2 barycentrics;
layout(location=0) rayPayloadInEXT vec3 payload;
void main() {
  vec3 n = normalize(gl_WorldRayDirectionEXT * -1.0);
  float diffuse = max(dot(n, normalize(vec3(0.4, 0.8, 0.3))), 0.0);
  payload = vec3(0.72, 0.76, 0.82) * (0.08 + 0.92 * diffuse);
}
