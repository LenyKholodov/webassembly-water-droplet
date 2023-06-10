#pragma once

#include <render/device.h>
#include <scene/camera.h>

#include <memory>

namespace engine {
namespace render {
namespace scene {

using ::engine::scene::Node;
using ::engine::scene::Camera;
using low_level::DeviceOptions;

//forward declarations
class SceneRenderer;
class ISceneRenderer;
class FrameNode;
class FrameNodeList;

/// Frame identifier
typedef size_t FrameId;

/// Rendering scene passes context
class ScenePassContext
{
  public:
    /// Index of the current frame
    FrameId current_frame_id() const;

    /// Set frame ID
    void set_current_frame_id(FrameId id);

    /// Frame node
    FrameNode& root_frame_node() const;

    /// Bindings
    low_level::BindingContext& bindings() const;

    /// Rendering device
    low_level::Device& device() const;

    /// Frame properties
    common::PropertyMap& properties() const;

    /// Frame textures
    low_level::TextureList& textures() const;

    /// Shared rendered materials
    low_level::MaterialList& materials() const;

    /// Shared frame nodes
    FrameNodeList& frame_nodes() const;

    /// Scene root node
    Node::Pointer root_node() const;

    /// Current view node (camera / light)
    Node::Pointer view_node() const;

    /// Set current view node
    void set_view_node(const Node::Pointer& view, const math::mat4f& projection_tm);

    /// Set camera
    void set_view_node(const Camera::Pointer& view);

    /// Current view TM (=inverse(view_node().world_tm()))
    const math::mat4f& view_tm() const;

    /// Inverse projection TM
    const math::mat4f& projection_tm() const;

    /// View & projection TM
    const math::mat4f& view_projection_tm() const;

  protected:
    /// Constructor
    ScenePassContext(ISceneRenderer&);

    /// Update bindings
    void bind(const low_level::BindingContext*);
    void unbind(const low_level::BindingContext*);

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

/// Frame DAG node for frame rendering with dependencies
class FrameNode
{
  public:
    /// Constructor
    FrameNode();

    /// Number of passes
    size_t passes_count() const;

    /// Add passes
    /// lower priorities render earlier
    void add_pass(const low_level::Pass&, int priority = 0);

    /// Add dependent frames
    /// this frame will be rendered after all dependent frames
    void add_dependency(const FrameNode& dependent_frame);

    /// Frame properties
    common::PropertyMap& properties() const;

    /// Frame textures
    low_level::TextureList& textures() const;

    /// ID of frame when this node has been rendered
    FrameId rendered_frame_id() const;

    /// Render frame
    void render(ScenePassContext& context);

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

/// Frame node list
class FrameNodeList
{
  public:
    /// List of nodes
    FrameNodeList();

    /// Nodes count
    size_t count() const;

    /// Add node
    void insert(const char* name, const FrameNode& node);

    /// Remove node
    void remove(const char* name);

    /// Find node by name
    FrameNode* find(const char* name) const;

    /// Get node by name or throw exception
    FrameNode& get(const char* name) const;

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

/// Scene render pass interface
class IScenePass
{
  public:
    /// Destructor
    virtual ~IScenePass() = default;

    /// Get dependencies (will be called ony once after the creation)
    virtual void get_dependencies(std::vector<std::string>& deps) = 0;

    /// Scene rendering
    virtual void render(ScenePassContext& context) = 0;
};

typedef std::shared_ptr<IScenePass> ScenePassPtr;

/// Scene pass factory
class ScenePassFactory
{
  public:
    typedef std::function<IScenePass* (SceneRenderer&, low_level::Device&)> ScenePassCreator;

    /// Register scene pass
    static void register_scene_pass(const char* pass, const ScenePassCreator& create_fn);

    /// Unregister scene pass
    static void unregister_scene_pass(const char* pass);

    /// Create scene pass
    static ScenePassPtr create_pass(const char* pass, SceneRenderer& renderer, low_level::Device& device);
};

/// Scene viewport
class SceneViewport
{
  public:
    /// Constructor
    SceneViewport();

    /// Viewport
    const low_level::Viewport& viewport() const;
    low_level::Viewport& viewport();

    /// Set viewport
    void set_viewport(const low_level::Viewport&);

    /// Camera
    scene::Camera::Pointer& camera() const;

    /// Set camera
    void set_camera(const scene::Camera::Pointer& camera);

    /// Scene viewport properties
    common::PropertyMap& properties() const;

    /// Set scene viewport properties
    void set_properties(const common::PropertyMap& properties);

    /// Scene viewport textures
    low_level::TextureList& textures() const;

    /// Set scene viewport textures
    void set_textures(const low_level::TextureList& textures);

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

/// Renderer
class SceneRenderer
{
  public:
    /// Constructor
    SceneRenderer(const application::Window& window, const low_level::DeviceOptions& options);

    /// Rendering device
    low_level::Device& device() const;

    /// Passes count
    size_t passes_count() const;

    /// Add scene pass
    void add_pass(const char* name, int priority = 0);

    /// Remove pass
    void remove_pass(const char* name);

    /// Render scene
    void render(const SceneViewport& viewport);

    /// Render scene
    void render(size_t count, const SceneViewport* viewports);

    /// Shared rendered properties
    common::PropertyMap& properties() const;

    /// Shared rendered textures
    low_level::TextureList& textures() const;

    /// Shared rendered materials
    low_level::MaterialList& materials() const;

    /// Shared frame nodes
    FrameNodeList& frame_nodes() const;

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

}}}
