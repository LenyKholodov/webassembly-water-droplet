#include "hull.h"
#include "vertex_hash_map.h"
#include "pool.h"

#include <common/log.h>

//TODO: vertex refining in two passes(save old position)

//#define VERIFICATION_ENABLED
//#define REPORT_MEMORY_USAGE

namespace
{

const size_t MAX_VERTICES_PER_VERTEX    = 20;
const size_t MIN_RESERVED_VERTICES_SIZE = 500;
const size_t MIN_RESERVED_INDICES_SIZE  = 1500;

/// Additional table based functions for Loop subdivision
class LoopSubdivisionHelpers
{
  public:
/// Constructor
    LoopSubdivisionHelpers(size_t max_vertices_per_vertex)
    {
      build_beta_table(max_vertices_per_vertex);
    }

/// Get value for beta function
    float get_beta(size_t n) const
    {
      if (n < beta.size())
        return beta[n];

      return compute_beta(n);
    }

  private:
/// Build table for beta function
    void build_beta_table(size_t max_vertices_per_vertex)
    {
      beta.resize(max_vertices_per_vertex);

      for (size_t n=0; n<max_vertices_per_vertex; n++)
      {
        beta[n] = compute_beta(n);
      }
    }

/// Compute beta
    static float compute_beta(size_t intN)
    {
      //see http://www.cs.virginia.edu/~gfx/Courses/2010/IntroGraphics/Lectures/20-SubdivisionSurfaces.pdf

      float n =(float)intN;

      static const float PI = 3.1415926f;
 
      float x = 3.0f / 8.0f + cos(2.0f * PI / n) / 4.0f;

      return(5.0f / 8.0f - x * x) / n;
    }

  private:
    typedef std::vector<float> FloatArray;

  private:
    FloatArray beta;
};

/// Smoother based on Loop algorithm
class LoopTesselationSmoother: public IHullSmoother
{
  public:
    /// Constructor
    LoopTesselationSmoother(unsigned short in_tesselation_level)
      : tesselation_level(in_tesselation_level)
      , refine_level(1)
      , helpers(MAX_VERTICES_PER_VERTEX)
      , first_triangle()
      , total_vertices_count()
      , total_triangles_count()
      , tesselation_iteration()
      , reported_mem_used()
    {
    }

    /// Configure smooth level(LOD)
    void set_smooth_level(unsigned short in_tesselation_level, unsigned short in_refine_level)
    {
      tesselation_level = in_tesselation_level;
      refine_level      = in_refine_level;
    }

    /// Smoothing
    void smooth(const HullVertexArray& in_vertices, const HullIndexArray& in_indices, HullVertexArray& out_vertices, HullIndexArray& out_indices)
    {
      if (tesselation_level == 0 )
      {
      	out_vertices = in_vertices;
      	out_indices  = in_indices;

      	return;
      }

        //prepare data structures for smoothing, memory reservation

      vertex_pool.reset();
      edge_pool.reset();
      triangle_pool.reset();

      size_t reserved_vertices_size = in_vertices.size(), reserved_indices_size = in_indices.size();

      if (reserved_vertices_size < MIN_RESERVED_VERTICES_SIZE) reserved_vertices_size = MIN_RESERVED_VERTICES_SIZE;
      if (reserved_indices_size  < MIN_RESERVED_INDICES_SIZE)  reserved_indices_size  = MIN_RESERVED_INDICES_SIZE;

      vertex_pool.reserve(reserved_vertices_size * size_t(pow(2.f, tesselation_level)));
      edge_pool.reserve(2 * reserved_indices_size * size_t(pow(3.f, tesselation_level)));
      triangle_pool.reserve(reserved_indices_size / 3 * size_t(pow(4.f, tesselation_level)));

      first_triangle        = 0;
      total_triangles_count = 0;
      tesselation_iteration = 0;

      reset_edge_hash_map(reserved_indices_size * 2);

#ifdef REPORT_MEMORY_USAGE
      report_memory_usage(vertex_pool.capacity(), triangle_pool.capacity() * 3);
#endif      

        //build adjacency half-edge graph for input data

      build_adjacency(in_vertices, in_indices);

#ifdef VERIFICATION_ENABLED
      verify("Adjacency building");
#endif

        //subdivide 

      for (int tesselation_step = 0; tesselation_step < tesselation_level; tesselation_step++)
      {
#ifdef VERIFICATION_ENABLED
        engine_log_debug("Tesselation iteration %d...", tesselation_step);
#endif

        tesselation_iteration++;

        subdivide();

        for (size_t i=0; i<refine_level; i++)
        {
          tesselation_iteration++;

          refine();
        }
      }

        //prepare outgoing buffers

      build_buffers(out_vertices, out_indices);

#ifdef REPORT_MEMORY_USAGE
      report_memory_usage(out_vertices.size(), out_indices.size());
#endif

#ifdef VERIFICATION_ENABLED
      engine_log_debug("Tesselation results: before - %u vertices %u indices, after - %u vertices %u indices", in_vertices.size(), in_indices.size(), out_vertices.size(), out_indices.size());
#endif
    }

  private:
    struct Edge;
    struct Triangle;

    /// Vertex state during smoothing
    enum VertexState
    {
      VertexState_Initial, //intial vertex doesn' have to be refined
      VertexState_New,     //new vertex after edge splitting have to be refined
      VertexState_Refined, //refined
      VertexState_Copied   //copied to VB buffer
    };
 
    /// Vertex in half-edge structure
    struct Vertex
    {
      math::vec3f pos;              //vertex position
      math::vec3f new_pos;           //new vertex position is used during refining
      Edge*       first_edge;        //first edge which connects with this vertex
      VertexState state;            //vertex state
      size_t      tesselation_level; //tesselation level is used as flag for avoid duplicate refining
      size_t      index;            //for back packing to VB
    };

    /// Edge in half-edge structure
    struct Edge
    {
      Vertex*   vertex;           //start vertex for edge
      Triangle* triangle;         //left hand triangle
      Edge*     pair;             //edge pair(same vertices with reversed direction)
      Edge*     next;             //next edge for owner triangle
      Edge*     prev;             //previous edge for owner triangle
      size_t    tesselation_level; //tesselation level is used as flag for avoid duplicate splitting
    };

    /// Triangle in half-edge structure
    struct Triangle
    {
      Edge*       first_edge;        //first edge for triangle
      math::vec3f normal;           //normal for triangle(may be not ready)
      bool        is_normal_computed; //flag which signals that normal has been already computed
      Triangle*   next;             //next triangle in list
      Triangle*   prev;             //previous triangle in list
    };

    typedef std::vector<Vertex*> VertexList;

    /// Add triangle to list
    void add_triangle(Triangle& triangle)
    {
      if (first_triangle)
      {
        triangle.next = first_triangle;
        triangle.prev = first_triangle->prev;
    
        triangle.next->prev = triangle.prev->next = &triangle;
      }
      else first_triangle = triangle.prev = triangle.next = &triangle;

      total_triangles_count++;
    }

    /// Remove triangle from list(don't update edge structure!)
    void remove_triangle(Triangle& triangle)
    {
      triangle.prev->next = triangle.next;
      triangle.next->prev = triangle.prev;

      if (&triangle == first_triangle)
      {
        if (triangle.next == first_triangle) first_triangle = 0;
        else                                first_triangle = triangle.next;
      }

      total_triangles_count--;
    }

    /// Edge cross linking
    void cross_link(Edge* edge)
    {
      edge->next->prev = edge->prev->next = edge;
    }

    /// Get hash for vertices
    static size_t get_hash(Vertex* v1, Vertex* v2)
    {
      return size_t(v1) ^ size_t(v2);
    }

    /// reset edge hash map
    void reset_edge_hash_map(size_t size)
    {
      edge_hash_map.clear();
      edge_hash_map.resize(size);

      edge_desc_pool.reset();
      edge_desc_pool.reserve(size);
    }

    /// Find edge in hash map
    Edge* find_edge(Vertex* v1, Vertex* v2)
    {
      size_t start_index = get_hash(v1, v2) % edge_hash_map.size();

      EdgeDesc* desc = edge_hash_map[start_index];

      for (;desc; desc=desc->next)
      {
        if (desc->vertex[0] == v1 && desc->vertex[1] == v2)
          return desc->edge;
      }

      return 0;
    }

    /// Add edge to hash map
    void add_edge_to_hash_map(Vertex* v1, Vertex* v2, Edge* edge)
    {
      size_t start_index = get_hash(v1, v2) % edge_hash_map.size();

      EdgeDesc* desc = edge_hash_map[start_index], *prev = 0;

      for (;desc; prev=desc, desc=desc->next)
      {
        if (desc->vertex[0] == v1 && desc->vertex[1] == v2)
        {
          engine_log_debug("Edge(%d %d) has been already added to hash map! %s", v1->index, v2->index, find_edge(v1, v2) ? "Find successfull" : "Find failed");
          return;
        }
      }

      desc = new(edge_desc_pool.allocate()) EdgeDesc;

      desc->vertex[0] = v1;
      desc->vertex[1] = v2;
      desc->edge       = edge;
     
      if (prev) prev->next               = desc;
      else      edge_hash_map[start_index] = desc;
    }

    /// Update links between edge and corresponding vertex
    void update_edge_vertex_links(Edge* edge)
    {
      Vertex* vertex = edge->vertex;

      if (vertex->first_edge)
      {     
        if (edge->next != edge)
        {
          if (edge->pair->next->vertex == vertex && edge->prev->pair->vertex != vertex)
            vertex->first_edge = edge;
        }
      }
      else
      {
        vertex->first_edge = edge;
      }
    }

    /// Dump edges for vertex
    void dump_edges(Vertex* v)
    {
      engine_log_debug("Edges for vertex %d", v->index);

      Edge* edge = v->first_edge;

      do
      {
        engine_log_debug(" (%d %d)", edge->vertex->index, edge->pair->vertex->index);

        edge = edge->pair->next;
      } while (edge != v->first_edge);
    }
   
    /// Add new edge
    Edge* add_edge(Triangle& triangle, Vertex* v1, Vertex* v2)
    {
#ifdef VERIFICATION_ENABLED
      engine_log_debug("adding edge: %d %d, ", v1->index, v2->index);
#endif

      Edge* edge_pair = find_edge(v2, v1), *edge = edge_pair ? edge_pair->pair : 0;

#ifdef VERIFICATION_ENABLED
      if (!edge && find_edge(v1, v2))
      {
        engine_log_debug("Reverse order has been detected for edge(%d %d)", v1->index, v2->index);

        dump_edges(v1);
        dump_edges(v2);
      }
#endif

      if (!edge)
      {
        edge     = edge_pool.allocate();
        edge_pair = edge_pool.allocate();

        edge->vertex           = v1;
        edge->pair             = edge_pair;
        edge->tesselation_level = tesselation_iteration;

        edge_pair->vertex           = v2;
        edge_pair->triangle         = 0; //right hand triangle is unknown at this moment
        edge_pair->pair             = edge;
        edge_pair->next             = edge_pair;
        edge_pair->prev             = edge_pair;
        edge_pair->tesselation_level = tesselation_iteration;

        add_edge_to_hash_map(v1, v2, edge);
        add_edge_to_hash_map(v2, v1, edge_pair);
      }

      edge->triangle = &triangle;

      if (triangle.first_edge)
      {
        Edge* tri_edge = triangle.first_edge;

        do
        {
          tri_edge = tri_edge->next;
        } while (tri_edge->pair->vertex != edge->vertex);

        edge->prev = tri_edge;
        edge->next = tri_edge->next;

        cross_link(edge);
      }
      else
      {
        triangle.first_edge = edge->next = edge->prev = edge;
      }

      update_edge_vertex_links(edge);

      //do not update edge vertex links for edge_pair because it is not in valid edges ring

#ifdef VERIFICATION_ENABLED
      Edge* tri_edge = triangle.first_edge;

      printf("  edges: ");

      do
      {
        engine_log_debug("(%d %d%s) ", tri_edge->vertex->index, tri_edge->pair->vertex->index, tri_edge->triangle ? "" : " null");

        tri_edge = tri_edge->next;
      } while (tri_edge != triangle.first_edge);

      printf("\n");
#endif

      return edge;
    }

    /// Building adjacency graph
    void build_adjacency(const HullVertexArray& in_vertices, const HullIndexArray& in_indices)
    {
      tmp_vertices.clear();
      tmp_vertices.resize(in_vertices.size());

        //compute mesh center to determine reverse triangle order during adjacency building

      math::vec3f mesh_center(0, 0, 0);

      for (HullIndexArray::const_iterator iter=in_indices.begin(), end=in_indices.end(); iter!=end; iter++)
        mesh_center += in_vertices[*iter].position;

      mesh_center /= in_indices.size();

        //fill adjacency graph

      for (HullIndexArray::const_iterator iter=in_indices.begin(), end=in_indices.end(); iter < end && iter + 3 <= end; iter += 3) //TODO: remove debug check
      {
          //link vertices

        Vertex* vertices[3] = {0, 0, 0};

        for (size_t i=0; i<3; i++)
        {
          engine::media::geometry::Mesh::index_type   index  = iter[i];
          Vertex*& vertex = vertices[i];

          if (!tmp_vertices[index])
          {
            tmp_vertices[index] = vertex = vertex_pool.allocate();

            vertex->pos              = in_vertices[index].position;
            vertex->first_edge        = 0;
            vertex->state            = VertexState_Initial;
            vertex->index            = index;
            vertex->tesselation_level = tesselation_iteration;
          }
          else vertex = tmp_vertices[index];
        }

          //create triangle

        Triangle* triangle = triangle_pool.allocate();

        triangle->first_edge        = 0;
        triangle->is_normal_computed = false;

        add_triangle(*triangle);

          //ñreate edges

        math::vec3f normal      = cross(vertices[1]->pos - vertices[0]->pos, vertices[2]->pos - vertices[0]->pos),
                  out_direction = vertices[0]->pos - mesh_center;

        if (dot(normal, out_direction) > 0) //angle between out_direction and normal is LESS than 90 degrees - counterclockwise order
        {
          add_edge(*triangle, vertices[0], vertices[1]);
          add_edge(*triangle, vertices[1], vertices[2]);
          add_edge(*triangle, vertices[2], vertices[0]);
        }
        else //reverse(clockwise) order -> rebuild in counterclockwise order
        {
          add_edge(*triangle, vertices[0], vertices[2]);
          add_edge(*triangle, vertices[2], vertices[1]);
          add_edge(*triangle, vertices[1], vertices[0]);
        }         
      }

      total_vertices_count = in_vertices.size();
    }

    /// Get edges for triangle
    ///  returns - total number of edges in ring(may be larger than maxCount)
    static size_t get_edges(Triangle& triangle, Edge** edges, size_t maxCount)
    {
      Edge* edge = triangle.first_edge;

      Edge** outEdge = edges;

      size_t count = 0;

      do
      {
        if (count < maxCount)
          *outEdge++ = edge;

        edge = edge->next;

        count++;
      } while (edge != triangle.first_edge); 

      return count;
    }

    /// Split edge(owne triangle is on left)
    void split_edge(Edge* edge)
    {
      Vertex *v1          = edge->vertex,
             *v2          = edge->pair->vertex,
             *v3          = edge->next->vertex,
             *v4          = edge->pair->next->vertex,
             *new_vertex  = vertex_pool.allocate();
      Edge   *edge_pair   = edge->pair,
             *new_edge     = edge_pool.allocate(),
             *new_edge_pair = edge_pool.allocate();

      new_vertex->state             = VertexState_Refined;
//      new_vertex->state            = VertexState_New;
      new_vertex->index             = total_vertices_count++;
      new_vertex->tesselation_level = tesselation_iteration;

      new_edge->vertex            = new_vertex;
      new_edge->pair              = edge->pair;
      new_edge->next              = edge->next;
      new_edge->prev              = edge;
      new_edge->triangle          = edge->triangle;
      new_edge->tesselation_level = tesselation_iteration;

      cross_link(new_edge);

      new_edge_pair->vertex            = new_vertex;
      new_edge_pair->pair              = edge;
      new_edge_pair->next              = edge_pair->next;
      new_edge_pair->prev              = edge_pair;
      new_edge_pair->triangle          = edge_pair->triangle;
      new_edge_pair->tesselation_level = tesselation_iteration;

      cross_link(new_edge_pair);

      edge->pair              = new_edge_pair;
      edge->next              = new_edge;
      edge->tesselation_level = tesselation_iteration;

      edge_pair->pair              = new_edge;
      edge_pair->next              = new_edge_pair;
      edge_pair->tesselation_level = tesselation_iteration;

      new_vertex->pos       = refine_edge_point(v1, v2, v3, v4);
      new_vertex->first_edge = new_edge;
    }

    /// Split triangle edges
    void split_edges(Triangle& triangle)
    {
      Edge* edge = triangle.first_edge;

      do
      {
        Edge* next = edge->next;

        if (edge->tesselation_level < tesselation_iteration)
          split_edge(edge);

        edge = next;
      } while (edge != triangle.first_edge);
    }

    /// First subdivision phase - adding edge points
    void subdivide_first_pass()
    {
      Triangle* triangle = first_triangle;

      if (!triangle)
        return;

      do
      {
        split_edges(*triangle);

        triangle = triangle->next;
      } while (triangle != first_triangle);

#ifdef VERIFICATION_ENABLED
      verify("subdivide first phase(after)", 6);
#endif
    }

    /// Add new triangle edge
    Edge* add_edge(Triangle& left_triangle, Triangle& right_triangle, Vertex* v1, Vertex* v2, Edge* prev_edge, Edge* next_edge)
    {
      Edge *edge     = edge_pool.allocate(),
           *edge_pair = edge_pool.allocate();

      edge->vertex           = v1;
      edge->triangle         = &left_triangle;
      edge->pair             = edge_pair;
      edge->next             = next_edge;
      edge->prev             = prev_edge;
      edge->tesselation_level = tesselation_iteration;

      cross_link(edge);

      edge_pair->vertex           = v2;
      edge_pair->triangle         = &right_triangle;
      edge_pair->pair             = edge;
      edge_pair->next             = edge_pair;
      edge_pair->prev             = edge_pair;
      edge_pair->tesselation_level = tesselation_iteration;

      prev_edge->triangle = next_edge->triangle = &left_triangle;

      if (!left_triangle.first_edge)  left_triangle.first_edge  = edge;
      if (!right_triangle.first_edge) right_triangle.first_edge = edge_pair;

      update_edge_vertex_links(edge);
      update_edge_vertex_links(edge_pair);

      return edge_pair;
    }

    Edge* add_triangle(Triangle& middle_triangle, Edge** edges, size_t edge_index1, size_t edge_index2)
    {
      Edge *prev_edge = edges[edge_index1], 
           *next_edge = edges[edge_index2];

      Vertex* vertices[3] = {prev_edge->vertex, edges[edge_index1 + 1]->vertex, next_edge->vertex};

        //create new triangle

      Triangle* triangle = triangle_pool.allocate();

      triangle->first_edge        = 0;
      triangle->is_normal_computed = false;

        //update edges

      Edge* middle_edge_pair = add_edge(*triangle, middle_triangle, vertices[1], vertices[2], prev_edge, next_edge);

        //add triangle to list of triangles

      add_triangle(*triangle);

      return middle_edge_pair;
    }

    void set_edge_links(Edge* edge, Edge* prev_edge, Edge* next_edge, Triangle& triangle)
    {
      edge->prev     = prev_edge;
      edge->next     = next_edge;
      edge->triangle = &triangle;

      update_edge_vertex_links(edge);
    }

    /// subdivide triangle from 6 points
    void subdivide_triangle(Triangle& triangle)
    {
        //get edges

      Edge* edges[7];

      size_t edges_count = get_edges(triangle, edges, 6);

      if (edges_count != 6)
      {
        engine_log_debug("Internal error: bad triangle structure. %u edges found instead of 6 before subdivision", edges_count);
        return;
      }

      edges[6] = edges[0];

        //prepare middle triangle

      Triangle* middle_triangle = triangle_pool.allocate();

      middle_triangle->first_edge        = 0;
      middle_triangle->is_normal_computed = false;

        //add border triangles
     
        //    Vertices              
        //       0                    
        //    1     5             
        //  2    3    4
    
        //      Edges
        //          *
        //        0   5
        //      *   6   *
        //    1   8   7   4
        //  *   2   *  3   *   

      Edge* edge6 = add_triangle(*middle_triangle, edges, 0, 5);
      Edge* edge7 = add_triangle(*middle_triangle, edges, 4, 3);
      Edge* edge8 = add_triangle(*middle_triangle, edges, 2, 1);

        //add middle triangle and update its edges

      set_edge_links(edge6, edge7, edge8, *middle_triangle);
      set_edge_links(edge7, edge8, edge6, *middle_triangle);
      set_edge_links(edge8, edge6, edge7, *middle_triangle);

      add_triangle(*middle_triangle);

        //remove source triangle

      remove_triangle(triangle);

      //triangle_pool.deallocate(&triangle); //???ARM
    }

    /// Second subdivision phase - adding triangles
    void subdivide_second_pass()
    {
      Triangle* triangle = first_triangle;

      if (!triangle)
        return;

        //adding dummy triangle as border for second pass

      Triangle dummy;

      dummy.first_edge        = 0;
      dummy.is_normal_computed = false;
     
      add_triangle(dummy);

        //subdivision pass

      do
      {
        Triangle* next = triangle->next;

        subdivide_triangle(*triangle);

        triangle = next;
      } while (triangle != &dummy);

        //remove dummy triangle

      remove_triangle(dummy);

#ifdef VERIFICATION_ENABLED
      verify("subdivide second phase(after)");
#endif
    }

    /// Subdivision
    void subdivide()
    {
      subdivide_first_pass();
      subdivide_second_pass();
    }

    /// Get vertex neighbours count
    static size_t get_neighbours_count(Vertex* vertex)
    {
      size_t neighbours_count = 0;

      Edge* edge = vertex->first_edge;      

      do
      {
        edge = edge->pair->next;

        neighbours_count++;
      } while (edge != vertex->first_edge && edge->vertex == vertex);

      return neighbours_count;
    }

    /// refine edge point
    math::vec3f refine_edge_point(Vertex* v1, Vertex* v2, Vertex* v3, Vertex* v4)
    {
      //http://www.holmes3d.net/graphics/subdivision/

      return 3.0f / 8.0f *(v1->pos + v2->pos) + 1.0f / 8.0f *(v3->pos + v4->pos);
    }

    /// refine vertex position
    void refine_vertex_point(Vertex* vertex)
    {
      if (vertex->tesselation_level >= tesselation_iteration)
        return;

        //http://www.cs.virginia.edu/~gfx/Courses/2010/IntroGraphics/Lectures/20-SubdivisionSurfaces.pdf
        //new_position =(1 - k*beta)*original_position + sum(beta * each_original_vertex)

      size_t neighbours_count = get_neighbours_count(vertex);

      float beta = helpers.get_beta(neighbours_count);

      math::vec3f avg(0, 0, 0);

      Edge* edge = vertex->first_edge;      

      do
      {
        avg  += edge->pair->vertex->pos * beta;
        edge  = edge->pair->next;
      } while (edge != vertex->first_edge && edge->vertex == vertex);

      const math::vec3f& original_position = vertex->pos;

//  math::vec3f origin = vertex->pos;
    
      vertex->new_pos   =(1.0f - neighbours_count * beta) * original_position + avg;
//      vertex->state = VertexState_Refined;
      vertex->tesselation_level = tesselation_iteration;

//      printf("refining %d from[%.3f %.3f %.3f] to[%.3f %.3f %.3f] beta=%.4f neighbours=%d\n", vertex->index, origin[0], origin[1], origin[2],
 //       vertex->pos[0], vertex->pos[1], vertex->pos[2], beta, neighbours_count);
    }

    /// refine triangle
    void refine(Triangle& triangle)
    {
      Edge* edge = triangle.first_edge;

      do
      {
        refine_vertex_point(edge->vertex);

        edge = edge->next;
      } while (edge != triangle.first_edge);
    }

    /// refine
    void refine()
    {
      Triangle* triangle = first_triangle;

      if (!triangle)
        return;

      do
      {
        refine(*triangle);

        triangle = triangle->next;
      } while (triangle != first_triangle);

      do
      {
        Edge* edge = triangle->first_edge;

        do
        {
          Vertex* vertex = edge->vertex;

          vertex->pos = vertex->new_pos; //TODO: optimize(use flag)

          edge = edge->next;
        } while (edge != triangle->first_edge);        

        triangle = triangle->next;
      } while (triangle != first_triangle);
    }

    /// Get triangle normal
    static const math::vec3f& get_normal(Triangle& triangle) //TODO: reference, crashes on Anroid
    {
      if (triangle.is_normal_computed)
        return triangle.normal;

      Edge* edges[3];

      size_t edges_count = get_edges(triangle, &edges[0], 3);

      if (edges_count != 3)
      {
        engine_log_debug("Internal error: bad triangle structure. %u edges found instead of 3 during normal computing", edges_count);

        static math::vec3f bad_normal(0, 1, 0);

        return bad_normal;
      }
      
      const math::vec3f &v1 = edges[0]->vertex->pos,
                      &v2 = edges[1]->vertex->pos,
                      &v3 = edges[2]->vertex->pos;

      math::vec3f normal = normalize(cross(v2 - v1, v3 - v1));

      triangle.normal           = normal;
      triangle.is_normal_computed = true;

      return triangle.normal;
    }

    /// copy vertex data
    static void copy(Vertex* vertex, engine::media::geometry::Vertex& out_vertex)
    {
        //compute normal

      math::vec3f avg_normal(0, 0, 0);

      Edge* edge = vertex->first_edge;

      do
      {
        const math::vec3f& normal = get_normal(*edge->pair->triangle);

        avg_normal += normal;
        edge       = edge->pair->next;
      } while (edge != vertex->first_edge && edge->vertex == vertex);

      avg_normal = normalize(avg_normal);

        //copy data

      out_vertex.position = vertex->pos;
      out_vertex.normal   = avg_normal;
    }

    /// copy triangle data to VB / IB
    static void copy(Triangle& triangle, HullVertexArray& out_vertices, HullIndexArray& out_indices)
    {
      Edge* edge = triangle.first_edge;

      size_t edges_count = 0;
      
      do
      {
        if (edges_count >= 3)
        {
          engine_log_debug("Internal error: bad triangle structure. %u edges found instead of 3 during data copying", edges_count);        
          break;
        }

        Vertex* vertex = edge->vertex;

          //copy data to VB

        if (vertex->state != VertexState_Copied)
        {
          copy(vertex, out_vertices[vertex->index]);

          vertex->state = VertexState_Copied;
        }

          //copy data to IB

        if (vertex->index >= out_vertices.size())
          throw engine::common::Exception::format("Internal error: bad vertex index %u during data copying", vertex->index);

        out_indices.push_back(vertex->index);

        edges_count++;

        edge = edge->next;
      } while (edge != triangle.first_edge);
    }

    /// copy smoothed data to index / vertex arrays
    void build_buffers(HullVertexArray& out_vertices, HullIndexArray& out_indices)
    {
      out_vertices.clear();
      out_vertices.resize(total_vertices_count);

      out_indices.clear();
      out_indices.reserve(total_triangles_count * 3);

      Triangle* triangle = first_triangle;

      if (!triangle)
        return;

      do
      {
        copy(*triangle, out_vertices, out_indices);

        triangle = triangle->next;
      } while (triangle != first_triangle);
    }

    /// verify data structure
    void verify(const char* message = 0, size_t required_edges_count_per_triangle = 3)
    {
      if (message)
      {
        engine_log_debug("Verification '%s' has started", message);
      }

      Triangle* triangle = first_triangle;

      if (!triangle)
        return;

      do
      {
        if (!triangle)
          engine_log_debug("Null item in triangle list has been detected");

        Edge* edge = triangle->first_edge;

        if (!edge)
          engine_log_debug("Triangle without edges has been detected");

        do
        {
          if (!edge || !edge->next)
            engine_log_debug("Null item in edge list has been detected");

          if (!edge->pair)
            engine_log_debug("Edge without pair has been detected");

          if (edge != edge->pair->pair)
            engine_log_debug("Wrong pair edge back reference value has been detected");          

          if (!edge->vertex->first_edge)
            engine_log_debug("Null first edge for vertex has been detected");
         
          if (!edge->triangle)
            engine_log_debug("Null edge triangle has been detected");

          Vertex* vertex    = edge->vertex;
          Edge*   vedge     = vertex->first_edge;
          bool    edge_found = false;

          do
          {
            if (vedge == edge)
            {
              if (edge_found)
                engine_log_debug("Edge for vertex found twice, edge=(%d %d)", vedge->vertex->index, vedge->pair->vertex->index);

              edge_found = true;
            }

            if (!vedge->triangle)
              engine_log_debug("Null edge triangle has been detected during vertex adjacency traverse");

            vedge = vedge->pair->next;
          } while (vedge != vertex->first_edge && vedge->vertex == vertex);

          if (!edge_found)
          {
            engine_log_debug("Edge(%d %d) has not been detected during vertex adjacency traverse. Edges: ", edge->vertex->index, edge->pair->vertex->index);

            Edge* vedge = vertex->first_edge;

            do
            {
              engine_log_debug(" (%d %d)", vedge->vertex->index, vedge->pair->vertex->index);
              vedge = vedge->pair->next;
            } while (vedge != vertex->first_edge);// && vedge->vertex == vertex);
          }
          

//          if (edge->pair->next != edge->pair)
//            engine_log_debug("Wrong references has been detected");

          edge = edge->next;    
        } while (edge != triangle->first_edge);

        size_t edges_count = get_edges(*triangle, 0, 0);

        if (required_edges_count_per_triangle > 0 && edges_count != required_edges_count_per_triangle)
          engine_log_debug("Internal error: bad triangle structure. %u edges found instead of 3 during data verification", edges_count);

        triangle = triangle->next;
      } while (triangle != first_triangle);      

      if (message)
      {
        engine_log_debug("Verification '%s' has stopped", message);
      }
    }

    /// Report about memory usage
    void report_memory_usage(size_t verts_count = 0, size_t indices_count = 0)
    {
      size_t mem_used = 0;

      mem_used += vertex_pool.capacity_in_bytes();
      mem_used += edge_pool.capacity_in_bytes();
      mem_used += triangle_pool.capacity_in_bytes();
      mem_used += tmp_vertices.size() * sizeof(VertexList::value_type);
      mem_used += edge_desc_pool.capacity_in_bytes();
      mem_used += edge_hash_map.size() * sizeof(EdgeDescArray::value_type);
      mem_used += verts_count * sizeof(Vertex);
      mem_used += indices_count * sizeof(engine::media::geometry::Mesh::index_type);

      if (mem_used >= reported_mem_used)
      {
        reported_mem_used = mem_used;
        engine_log_debug("Loop tesselation memory usage: %.2fM", reported_mem_used / 1000000.0);
      }
    }
    
  private:
    typedef Pool<Vertex>   VertexPool;
    typedef Pool<Edge>     EdgePool;
    typedef Pool<Triangle> TrianglePool;
    
    struct EdgeDesc
    {
      Vertex*   vertex[2];
      Edge*     edge;
      EdgeDesc* next;

      EdgeDesc() : edge(), next()
      {
        vertex[0] = vertex[1] = 0;
      }
    };

    typedef Pool<EdgeDesc> EdgeDescPool;

    typedef std::vector<EdgeDesc*> EdgeDescArray;

  private:
    unsigned short         tesselation_level;
    unsigned short         refine_level;
    LoopSubdivisionHelpers helpers;
    EdgeDescPool           edge_desc_pool;
    EdgeDescArray          edge_hash_map;
    VertexPool             vertex_pool;
    EdgePool               edge_pool;
    TrianglePool           triangle_pool;
    VertexList             tmp_vertices;
    Triangle*              first_triangle;
    size_t                 total_vertices_count;
    size_t                 total_triangles_count;
    size_t                 tesselation_iteration;
    size_t                 reported_mem_used;
};

}

/// Tesselation smoother create function
IHullSmoother* create_loop_tesselation_smoother(unsigned short smooth_level)
{
  return new LoopTesselationSmoother(smooth_level);
}
