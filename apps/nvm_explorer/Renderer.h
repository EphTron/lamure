#ifndef LAMURE_RENDERER_H
#define LAMURE_RENDERER_H

// #include <lamure/utils.h>
// #include <lamure/types.h>
#include "Camera_View.h"
#include "Scene.h"
#include "Struct_Surfel_Brush.h"

#include <iostream>

#include <scm/core.h>
#include <scm/core/io/tools.h>
#include <scm/core/pointer_types.h>
#include <scm/core/time/accum_timer.h>
#include <scm/core/time/high_res_timer.h>
#include <scm/log.h>

#include <scm/gl_util/data/imaging/texture_loader.h>
#include <scm/gl_util/primitives/box.h>
#include <scm/gl_util/primitives/quad.h>
#include <scm/gl_util/viewer/camera.h>

#include <scm/core/math.h>

#include <scm/gl_core/gl_core_fwd.h>
#include <scm/gl_util/primitives/geometry.h>
#include <scm/gl_util/primitives/primitives_fwd.h>

#include <scm/gl_util/font/font_face.h>
#include <scm/gl_util/font/text.h>
#include <scm/gl_util/font/text_renderer.h>

#include <scm/core/platform/platform.h>
#include <scm/core/utilities/platform_warning_disable.h>
#include <scm/gl_util/primitives/geometry.h>

#include <lamure/ren/controller.h>
#include <lamure/ren/cut_database.h>
#include <lamure/ren/model_database.h>
#include <lamure/ren/policy.h>

#include <boost/assign/list_of.hpp>
#include <memory>

#include <fstream>
#include <lamure/types.h>
#include <lamure/utils.h>
#include <map>
#include <vector>

#include <lamure/ren/cut.h>
#include <lamure/ren/cut_update_pool.h>
#include <lamure/ren/ray.h>

#include <lamure/pro/partitioning/SparseOctree.h>

class Renderer
{
  public:
    // Renderer();
    void init(char **argv, scm::shared_ptr<scm::gl::render_device> device, int width_window, int height_window, std::string name_file_lod, std::string name_file_dense,
              lamure::ren::Data_Provenance data_provenance);
    bool render(Scene &scene);

    void update_state_lense();
    void translate_sphere(scm::math::vec3f offset);
    void update_radius_sphere(float offset);

    void update_size_point(float scale);
    void update_size_pixels_brush(float scale);

    Camera_View &get_camera_view();

    void toggle_camera(Scene scene);
    void toggle_is_camera_active();
    void previous_camera(Scene scene);
    void next_camera(Scene scene);
    void handle_mouse_movement(int x, int y);

    void start_brushing(int x, int y, Scene &scene);
    std::vector<uint16_t> search_tree(scm::math::vec3f const &surfel_brush, Scene &scene);

    bool dense_points_only = false;
    bool mode_draw_points_dense = false;
    bool mode_draw_images = true;
    bool mode_draw_lines = false;
    bool mode_draw_cameras = true;
    int mode_prov_data = 0;
    bool _mode_depth_test_surfels_brush = true;
    bool dispatch = true;

    bool is_default_camera = true;

    bool is_camera_active = false;
    unsigned long index_current_image_camera = 0;

  private:
    scm::shared_ptr<scm::gl::render_context> _context;
    scm::shared_ptr<scm::gl::render_device> _device;
    scm::gl::program_ptr _program_points;
    scm::gl::program_ptr _program_points_dense;
    scm::gl::program_ptr _program_cameras;
    scm::gl::program_ptr _program_images;
    scm::gl::program_ptr _program_frustra;
    scm::gl::program_ptr _program_lines;
    scm::gl::program_ptr _program_legend;
    scm::gl::program_ptr _program_surfels_brush;
    scm::gl::program_ptr _program_pixels_brush;

    scm::shared_ptr<prov::SparseOctree> _sparse_octree = nullptr;

    lamure::ren::Data_Provenance _data_provenance;

    // scm::shared_ptr<scm::gl::quad_geometry> _quad_legend;
    scm::gl::buffer_ptr _vertex_buffer_object_lines;
    scm::gl::vertex_array_ptr _vertex_array_object_lines;

    scm::gl::rasterizer_state_ptr _rasterizer_state;

    float _size_point_dense = 0.1f;
    float _size_point_sparse_float = 1.0f;
    int _size_point_sparse = 1;

    int _buffer_surfels_brush_size = 0;

    float _size_pixels_brush_minimum = 4.0f * 1.12f / 1000000.0f;
    float _size_pixels_brush_current = 4.0f * 1.12f / 1000.0f;

    Camera_View _camera_view;
    lamure::ren::camera *_camera = new lamure::ren::camera();

    float _radius_sphere = 300.0;
    scm::math::vec3f _position_sphere = scm::math::vec3f(0.0f, 0.0f, 0.0f);
    int _state_lense = 0;

    void draw_points_sparse(Scene &scene);
    void draw_cameras(Scene &scene);
    void draw_lines_test();
    void draw_lines(Scene &scene);
    void draw_images(Scene &scene);
    void draw_frustra(Scene &scene);
    bool draw_points_dense(Scene &scene);
    void draw_surfels_brush();
    void draw_pixels_brush(Scene &scene);

    scm::gl::depth_stencil_state_ptr depth_state_disable_;
    scm::gl::depth_stencil_state_ptr depth_state_enable_;

    void add_surfel_brush(Struct_Surfel_Brush const &surfel_brush, Scene &scene);

    lamure::model_t _model_id;
    // BRUSHING ///////////////////////////////////////////////
    std::vector<Struct_Surfel_Brush> _surfels_brush;

    scm::gl::buffer_ptr _vertex_buffer_object_surfels_brush;
    scm::gl::vertex_array_ptr _vertex_array_object_surfels_brush;

    scm::math::vec3f convert_to_world_space(int x, int y, int z);
    // void draw_legend();
};

#endif // LAMURE_RENDERER_H
