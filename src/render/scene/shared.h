#pragma once

#include <render/scene_render.h>

#include <common/named_dictionary.h>
#include <common/exception.h>
#include <common/string.h>
#include <common/log.h>

#include <unordered_set>

namespace engine {
namespace render {
namespace scene {

/// Scene renderer internal interchange API
class ISceneRenderer
{
  public:
    /// Shared rendered properties
    virtual common::PropertyMap& properties() = 0;

    /// Shared rendered textures
    virtual low_level::TextureList& textures() = 0;

    /// Shared rendered materials
    virtual low_level::MaterialList& materials() = 0;

    /// Shared frame nodes
    virtual FrameNodeList& frame_nodes() = 0;

    /// Rendering device
    virtual low_level::Device& device() = 0;

  protected:
    virtual ~ISceneRenderer() = default;
};


}}}