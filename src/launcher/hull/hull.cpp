#include "btBulletDynamicsCommon.h"
#include "BulletCollision/CollisionShapes/btShapeHull.h"

#include "hull.h"

#include <common/log.h>

using namespace engine;
using namespace media::geometry;

const unsigned short DEFAULT_SMOOTH_LEVEL = 1;
const float CONVEX_MARGIN = 0.1f;
const size_t RESERVE_MESH_VERTEX_COUNT = 1000;
const size_t RESERVE_MESH_INDEX_COUNT = RESERVE_MESH_VERTEX_COUNT * 3;

typedef std::vector<btScalar> ScalarArray;

/// Implementation internals of HullBuilder
struct HullBuilder::Impl
{
  ScalarArray                    input_vertices;
  std::vector<Vertex>            input_hull_vertices;
  std::vector<Mesh::index_type>  input_hull_indices;
  std::vector<Vertex>            result_hull_vertices;
  std::vector<Mesh::index_type>  result_hull_indices;  
  std::unique_ptr<IHullSmoother> smoother;
  unsigned short                 smooth_level;
  unsigned short                 refine_level;
  Mesh                           mesh;

  Impl()
    : smoother(create_loop_tesselation_smoother(DEFAULT_SMOOTH_LEVEL))
    , smooth_level(DEFAULT_SMOOTH_LEVEL)
    , refine_level(1)
  {
    mesh.vertices_resize(RESERVE_MESH_VERTEX_COUNT);
    mesh.indices_resize(RESERVE_MESH_INDEX_COUNT);
  }
};

HullBuilder::HullBuilder()
  : impl (new Impl)
{
}

HullBuilder::~HullBuilder()
{
}

void HullBuilder::set_smooth_level(unsigned short tesselation_level, unsigned short refine_level)
{
  impl->smooth_level = tesselation_level;
  impl->refine_level = refine_level;
}

unsigned short HullBuilder::smooth_level() const
{
  return impl->smooth_level;
}

unsigned short HullBuilder::refine_level() const
{
  return impl->refine_level;
}

void HullBuilder::reserve(size_t points_count)
{
  impl->input_vertices.reserve(points_count * 3);
}

void HullBuilder::reset ()
{
  impl->input_vertices.clear ();
}

void HullBuilder::add_point (const math::vec3f& position)
{
  impl->input_vertices.insert(impl->input_vertices.end (), &position [0], &position [0] + 3);
}

const engine::media::geometry::Mesh& HullBuilder::mesh() const
{
  return impl->mesh;
}

bool HullBuilder::build_hull(const char* material_name)
{
  engine_check_null(material_name);

    //remove far points

  

    //convex hull building

  btConvexHullShape source_shape (&impl->input_vertices[0], impl->input_vertices.size () / 3, 3 * sizeof (float));
  btShapeHull hull(&source_shape);

  if (!hull.buildHull(CONVEX_MARGIN))
    return false;

    //smoothing

  impl->input_hull_vertices.clear ();
  impl->input_hull_vertices.resize (hull.numVertices ());

  btVector3 bt_center;

  const btVector3* src_pos = hull.getVertexPointer ();

  for (size_t i=0, count=hull.numVertices (); i<count; i++, src_pos++)
    bt_center += *src_pos;

  bt_center /= hull.numVertices ();

  math::vec3f center(bt_center.x (), bt_center.y (), bt_center.z ());

  src_pos = hull.getVertexPointer ();

  media::geometry::Vertex* dst_pos = &impl->input_hull_vertices [0];

  for (size_t i=0, count=hull.numVertices (); i<count; i++, src_pos++, dst_pos++)
  {
    dst_pos->position = math::vec3f(src_pos->x(), src_pos->y(), src_pos->z());

    math::vec3f normal = normalize(dst_pos->position - center);

    dst_pos->normal = normal;
  }

  impl->input_hull_indices.assign(hull.getIndexPointer(), hull.getIndexPointer() + hull.numIndices());

    //smooth & refine

  impl->smoother->set_smooth_level(impl->smooth_level, impl->refine_level);
  impl->smoother->smooth(impl->input_hull_vertices, impl->input_hull_indices, impl->result_hull_vertices, impl->result_hull_indices);

    //generate texcoord & colors

  for (Vertex& vertex : impl->result_hull_vertices)
  {
    static const float PI = 3.1415926f;
    vertex.tex_coord = math::vec2f(asin(vertex.normal.x)/PI + 0.5f, asin(vertex.normal.y)/PI + 0.5f);
    vertex.color = math::vec4f(1.0f, 1.0f, 1.0f, 1.0f);
    vertex.normal = normalize(vertex.position - center); //???
  }

    //update mesh

  impl->mesh.vertices_resize(impl->result_hull_vertices.size());
  impl->mesh.indices_resize(impl->result_hull_indices.size());
  impl->mesh.remove_all_primitives();

  std::copy(impl->result_hull_vertices.begin(), impl->result_hull_vertices.end(), impl->mesh.vertices_data());
  std::copy(impl->result_hull_indices.begin(), impl->result_hull_indices.end(), impl->mesh.indices_data());
  
  impl->mesh.add_primitive(material_name, PrimitiveType_TriangleList, 0, impl->result_hull_indices.size() / 3, 0);

  impl->mesh.touch();

  return true;
}
