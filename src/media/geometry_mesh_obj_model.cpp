#include <media/geometry.h>
#include <common/exception.h>

#include <vector>

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

      //parse materials
    
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

      result_mtl.add_texture("diffuseTexture", material.map_Kd.path);
      result_mtl.add_texture("ambientTexture", material.map_Ka.path);
      result_mtl.add_texture("specularTexture", material.map_Ks.path);
      result_mtl.add_texture("emissionTexture", material.map_Ke.path);
    }

    return model;
  }
  catch(...)
  {
    fast_obj_destroy(mesh);
    throw;
  }
}