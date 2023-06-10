#include <media/geometry.h>

#include <math/constants.h>
#include <math/utility.h>

using namespace math;
using namespace engine::media::geometry;

namespace
{

/// Constants
const size_t SPHERE_PARALLELS_COUNT = 16;
const size_t SPHERE_MERIDIANS_COUNT = 32;

}

/// Mesh factory

/// Create simple geometry objects
Mesh MeshFactory::create_box(const char* material, float width, float height, float depth, const math::vec3f& offset)
{
  Mesh return_value;

  Vertex           vertices[24];  //6 faces each containing 4 vertices
  Mesh::index_type indices[36];   //6 faces each containing 2 triangles
  vec3f            normals[6] = { vec3f(1, 0, 0), vec3f(-1, 0, 0), vec3f(0, 1, 0), vec3f(0, -1, 0), vec3f(0, 0, 1), vec3f(0, 0, -1) };
  vec3f            size(width * 0.5f, height * 0.5f, depth * 0.5f);

  for (size_t i = 0, count = sizeof(normals) / sizeof(*normals); i < count; i++)
  {
    const vec3f&     normal            = normals[i];
    Mesh::index_type base_vertex_index = i * 4;
    Vertex*          base_vertex       = vertices + base_vertex_index;
    vec3f            corner1           = vec3f(1.0f) - abs(normal),
                     corner2           = rotate(radian(constf::pi / 2.0f), normal) * corner1;

    for (size_t j = 0; j < 4; j++)
    {
      Vertex& vertex = base_vertex[j];

      vertex.normal = normal;
      vertex.color  = 1.f;
    }

    base_vertex[0].position  = offset + (normal + corner1) * size;
    base_vertex[0].tex_coord = vec2f(0.f, 1.f);
    base_vertex[1].position  = offset + (normal + corner2) * size;
    base_vertex[1].tex_coord = vec2f(1.f, 1.f);
    base_vertex[2].position  = offset + (normal - corner1) * size;
    base_vertex[2].tex_coord = vec2f(1.f, 0.f);
    base_vertex[3].position  = offset + (normal - corner2) * size;
    base_vertex[3].tex_coord = vec2f(0.f, 0.f);

    Mesh::index_type* base_index = indices + i * 6;

    base_index[0] = base_vertex_index;
    base_index[1] = base_vertex_index + 1;
    base_index[2] = base_vertex_index + 2;
    base_index[3] = base_vertex_index;
    base_index[4] = base_vertex_index + 2;
    base_index[5] = base_vertex_index + 3;
  }

  return_value.add_primitive(material, PrimitiveType_TriangleList, vertices, sizeof(vertices) / sizeof(*vertices), indices, sizeof(indices) / sizeof(*indices));

  return return_value;
}

Mesh MeshFactory::create_sphere(const char* material, float radius, const math::vec3f& offset)
{
  Mesh return_value;
    
  constexpr size_t TRIANGLES_COUNT = 2 * (SPHERE_PARALLELS_COUNT + 1) * SPHERE_MERIDIANS_COUNT;
  constexpr size_t VERTICES_COUNT = 2 + SPHERE_PARALLELS_COUNT * SPHERE_MERIDIANS_COUNT;

  Vertex          vertices[VERTICES_COUNT];
  Mesh::index_type indices[TRIANGLES_COUNT * 3];

  float horizontal_angle_step = 2.0f * constf::pi / float(SPHERE_MERIDIANS_COUNT - 1),
        vertical_angle_step   = constf::pi / float(SPHERE_PARALLELS_COUNT + 2);

    //north and south pole vertices

  vertices[0].normal = vec3f(0.f, 1.f, 0.f);
  vertices[1].normal = vec3f(0.f, -1.f, 0.f);
  vertices[0].tex_coord = vec2f(0.f, 0.f);
  vertices[1].tex_coord = vec2f(0.f, 1.f);

  float current_horizontal_angle = 0.f;

  size_t base_vertex = 2;

  Vertex* current_vertex = vertices + base_vertex;

    //generate vertices for each meridian

  for (size_t i = 0; i < SPHERE_MERIDIANS_COUNT; i++, current_horizontal_angle += horizontal_angle_step)
  {
    float current_vertical_angle = vertical_angle_step;

    float x = cos(current_horizontal_angle),
          z = sin(current_horizontal_angle);

    for (size_t j = 0; j < SPHERE_PARALLELS_COUNT; j++, current_vertical_angle += vertical_angle_step, current_vertex++)
    {
      float r = sin(current_vertical_angle);

      current_vertex->normal = vec3f(r * x, cos(current_vertical_angle), r * z);
      current_vertex->tex_coord = vec2f(current_horizontal_angle / constf::pi / 2.f, (current_vertical_angle + constf::pi / 2.f) / constf::pi);
    }
  }

  Mesh::index_type* current_index = indices;

    //fill indices

  for (size_t i = 0; i < SPHERE_MERIDIANS_COUNT; i++)
  {
    size_t vertex1 = base_vertex + i * SPHERE_PARALLELS_COUNT,                                  //first vertex for current meridian
           vertex2 = base_vertex + (i + 1) % SPHERE_MERIDIANS_COUNT * SPHERE_PARALLELS_COUNT;   //first vertex for next meridian

      //north pole triangle

    *current_index++ = vertex2;
    *current_index++ = vertex1;
    *current_index++ = 0;    

      //south pole triangle

    *current_index++ = vertex1 + SPHERE_PARALLELS_COUNT - 1;
    *current_index++ = vertex2 + SPHERE_PARALLELS_COUNT - 1;
    *current_index++ = 1;    

      //north to south meridian triangles

    for (size_t j = 0; j < SPHERE_PARALLELS_COUNT - 1; j++)
    {
      *current_index++ = vertex2 + j + 1;
      *current_index++ = vertex1 + j + 1;
      *current_index++ = vertex1 + j;      

      *current_index++ = vertex2 + j;
      *current_index++ = vertex2 + j + 1;
      *current_index++ = vertex1 + j;      
    }
  }

  for (size_t i = 0; i < VERTICES_COUNT; i++)
  {
    Vertex& vertex = vertices[i];

    vertex.color     = 1.f;
    vertex.position  = offset + vertex.normal * radius;
  }

  return_value.add_primitive(material, PrimitiveType_TriangleList, vertices, sizeof(vertices) / sizeof(*vertices), indices, sizeof(indices) / sizeof(*indices));

  return return_value;
}
