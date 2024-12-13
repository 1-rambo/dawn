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
uniform highp sampler2D arg_1_arg_2;
vec4 textureGather_d6507c() {
  vec4 res = textureGatherOffset(arg_1_arg_2, vec2(1.0f), ivec2(1), int(1u));
  return res;
}
void main() {
  v.inner = textureGather_d6507c();
}
//
// compute_main
//
#version 310 es

layout(binding = 0, std430)
buffer prevent_dce_block_1_ssbo {
  vec4 inner;
} v;
uniform highp sampler2D arg_1_arg_2;
vec4 textureGather_d6507c() {
  vec4 res = textureGatherOffset(arg_1_arg_2, vec2(1.0f), ivec2(1), int(1u));
  return res;
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  v.inner = textureGather_d6507c();
}
//
// vertex_main
//
#version 310 es


struct VertexOutput {
  vec4 pos;
  vec4 prevent_dce;
};

uniform highp sampler2D arg_1_arg_2;
layout(location = 0) flat out vec4 tint_interstage_location0;
vec4 textureGather_d6507c() {
  vec4 res = textureGatherOffset(arg_1_arg_2, vec2(1.0f), ivec2(1), int(1u));
  return res;
}
VertexOutput vertex_main_inner() {
  VertexOutput v = VertexOutput(vec4(0.0f), vec4(0.0f));
  v.pos = vec4(0.0f);
  v.prevent_dce = textureGather_d6507c();
  return v;
}
void main() {
  VertexOutput v_1 = vertex_main_inner();
  gl_Position = v_1.pos;
  gl_Position.y = -(gl_Position.y);
  gl_Position.z = ((2.0f * gl_Position.z) - gl_Position.w);
  tint_interstage_location0 = v_1.prevent_dce;
  gl_PointSize = 1.0f;
}
