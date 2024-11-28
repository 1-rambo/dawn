#version 310 es
precision highp float;
precision highp int;


struct TintTextureUniformData {
  uint tint_builtin_value_0;
};

layout(binding = 0, std430)
buffer prevent_dce_block_1_ssbo {
  vec4 inner;
} v;
layout(binding = 0, std140)
uniform tint_symbol_1_1_ubo {
  TintTextureUniformData inner;
} v_1;
uniform highp sampler2DArray arg_0;
vec4 textureLoad_54a59b() {
  uvec2 arg_1 = uvec2(1u);
  int arg_2 = 1;
  int arg_3 = 1;
  uvec2 v_2 = arg_1;
  int v_3 = arg_2;
  int v_4 = arg_3;
  uint v_5 = (uint(textureSize(arg_0, 0).z) - 1u);
  uint v_6 = min(uint(v_3), v_5);
  uint v_7 = (v_1.inner.tint_builtin_value_0 - 1u);
  uint v_8 = min(uint(v_4), v_7);
  ivec2 v_9 = ivec2(min(v_2, (uvec2(textureSize(arg_0, int(v_8)).xy) - uvec2(1u))));
  ivec3 v_10 = ivec3(v_9, int(v_6));
  vec4 res = texelFetch(arg_0, v_10, int(v_8));
  return res;
}
void main() {
  v.inner = textureLoad_54a59b();
}
#version 310 es


struct TintTextureUniformData {
  uint tint_builtin_value_0;
};

layout(binding = 0, std430)
buffer prevent_dce_block_1_ssbo {
  vec4 inner;
} v;
layout(binding = 0, std140)
uniform tint_symbol_1_1_ubo {
  TintTextureUniformData inner;
} v_1;
uniform highp sampler2DArray arg_0;
vec4 textureLoad_54a59b() {
  uvec2 arg_1 = uvec2(1u);
  int arg_2 = 1;
  int arg_3 = 1;
  uvec2 v_2 = arg_1;
  int v_3 = arg_2;
  int v_4 = arg_3;
  uint v_5 = (uint(textureSize(arg_0, 0).z) - 1u);
  uint v_6 = min(uint(v_3), v_5);
  uint v_7 = (v_1.inner.tint_builtin_value_0 - 1u);
  uint v_8 = min(uint(v_4), v_7);
  ivec2 v_9 = ivec2(min(v_2, (uvec2(textureSize(arg_0, int(v_8)).xy) - uvec2(1u))));
  ivec3 v_10 = ivec3(v_9, int(v_6));
  vec4 res = texelFetch(arg_0, v_10, int(v_8));
  return res;
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  v.inner = textureLoad_54a59b();
}
#version 310 es


struct TintTextureUniformData {
  uint tint_builtin_value_0;
};

struct VertexOutput {
  vec4 pos;
  vec4 prevent_dce;
};

layout(binding = 0, std140)
uniform tint_symbol_1_1_ubo {
  TintTextureUniformData inner;
} v;
uniform highp sampler2DArray arg_0;
layout(location = 0) flat out vec4 vertex_main_loc0_Output;
vec4 textureLoad_54a59b() {
  uvec2 arg_1 = uvec2(1u);
  int arg_2 = 1;
  int arg_3 = 1;
  uvec2 v_1 = arg_1;
  int v_2 = arg_2;
  int v_3 = arg_3;
  uint v_4 = (uint(textureSize(arg_0, 0).z) - 1u);
  uint v_5 = min(uint(v_2), v_4);
  uint v_6 = (v.inner.tint_builtin_value_0 - 1u);
  uint v_7 = min(uint(v_3), v_6);
  ivec2 v_8 = ivec2(min(v_1, (uvec2(textureSize(arg_0, int(v_7)).xy) - uvec2(1u))));
  ivec3 v_9 = ivec3(v_8, int(v_5));
  vec4 res = texelFetch(arg_0, v_9, int(v_7));
  return res;
}
VertexOutput vertex_main_inner() {
  VertexOutput tint_symbol = VertexOutput(vec4(0.0f), vec4(0.0f));
  tint_symbol.pos = vec4(0.0f);
  tint_symbol.prevent_dce = textureLoad_54a59b();
  return tint_symbol;
}
void main() {
  VertexOutput v_10 = vertex_main_inner();
  gl_Position = v_10.pos;
  gl_Position.y = -(gl_Position.y);
  gl_Position.z = ((2.0f * gl_Position.z) - gl_Position.w);
  vertex_main_loc0_Output = v_10.prevent_dce;
  gl_PointSize = 1.0f;
}
