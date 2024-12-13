//
// fragment_main
//
#version 310 es
precision highp float;
precision highp int;

layout(binding = 0, std430)
buffer f_prevent_dce_block_ssbo {
  vec4 inner;
} v;
uniform highp sampler2DArray arg_0_arg_1;
vec4 textureSampleLevel_b7c55c() {
  vec3 v_1 = vec3(vec2(1.0f), float(1));
  vec4 res = textureLodOffset(arg_0_arg_1, v_1, float(1.0f), ivec2(1));
  return res;
}
void main() {
  v.inner = textureSampleLevel_b7c55c();
}
//
// compute_main
//
#version 310 es

layout(binding = 0, std430)
buffer prevent_dce_block_1_ssbo {
  vec4 inner;
} v;
uniform highp sampler2DArray arg_0_arg_1;
vec4 textureSampleLevel_b7c55c() {
  vec3 v_1 = vec3(vec2(1.0f), float(1));
  vec4 res = textureLodOffset(arg_0_arg_1, v_1, float(1.0f), ivec2(1));
  return res;
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  v.inner = textureSampleLevel_b7c55c();
}
//
// vertex_main
//
#version 310 es


struct VertexOutput {
  vec4 pos;
  vec4 prevent_dce;
};

uniform highp sampler2DArray arg_0_arg_1;
layout(location = 0) flat out vec4 tint_interstage_location0;
vec4 textureSampleLevel_b7c55c() {
  vec3 v = vec3(vec2(1.0f), float(1));
  vec4 res = textureLodOffset(arg_0_arg_1, v, float(1.0f), ivec2(1));
  return res;
}
VertexOutput vertex_main_inner() {
  VertexOutput v_1 = VertexOutput(vec4(0.0f), vec4(0.0f));
  v_1.pos = vec4(0.0f);
  v_1.prevent_dce = textureSampleLevel_b7c55c();
  return v_1;
}
void main() {
  VertexOutput v_2 = vertex_main_inner();
  gl_Position = v_2.pos;
  gl_Position.y = -(gl_Position.y);
  gl_Position.z = ((2.0f * gl_Position.z) - gl_Position.w);
  tint_interstage_location0 = v_2.prevent_dce;
  gl_PointSize = 1.0f;
}
