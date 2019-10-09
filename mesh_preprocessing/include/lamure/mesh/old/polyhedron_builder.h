#include "CGAL_typedefs.h"

#include <CGAL/Polyhedron_incremental_builder_3.h>

#include "kdtree.h"

template <class HDS>
class polyhedron_builder : public CGAL::Modifier_base<HDS>
{
  private:
    const std::vector<indexed_triangle_t>& indexed_triangles;

  public:
    polyhedron_builder(std::vector<indexed_triangle_t>& triangles) : indexed_triangles(triangles) {}

    // Returns false for error
    void operator()(HDS& hds)
    {
        // create a cgal incremental builder
        CGAL::Polyhedron_incremental_builder_3<HDS> B(hds, true);
        B.begin_surface(indexed_triangles.size() * 3, indexed_triangles.size(), 0, CGAL::Polyhedron_incremental_builder_3<HDS>::ABSOLUTE_INDEXING);

        std::cout << "Creating polyhedron indexed vertex list...\n";

        // create indexed vertex list
        std::vector<XtndPoint<Kernel>> vertices_indexed;
        std::vector<uint32_t> tris_indexed;
        create_indexed_triangle_list(tris_indexed, vertices_indexed);

        // add vertices of surface
        // std::cout << "Polyhedron builder: adding vertices\n";
        for(uint32_t i = 0; i < vertices_indexed.size(); ++i)
        {
            Vertex_handle vh = B.add_vertex(vertices_indexed[i]);
            vh->id = i;
        }

        std::vector<size_t> new_face(4);

        // create faces using vertex index references
        // std::cout << "Polyhedron builder: adding faces\n";
        uint32_t face_count = 0;
        for(uint32_t i = 0; i < tris_indexed.size(); i += 3)
        {
            // check for degenerate faces:
            bool degenerate = is_face_degenerate(Point(vertices_indexed[tris_indexed[i]]), Point(vertices_indexed[tris_indexed[i + 1]]), Point(vertices_indexed[tris_indexed[i + 2]]));

            new_face[0] = tris_indexed[i];
            new_face[1] = tris_indexed[i + 1];
            new_face[2] = tris_indexed[i + 2];
            new_face[3] = tris_indexed[i];

            bool valid = B.test_facet_indices(new_face);

            if(!valid)
            {
                std::cerr << "Triangle " << i / 3 << " violates non-manifoldness constraint, skipping" << std::endl;
                continue;
            }

            if(!degenerate && !B.error())
            {
                Facet_handle fh = B.begin_facet();
                fh->id() = face_count++;

                // add tex coords
                TexCoord tc1(vertices_indexed[tris_indexed[i]].get_u(), vertices_indexed[tris_indexed[i]].get_v());
                TexCoord tc2(vertices_indexed[tris_indexed[i + 1]].get_u(), vertices_indexed[tris_indexed[i + 1]].get_v());
                TexCoord tc3(vertices_indexed[tris_indexed[i + 2]].get_u(), vertices_indexed[tris_indexed[i + 2]].get_v());
                fh->add_tex_coords(tc1, tc2, tc3);

                fh->tex_id = indexed_triangles[i / 3].tex_idx_;
                fh->tri_id = indexed_triangles[i / 3].tri_id_;

                B.add_vertex_to_facet(tris_indexed[i]);
                B.add_vertex_to_facet(tris_indexed[i + 1]);
                B.add_vertex_to_facet(tris_indexed[i + 2]);
                B.end_facet();
            }

            if(B.error())
            {
                B.end_surface();
                std::cerr << "Error in incremental polyhedron building" << std::endl;
                return;
            }
        }

        // finish up the surface
        B.end_surface();
    }

    bool is_face_degenerate(Point v1, Point v2, Point v3)
    {
        if(v1 == v2 || v1 == v3 || v2 == v3)
        {
            return true;
        }
        return false;
    }

    // creates indexed triangles list (indexes and vertices) from triangle list
    void create_indexed_triangle_list(std::vector<uint32_t>& tris_indexed, std::vector<XtndPoint<Kernel>>& vertices_indexed)
    {
        // for each vertex in original tris list
        for(uint32_t v = 0; v < indexed_triangles.size() * 3; v++)
        {
            int triangle_pos = v / 3;
            int vertex_subpos = v % 3;

            // get Point value
            XtndPoint<Kernel> tri_pnt;

            switch(vertex_subpos)
            {
            case 0:
            {
                tri_pnt = XtndPoint<Kernel>(indexed_triangles[triangle_pos].v0_.pos_.x,
                                            indexed_triangles[triangle_pos].v0_.pos_.y,
                                            indexed_triangles[triangle_pos].v0_.pos_.z,
                                            indexed_triangles[triangle_pos].v0_.tex_.x,
                                            indexed_triangles[triangle_pos].v0_.tex_.y,
                                            indexed_triangles[triangle_pos].v0_.nml_.x,
                                            indexed_triangles[triangle_pos].v0_.nml_.y,
                                            indexed_triangles[triangle_pos].v0_.nml_.z);
            }
            break;
            case 1:
            {
                tri_pnt = XtndPoint<Kernel>(indexed_triangles[triangle_pos].v1_.pos_.x,
                                            indexed_triangles[triangle_pos].v1_.pos_.y,
                                            indexed_triangles[triangle_pos].v1_.pos_.z,
                                            indexed_triangles[triangle_pos].v1_.tex_.x,
                                            indexed_triangles[triangle_pos].v1_.tex_.y,
                                            indexed_triangles[triangle_pos].v1_.nml_.x,
                                            indexed_triangles[triangle_pos].v1_.nml_.y,
                                            indexed_triangles[triangle_pos].v1_.nml_.z);
            }
            break;
            case 2:
            {
                tri_pnt = XtndPoint<Kernel>(indexed_triangles[triangle_pos].v2_.pos_.x,
                                            indexed_triangles[triangle_pos].v2_.pos_.y,
                                            indexed_triangles[triangle_pos].v2_.pos_.z,
                                            indexed_triangles[triangle_pos].v2_.tex_.x,
                                            indexed_triangles[triangle_pos].v2_.tex_.y,
                                            indexed_triangles[triangle_pos].v2_.nml_.x,
                                            indexed_triangles[triangle_pos].v2_.nml_.y,
                                            indexed_triangles[triangle_pos].v2_.nml_.z);
            }
            break;
            }

            // compare point with all previous vertices
            // go backwards, neighbours are more likely to be added at the end of the list
            uint32_t vertex_id = vertices_indexed.size();
            for(int32_t p = (vertices_indexed.size() - 1); p >= 0; --p)
            {
                // if a match is found, record index
                if(XtndPoint<Kernel>::float_safe_equals(tri_pnt, vertices_indexed[p]))
                {
                    vertex_id = p;
                    break;
                }
            }
            // if no match then add to vertices list
            if(vertex_id == vertices_indexed.size())
            {
                vertices_indexed.push_back(tri_pnt);
            }
            // store index
            tris_indexed.push_back(vertex_id);
        }
    }
};