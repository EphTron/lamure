// Copyright (c) 2014-2018 Bauhaus-Universitaet Weimar
// Copyright (c) 2014-2018 Bauhaus-Universitaet Weimar
// This Software is distributed under the Modified BSD License, see license.txt.
//
// Virtual Reality and Visualization Research Group 
// Faculty of Media, Bauhaus-Universitaet Weimar
// http://www.uni-weimar.de/medien/vr

#version 420 core

layout (location = 0) out vec4 out_color;


uniform mat4 model_view_matrix;

layout(binding  = 10) uniform sampler2D in_mesh_color_texture;


/*
  color_rendering_mode == 0 -> color coded uvs
  color_rendering_mode == 1 -> texture color
*/

uniform int color_rendering_mode = 0;

in vertex_data {
    vec4 position;
    vec4 normal;
    vec2 coord;
} vertex_in;

OPTIONAL_BEGIN
  INCLUDE ../common/shading/blinn_phong.glsl
OPTIONAL_END

void main() {

  vec4 n = vertex_in.normal;
  vec3 nv = normalize((n*inverse(model_view_matrix)).xyz);


  vec3 color = vec3(0.0);
  if(0 == color_rendering_mode) {
    color = vec3(vertex_in.coord.x, vertex_in.coord.y, 0.5);
  } else {
    color = texture2D(in_mesh_color_texture, vertex_in.coord.xy).rgb;
  }

  //to show normals
  //color = vec3(n.x*0.5+0.5, n.y*0.5+0.5, n.z*0.5+0.5);
  

  OPTIONAL_BEGIN
    vec4 pos_es = model_view_matrix * vec4(vertex_in.position.xyz, 1.0f);
    color = shade_blinn_phong(pos_es.xyz, n.xyz, vec3(0.0, 0.0, 0.0), color);
  OPTIONAL_END

  out_color = vec4(color, 1.0);

}

