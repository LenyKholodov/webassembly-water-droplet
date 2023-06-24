#include <media/geometry.h>
#include <common/exception.h>
#include <common/log.h>

#include <vector>

#define FAST_OBJ_IMPLEMENTATION
#include <third-party/fast_obj/fast_obj.h>

using namespace engine::media::geometry;

Model MeshFactory::load_obj_model(const char* file_name)
{
  engine_check_null(file_name);

  fastObjMesh* mesh = fast_obj_read(file_name);

  engine_check_null(mesh);

  try
  {
    Model model;

      //load materials
    
    for (size_t i=0; i<mesh->material_count; i++)
    {
      fastObjMaterial& material = mesh->materials[i];
      media::geometry::Material result_mtl;
      common::PropertyMap& properties = result_mtl.properties();

      properties.set("diffuseColor", math::vec3f(material.Kd[0], material.Kd[1], material.Kd[2]));
      properties.set("ambientColor", math::vec3f(material.Ka[0], material.Ka[1], material.Ka[2]));
      properties.set("specularColor", math::vec3f(material.Ks[0], material.Ks[1], material.Ks[2]));
      properties.set("emissionColor", math::vec3f(material.Ke[0], material.Ke[1], material.Ke[2]));
      properties.set("shininess", material.Ns);

      if (material.map_Kd.path) result_mtl.add_texture("diffuseTexture", material.map_Kd.path);
      if (material.map_Ka.path) result_mtl.add_texture("ambientTexture", material.map_Ka.path);
      if (material.map_Ks.path) result_mtl.add_texture("specularTexture", material.map_Ks.path);
      if (material.map_Ke.path) result_mtl.add_texture("emissionTexture", material.map_Ke.path);
      if (material.map_bump.path) result_mtl.add_texture("normalTexture", material.map_bump.path);

      model.materials.insert(material.name, result_mtl);
    }

      //load vertices

    struct Hasher
    {
      size_t operator()(const std::tuple<unsigned int, unsigned int, unsigned int>& v) const { 
        return std::get<0>(v) ^ std::get<1>(v) ^ std::get<2>(v);
      }
    };

    std::unordered_map<std::tuple<unsigned int, unsigned int, unsigned int>, size_t, Hasher> vertex_map;
    std::vector<Vertex> vertices;
    std::vector<Mesh::index_type> indices;

    vertices.reserve(mesh->position_count);
    indices.reserve(mesh->index_count);

    const fastObjIndex* src_index = mesh->indices;

    for (size_t i=0; i<mesh->index_count; i++, src_index++)
    {
      std::tuple<unsigned int, unsigned int, unsigned int> key(src_index->p, src_index->t, src_index->n);
      auto it = vertex_map.find(key);

      if (it == vertex_map.end())
      {
          //add new vertex

        const float* p = mesh->positions + src_index->p * 3;
        const float* t = mesh->texcoords + src_index->t * 2;
        const float* n = mesh->normals + src_index->n * 3;
        Vertex vertex;

        vertex.position = math::vec3f(p[0], p[1], p[2]);
        vertex.tex_coord = math::vec2f(t[0], t[1]);
        vertex.normal = math::vec3f(n[0], n[1], n[2]);
        vertex.color = math::vec4f(1.0f, 0, 0, 1.0f);
    
        size_t index = vertices.size();
        vertices.push_back(vertex);
        vertex_map.insert(std::make_pair(key, index));

        indices.push_back(index);
      }
      else
      {
        //use existing vertex
    
        indices.push_back(it->second);
      }
    }

      //copy vertices and indices to model

    model.mesh.vertices_resize(vertices.size());
    model.mesh.indices_resize(indices.size());

    std::copy(vertices.begin(), vertices.end(), model.mesh.vertices_data());
    std::copy(indices.begin(), indices.end(), model.mesh.indices_data());

      //load geometry

    for (size_t i=0; i<mesh->group_count; i++)
    {
      fastObjGroup& group = mesh->groups[i];

      engine_log_debug("Parsing objgroup '%s' of model '%s'", group.name, file_name);

      unsigned int* face_material = mesh->face_materials + group.face_offset;
      unsigned int index_offset = group.index_offset;
      unsigned int current_material = (unsigned int)-1;
      unsigned int group_start_index = index_offset;

      engine_check(group.index_offset % 3 == 0);
      
      for (size_t j=0; j<group.face_count+1; j++, face_material++, index_offset += 3)
        if (current_material != *face_material || j == group.face_count)
        {
          if (group_start_index != index_offset)
          {
            auto primitive_index = model.mesh.add_primitive(mesh->materials[current_material].name, PrimitiveType_TriangleList, group_start_index / 3, (index_offset - group_start_index) / 3, 0);
            model.mesh.set_primitive_name(primitive_index, group.name);
          }
          
          current_material = *face_material;
          group_start_index = index_offset;
        }
    }

    fast_obj_destroy(mesh);

    return model;
  }
  catch(...)
  {
    fast_obj_destroy(mesh);
    throw;
  }
}