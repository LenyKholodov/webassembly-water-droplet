#include "shared.h"

using namespace engine::render::scene;
using namespace engine::render::low_level;
using namespace engine::common;

///
/// Constants
///

static constexpr size_t RESERVED_PASSES_COUNT = 8;
static constexpr size_t RESERVED_DEPENDENCIES_COUNT = 8;

///
/// FrameNode
///

namespace
{

struct PassEntry
{
  Pass pass; //low level rendering pass
  int priority; //rendering priority

  PassEntry(const Pass& pass, int priority)
    : pass(pass)
    , priority(priority)
  {
  }

  bool operator < (const PassEntry& other) const { return priority < other.priority; }
};

typedef std::vector<PassEntry> PassArray;
typedef std::vector<FrameNode> FrameArray;

}

/// Implementation details of frame node
struct FrameNode::Impl
{
  FrameId rendered_frame_id; //frame ID when this frame was rendered
  PassArray passes; //list of frame passes
  bool need_sort_passes; //passes should be sorted
  PropertyMap properties; //frame properties
  TextureList textures; //frame textures
  FrameArray deps; //frames which this frames depends on

  Impl()
    : rendered_frame_id()
    , need_sort_passes()
  {
    passes.reserve(RESERVED_PASSES_COUNT);
    deps.reserve(RESERVED_DEPENDENCIES_COUNT);
  }
};

FrameNode::FrameNode()
  : impl(std::make_shared<Impl>())
{
}

size_t FrameNode::passes_count() const
{
  return impl->passes.size();
}

void FrameNode::add_pass(const Pass& pass, int priority)
{
  impl->passes.push_back(PassEntry(pass, priority));

  impl->need_sort_passes = true;
}

void FrameNode::add_dependency(const FrameNode& frame)
{
  impl->deps.push_back(frame);
}

PropertyMap& FrameNode::properties() const
{
  return impl->properties;
}

TextureList& FrameNode::textures() const
{
  return impl->textures;
}

FrameId FrameNode::rendered_frame_id() const
{
  return impl->rendered_frame_id;
}

void FrameNode::render(ScenePassContext& context)
{
    //render dependencies

  FrameId current_frame_id = context.current_frame_id();

  for (auto& frame : impl->deps)
  {
    if (frame.rendered_frame_id() >= current_frame_id)
      continue;

    frame.render(context);
  }

    //sort passes

  if (impl->need_sort_passes)
  {    
    std::stable_sort(impl->passes.begin(), impl->passes.end());

    impl->need_sort_passes = false;
  }

    //render this frame

  BindingContext bindings(&context.bindings(), impl->properties, impl->textures);

  for (auto& pass : impl->passes)
  {
    pass.pass.render(&bindings);
  }

    //update frame info

  impl->rendered_frame_id = current_frame_id;

    //clear frame data

  impl->deps.clear();
  impl->passes.clear();
}

///
/// FrameNodeList
///

typedef NamedDictionary<FrameNode> FrameNodeDict;

/// Implementation details of frame node list
struct FrameNodeList::Impl
{
  FrameNodeDict nodes; //dictionary of nodes
};

FrameNodeList::FrameNodeList()
  : impl(std::make_shared<Impl>())
{
}

size_t FrameNodeList::count() const
{
  return impl->nodes.size();
}

void FrameNodeList::insert(const char* name, const FrameNode& node)
{
  engine_check_null(name);

  impl->nodes.insert(name, node);
}

void FrameNodeList::remove(const char* name)
{
  impl->nodes.erase(name);
}

FrameNode* FrameNodeList::find(const char* name) const
{
  return impl->nodes.find(name);
}

FrameNode& FrameNodeList::get(const char* name) const
{
  if (FrameNode* node = find(name))
    return *node;

  throw Exception::format("Frame node '%s' has not been found", name);
}
