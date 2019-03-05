// Copyright (c) 2014-2018 Bauhaus-Universitaet Weimar
// This Software is distributed under the Modified BSD License, see license.txt.
//
// Virtual Reality and Visualization Research Group 
// Faculty of Media, Bauhaus-Universitaet Weimar
// http://www.uni-weimar.de/medien/vr

#include "split_screen_renderer.h"

#include <lamure/config.h>
#include <ctime>

split_screen_renderer::
split_screen_renderer(std::vector<scm::math::mat4f> const& model_transformations)
    : height_divided_by_top_minus_bottom_(0.f),
      near_plane_(0.f),
      far_minus_near_plane_(0.f),
      point_size_factor_(1.0f),
      render_mode_(0),
      ellipsify_(true),
      render_bounding_boxes_(false),
      clamped_normal_mode_(true),
      max_deform_ratio_(0.35f),
      fps_(0.0),
      rendered_splats_(0),
      uploaded_nodes_(0),
      is_cut_update_active_(true),
      resize_value(1.f),
      current_cam_id_left_(0),
      current_cam_id_right_(0),
      model_transformations_(model_transformations)
{

    lamure::ren::policy* policy = lamure::ren::policy::get_instance();
    win_x_ = policy->window_width();
    win_y_ = policy->window_height();

    initialize_device_and_shaders(win_x_, win_y_);
    initialize_vbos();
    reset_viewport(win_x_, win_y_);

    calc_rad_scale_factors();
}

split_screen_renderer::
~split_screen_renderer()
{
    filter_nearest_.reset();
    filter_linear_.reset();
    color_blending_state_.reset();

    change_point_size_in_shader_state_.reset();

    depth_state_less_.reset();
    depth_state_disable_.reset();

    pass1_visibility_shader_program_.reset();
    pass2_accumulation_shader_program_.reset();
    pass3_pass_trough_shader_program_.reset();

    render_target_shader_program_.reset();
    bounding_box_vis_shader_program_.reset();

    pass1_depth_buffer_.reset();
    pass1_visibility_fbo_.reset();

    user_0_fbo_.reset();
    user_1_fbo_.reset();
    user_0_fbo_texture_.reset();
    user_1_fbo_texture_.reset();

    pass2_accumulated_color_buffer_.reset();

    pass2_accumulation_fbo_.reset();

    pass_filling_color_texture_.reset();

    gaussian_texture_.reset();

    screen_quad_A_.reset();


    context_.reset();
    device_.reset();


}

void split_screen_renderer::
reset()
{

}

void split_screen_renderer::upload_uniforms(lamure::ren::camera const& camera) const
{
    using namespace scm::gl;
    using namespace scm::math;

    using namespace lamure::ren;

    mat4f    view_matrix         = camera.get_view_matrix();
    mat4f    model_matrix        = mat4f::identity();
    mat4f    projection_matrix   = camera.get_projection_matrix();

    mat4f    model_view_matrix   = view_matrix * model_matrix;

    pass1_visibility_shader_program_->uniform("mvp_matrix", projection_matrix * model_view_matrix  );
    pass1_visibility_shader_program_->uniform("model_view_matrix", model_view_matrix);

    pass1_visibility_shader_program_->uniform("height_divided_by_top_minus_bottom", height_divided_by_top_minus_bottom_);
    pass1_visibility_shader_program_->uniform("near_plane", near_plane_);
    pass1_visibility_shader_program_->uniform("far_minus_near_plane", far_minus_near_plane_);
    pass1_visibility_shader_program_->uniform("point_size_factor", point_size_factor_);

    pass1_visibility_shader_program_->uniform("inv_mv_matrix", scm::math::transpose(scm::math::inverse(model_view_matrix)));
    pass1_visibility_shader_program_->uniform("ellipsify", ellipsify_);
    pass1_visibility_shader_program_->uniform("clamped_normal_mode", clamped_normal_mode_);
    pass1_visibility_shader_program_->uniform("max_deform_ratio", max_deform_ratio_);


    pass2_accumulation_shader_program_->uniform("mvp_matrix", projection_matrix * model_view_matrix);
    pass2_accumulation_shader_program_->uniform("model_view_matrix", model_view_matrix);
    pass2_accumulation_shader_program_->uniform("depth_texture_pass1", 0);
    pass2_accumulation_shader_program_->uniform("pointsprite_texture", 1);
    pass2_accumulation_shader_program_->uniform("win_size", scm::math::vec2f(win_x_, win_y_) );

    pass2_accumulation_shader_program_->uniform("height_divided_by_top_minus_bottom", height_divided_by_top_minus_bottom_);
    pass2_accumulation_shader_program_->uniform("near_plane", near_plane_);
    pass2_accumulation_shader_program_->uniform("far_minus_near_plane", far_minus_near_plane_);
    pass2_accumulation_shader_program_->uniform("point_size_factor", point_size_factor_);
    pass2_accumulation_shader_program_->uniform("inv_mv_matrix", scm::math::transpose(scm::math::inverse(model_view_matrix)));

    pass2_accumulation_shader_program_->uniform("ellipsify", ellipsify_);
    pass2_accumulation_shader_program_->uniform("clamped_normal_mode", clamped_normal_mode_);
    pass2_accumulation_shader_program_->uniform("max_deform_ratio", max_deform_ratio_);


    pass3_pass_trough_shader_program_->uniform_sampler("in_color_texture", 0);

    pass3_pass_trough_shader_program_->uniform("renderMode", render_mode_);

    pass_filling_program_->uniform_sampler("in_color_texture", 0);
    pass_filling_program_->uniform_sampler("depth_texture", 1);
    pass_filling_program_->uniform("win_size", scm::math::vec2f(win_x_, win_y_) );

    pass_filling_program_->uniform("renderMode", render_mode_);

    bounding_box_vis_shader_program_->uniform("projection_matrix", projection_matrix);
    bounding_box_vis_shader_program_->uniform("model_view_matrix", model_view_matrix);



    context_->clear_default_color_buffer(FRAMEBUFFER_BACK, vec4f(.0f, .0f, .0f, 1.0f)); // how does the image look, if nothing is drawn
    context_->clear_default_depth_stencil_buffer();

    context_->apply();
}




void split_screen_renderer::
upload_transformation_matrices(lamure::ren::camera const& camera, lamure::model_t model_id, uint32_t pass_id) const
{

    using namespace lamure::ren;

    model_database* database = model_database::get_instance();
    const bvh* bvh = database->get_model(model_id)->get_bvh();

    scm::math::mat4f    view_matrix         = camera.get_view_matrix();
    scm::math::mat4f    model_matrix        = model_transformations_[model_id];

    scm::math::mat4f    projection_matrix   = camera.get_projection_matrix();

    scm::math::mat4f    model_view_matrix   = view_matrix * model_matrix;

    float rad_scale_fac = rad_scale_fac_[model_id];

    if(pass_id == 1)
    {
        pass1_visibility_shader_program_->uniform("mvp_matrix", projection_matrix * model_view_matrix  );
        pass1_visibility_shader_program_->uniform("model_view_matrix", model_view_matrix);
        pass1_visibility_shader_program_->uniform("inv_mv_matrix", scm::math::transpose(scm::math::inverse(model_view_matrix)));
        pass1_visibility_shader_program_->uniform("rad_scale_fac", rad_scale_fac);
    }
    else if(pass_id == 2)
    {
        pass2_accumulation_shader_program_->uniform("mvp_matrix", projection_matrix * model_view_matrix);
        pass2_accumulation_shader_program_->uniform("model_view_matrix", model_view_matrix);
        pass2_accumulation_shader_program_->uniform("inv_mv_matrix", scm::math::transpose(scm::math::inverse(model_view_matrix)));
        pass2_accumulation_shader_program_->uniform("rad_scale_fac", rad_scale_fac);
    }
    else if(pass_id == 5)
    {
        bounding_box_vis_shader_program_->uniform("projection_matrix", projection_matrix);
        bounding_box_vis_shader_program_->uniform("model_view_matrix", model_view_matrix );
    }
    else
    {
        std::cout<<"Shader does not need model_view_transformation\n\n";
    }


    context_->apply();

}



void split_screen_renderer::
render(lamure::context_t context_id, lamure::ren::camera const& camera, const lamure::view_t view_id, const uint32_t target)
{
    using namespace lamure;
    using namespace lamure::ren;

    update_frustum_dependent_parameters(camera);

    upload_uniforms(camera);

    using namespace scm::gl;
    using namespace scm::math;


    controller* controller = controller::get_instance();
    model_database* database = model_database::get_instance();
    cut_database* cuts = cut_database::get_instance();


    model_t models_count = database->num_models();


    std::vector<uint32_t>  frustum_culling_results;

    uint32_t size_of_culling_result_vector = 0;

    for (model_t model_id = 0; model_id < models_count; ++model_id)
    {
        cut& cut = cuts->get_cut(context_id, view_id, model_id);

        std::vector<cut::node_slot_aggregate> renderable = cut.complete_set();

        size_of_culling_result_vector += renderable.size();
    }


      frustum_culling_results.clear();
      frustum_culling_results.resize(size_of_culling_result_vector);


            {
                /***************************************************************************************
                *******************************BEGIN DEPTH PASS*****************************************
                ****************************************************************************************/


                {


                    context_->clear_depth_stencil_buffer(pass1_visibility_fbo_);


                    context_->set_frame_buffer(pass1_visibility_fbo_);

                    context_->set_viewport(viewport(vec2ui(0, 0), 1 * vec2ui(win_x_, win_y_)));

                    context_->bind_program(pass1_visibility_shader_program_);

                    context_->set_rasterizer_state(change_point_size_in_shader_state_);

                    scm::gl::vertex_array_ptr memory = controller->get_context_memory(context_id, bvh::primitive_type::POINTCLOUD, device_);
                    context_->bind_vertex_array(memory);


                    pass1_visibility_shader_program_->uniform("minsurfelsize", 1.0f);
                    pass1_visibility_shader_program_->uniform("QuantFactor", 1.0f);
                    context_->apply();

                    node_t node_counter = 0;

                    for (model_t model_id = 0; model_id < models_count; ++model_id)
                    {
                        const bvh* bvh = database->get_model(model_id)->get_bvh();

                        if (bvh->get_primitive() == bvh::primitive_type::POINTCLOUD) {

                            cut& cut = cuts->get_cut(context_id, view_id, model_id);

                            std::vector<cut::node_slot_aggregate> renderable = cut.complete_set();

                            uint32_t surfels_per_node_of_model = bvh->get_primitives_per_node();
                            //store culling result and push it back for second pass#

                            std::vector<scm::gl::boxf>const & bounding_box_vector = bvh->get_bounding_boxes();


                            upload_transformation_matrices(camera, model_id, 1);

                            scm::gl::frustum frustum_by_model = camera.get_frustum_by_model(model_transformations_[model_id]);


                            unsigned int leaf_level_start_index = bvh->get_first_node_id_of_depth(bvh->get_depth());

                            for(std::vector<cut::node_slot_aggregate>::const_iterator k = renderable.begin(); k != renderable.end(); ++k, ++node_counter)
                            {

                             //   uint32_t node_culling_result = camera.cull_against_frustum( frustum_by_model ,bounding_box_vector[ k->node_id_ ] );
                               uint32_t node_culling_result = 0;
                               //frustum_culling_results.push_back(node_culling_result);

                                 frustum_culling_results[node_counter] = node_culling_result;


                                if( (node_culling_result != 1) )
                                {


                                    bool is_leaf = (leaf_level_start_index <= k->node_id_);


                                    pass1_visibility_shader_program_->uniform("is_leaf", is_leaf);

                                    context_->apply();

                                    context_->draw_arrays(PRIMITIVE_POINT_LIST, (k->slot_id_) * database->get_primitives_per_node(), surfels_per_node_of_model);
                                }


                            }

                        }

                   }



                }



                /***************************************************************************************
                *******************************BEGIN ACCUMULATION PASS**********************************
                ****************************************************************************************/
                {

                    context_->clear_color_buffer(pass2_accumulation_fbo_ , 0, vec4f( .0f, .0f, .0f, 0.0f));
                    context_->clear_color_buffer(pass2_accumulation_fbo_ , 1, vec4f( .0f, .0f, .0f, 0.0f));

                    context_->set_frame_buffer(pass2_accumulation_fbo_);

                    context_->set_blend_state(color_blending_state_);
                    context_->set_depth_stencil_state(depth_state_disable_);

                    context_->bind_program(pass2_accumulation_shader_program_);

                    context_->bind_texture(pass1_depth_buffer_, filter_nearest_,   0);

                    context_->bind_texture( gaussian_texture_  ,  filter_linear_,   1);

                    scm::gl::vertex_array_ptr memory = controller->get_context_memory(context_id, bvh::primitive_type::POINTCLOUD, device_);
                    context_->bind_vertex_array(memory);
                    context_->apply();


                    pass2_accumulation_shader_program_->uniform("minsurfelsize", 1.0f);
                    pass2_accumulation_shader_program_->uniform("QuantFactor", 1.0f);
                    context_->apply();


                   node_t node_counter = 0;

                   node_t actually_rendered_nodes = 0;



                    for (model_t model_id = 0; model_id < models_count; ++model_id)
                    {
                        const bvh* bvh = database->get_model(model_id)->get_bvh();

                        if (bvh->get_primitive() == bvh::primitive_type::POINTCLOUD) {

                            cut& cut = cuts->get_cut(context_id, view_id, model_id);

                            std::vector<cut::node_slot_aggregate> renderable = cut.complete_set();

                            uint32_t surfels_per_node_of_model = bvh->get_primitives_per_node();
                            //store culling result and push it back for second pass#


                            upload_transformation_matrices(camera, model_id, 2);

                            unsigned int leaf_level_start_index = bvh->get_first_node_id_of_depth(bvh->get_depth());

                            for(std::vector<cut::node_slot_aggregate>::const_iterator k = renderable.begin(); k != renderable.end(); ++k, ++node_counter)
                            {

                                if( frustum_culling_results[node_counter] != 1)  // 0 = inside, 1 = outside, 2 = intersectingS
                                {

                                    bool is_leaf = (leaf_level_start_index <= k->node_id_);


                                    pass2_accumulation_shader_program_->uniform("is_leaf", is_leaf);

                                    context_->apply();


                                    context_->draw_arrays(PRIMITIVE_POINT_LIST, (k->slot_id_) * database->get_primitives_per_node(), surfels_per_node_of_model);

                                    ++actually_rendered_nodes;
                                }
                            }

                        }


                   }
                    rendered_splats_ = actually_rendered_nodes * database->get_primitives_per_node();

                }




                /***************************************************************************************
                *******************************BEGIN NORMALIZATION PASS*********************************
                ****************************************************************************************/
                {

                    context_->clear_color_buffer(pass_filling_fbo_, 0, vec4( 0, 0, 0, 0) );
                    context_->set_frame_buffer(pass_filling_fbo_);

                    context_->set_depth_stencil_state(depth_state_less_);

                    context_->bind_program(pass3_pass_trough_shader_program_);



                    context_->bind_texture(pass2_accumulated_color_buffer_, filter_nearest_,   0);
                    context_->apply();

                    screen_quad_A_->draw(context_);


                }



                /***************************************************************************************
                *******************************BEGIN RECONSTRUCTION PASS*********************************
                ****************************************************************************************/
                {

                    if (target == 0)
                    {
                        context_->set_frame_buffer(user_0_fbo_);
                        context_->clear_color_buffer(user_0_fbo_, 0, vec4( 0, 0, 0, 0) );
                    }
                    else
                    {
                        context_->set_frame_buffer(user_1_fbo_);
                        context_->clear_color_buffer(user_1_fbo_, 0, vec4( 0, 0, 0, 0) );
                    }

                    context_->bind_program(pass_filling_program_);

                    context_->bind_texture(pass_filling_color_texture_, filter_nearest_,   0);
                    context_->bind_texture(pass1_depth_buffer_, filter_nearest_,   1);
                    context_->apply();

                    screen_quad_A_->draw(context_);
                }


                if (target == 1)
                {

                    /***************************************************************************************
                    *******************************BEGIN RENDER TARGET COMBINE******************************
                    ****************************************************************************************/
                    {

                        context_->set_default_frame_buffer();

                        context_->set_viewport(scm::gl::viewport(scm::math::vec2ui(0, 0), 1 * scm::math::vec2ui(win_x_*(1/resize_value), win_y_*(1/resize_value))));
                        context_->bind_program(render_target_shader_program_);

                        context_->bind_texture(user_0_fbo_texture_, filter_nearest_,   0);

                        context_->apply();

                        screen_quad_A_->draw(context_);

                        context_->set_viewport(scm::gl::viewport(scm::math::vec2ui(win_x_*(1/resize_value), 0), 1 * scm::math::vec2ui(win_x_*(1/resize_value), win_y_*(1/resize_value))));
                        context_->bind_program(render_target_shader_program_);

                        context_->bind_texture(user_1_fbo_texture_, filter_nearest_,   0);

                        context_->apply();

                        screen_quad_A_->draw(context_);


                    }

                    context_->apply();
                }





        }//end if render_splats_



        context_->reset();

        if (target == 1)
        {
            frame_time_.stop();


            if (true)
            {
                //schism bug ? time::to_seconds yields milliseconds
                if (scm::time::to_seconds(frame_time_.accumulated_duration()) > 100.0)
                {

                    fps_ = 1000.0f / scm::time::to_seconds(frame_time_.average_duration());


                    frame_time_.reset();
                }
            }

            frame_time_.start();

          display_status();
      }

      context_->reset();







}




void split_screen_renderer::send_model_transform(const lamure::model_t model_id, const scm::math::mat4f& transform) {
    model_transformations_[model_id] = transform;

}



void split_screen_renderer::display_status()
{
    context_->set_viewport(scm::gl::viewport(scm::math::vec2ui(0, 0), 1 * scm::math::vec2ui(win_x_*(1/resize_value), win_y_*(1/resize_value))));

    std::stringstream os;
   // os.setprecision(5);
    os<<"FPS:   "<<std::setprecision(4)<<fps_<<"\n"
      <<"PointsizeFactor:   "<<point_size_factor_<<"\n"
      <<"Normal Clamping:   "<< (clamped_normal_mode_ ? "ON" : "OFF")<<"\n"
      <<"Clamping Threshold:   "<<max_deform_ratio_<<"\n"
      <<"Splat Mode:   "<<(ellipsify_ ? "elliptical" : "round")<<"\n"
      <<"render Mode:   "<<(render_mode_ == 0 ? "Color" : (render_mode_ == 1 ? "LOD" : "Normal"))<<"\n"
      <<"rendered Splats:   "<<rendered_splats_<<"\n"
      <<"Uploaded Nodes:   "<<uploaded_nodes_<<"\n"
      <<"\n"
      <<"cut Update:   "<< (is_cut_update_active_ == true ? "active" : "frozen") <<"\n"
      <<"Left camera :   "<< current_cam_id_left_<<"\n"
      <<"Right camera :   "<< current_cam_id_right_;


    renderable_text_->text_string(os.str());
    text_renderer_->draw_shadowed(context_, scm::math::vec2i(10, win_y_- 20), renderable_text_);

    rendered_splats_ = 0;
    uploaded_nodes_ = 0;


}

void split_screen_renderer::
initialize_vbos()
{

    // init the GL context
    using namespace scm;
    using namespace scm::gl;
    using namespace scm::math;



    filter_nearest_ = device_->create_sampler_state(FILTER_MIN_MAG_LINEAR, WRAP_CLAMP_TO_EDGE);

    filter_linear_ = device_->create_sampler_state(FILTER_MIN_MAG_LINEAR, WRAP_CLAMP_TO_EDGE);

    change_point_size_in_shader_state_ = device_->create_rasterizer_state(FILL_SOLID, CULL_NONE, ORIENT_CCW, false, false, 0.0, false, false, point_raster_state(true));

    pass1_visibility_fbo_ = device_->create_frame_buffer();


    pass1_depth_buffer_           =device_->create_texture_2d(vec2ui(win_x_, win_y_), FORMAT_D32F, 1, 1, 1);




    pass1_visibility_fbo_->attach_depth_stencil_buffer(pass1_depth_buffer_);



    pass2_accumulation_fbo_ = device_->create_frame_buffer();

    pass2_accumulated_color_buffer_   = device_->create_texture_2d(vec2ui(win_x_, win_y_) , FORMAT_RGBA_32F , 1, 1, 1);



    pass2_accumulation_fbo_->attach_color_buffer(0, pass2_accumulated_color_buffer_);


    screen_quad_A_.reset(new scm::gl::quad_geometry(device_, scm::math::vec2f(-1.0f, -1.0f), scm::math::vec2f(1.0f, 1.0f)));


    float gaussian_buffer[32] = {255, 255, 252, 247, 244, 234, 228, 222, 208, 201,
                                 191, 176, 167, 158, 141, 131, 125, 117, 100,  91,
                                 87,  71,  65,  58,  48,  42,  39,  32,  28,  25,
                                 19,  16};



    texture_region ur(vec3ui(0u), vec3ui(32, 1, 1));

    gaussian_texture_ = device_->create_texture_2d(vec2ui(32,1), FORMAT_R_32F, 1, 1, 1);
    context_->update_sub_texture(gaussian_texture_, ur, 0u, FORMAT_R_32F, gaussian_buffer);



    color_blending_state_ = device_->create_blend_state(true, FUNC_ONE, FUNC_ONE, FUNC_ONE, FUNC_ONE, EQ_FUNC_ADD, EQ_FUNC_ADD);




    depth_state_less_ = device_->create_depth_stencil_state(true, true, COMPARISON_LESS);

    depth_stencil_state_desc no_depth_test_descriptor = depth_state_less_->descriptor();
    no_depth_test_descriptor._depth_test = false;

    depth_state_disable_ = device_->create_depth_stencil_state(no_depth_test_descriptor);



    user_0_fbo_ = device_->create_frame_buffer();
    user_0_fbo_texture_ = device_->create_texture_2d(vec2ui(win_x_, win_y_) * 1, FORMAT_RGBA_32F , 1, 1, 1);
    user_0_fbo_->attach_color_buffer(0, user_0_fbo_texture_);



    user_1_fbo_ = device_->create_frame_buffer();
    user_1_fbo_texture_ = device_->create_texture_2d(vec2ui(win_x_, win_y_) * 1, FORMAT_RGBA_32F , 1, 1, 1);
    user_1_fbo_->attach_color_buffer(0, user_1_fbo_texture_);

}

bool split_screen_renderer::
initialize_device_and_shaders(int resX, int resY)
{
    std::string root_path = LAMURE_SHADERS_DIR;

    std::string visibility_vs_source;
    std::string visibility_fs_source;

    std::string pass_trough_vs_source;
    std::string pass_trough_fs_source;

    std::string accumulation_vs_source;
    std::string accumulation_fs_source;

    std::string filling_vs_source;
    std::string filling_fs_source;

    std::string bounding_box_vs_source;
    std::string bounding_box_fs_source;


    std::string render_target_vs_source;
    std::string render_target_fs_source;



    if (   !scm::io::read_text_file(root_path +  "/pass1_visibility_pass.glslv", visibility_vs_source)
        || !scm::io::read_text_file(root_path +  "/pass1_visibility_pass.glslf", visibility_fs_source)
        || !scm::io::read_text_file(root_path +  "/pass2_accumulation_pass.glslv", accumulation_vs_source)
        || !scm::io::read_text_file(root_path +  "/pass2_accumulation_pass.glslf", accumulation_fs_source)
        || !scm::io::read_text_file(root_path +  "/pass3_pass_trough.glslv", pass_trough_vs_source)
        || !scm::io::read_text_file(root_path +  "/pass3_pass_trough.glslf", pass_trough_fs_source)
        || !scm::io::read_text_file(root_path +  "/pass_reconstruction.glslv", filling_vs_source)
        || !scm::io::read_text_file(root_path +  "/pass_reconstruction.glslf", filling_fs_source)
        || !scm::io::read_text_file(root_path +  "/bounding_box_vis.glslv", bounding_box_vs_source)
        || !scm::io::read_text_file(root_path +  "/bounding_box_vis.glslf", bounding_box_fs_source)
        || !scm::io::read_text_file(root_path +  "/render_target.glslv", render_target_vs_source)
        || !scm::io::read_text_file(root_path +  "/render_target.glslf", render_target_fs_source)
       )
    {
        scm::err() << "error reading shader files" << scm::log::end;
        return false;
    }

    device_.reset(new scm::gl::render_device());

    context_ = device_->main_context();

    pass1_visibility_shader_program_ = device_->create_program(boost::assign::list_of(device_->create_shader(scm::gl::STAGE_VERTEX_SHADER, visibility_vs_source))
                                                               (device_->create_shader(scm::gl::STAGE_FRAGMENT_SHADER, visibility_fs_source)));

    pass2_accumulation_shader_program_ = device_->create_program(boost::assign::list_of(device_->create_shader(scm::gl::STAGE_VERTEX_SHADER, accumulation_vs_source))
                                                                 (device_->create_shader(scm::gl::STAGE_FRAGMENT_SHADER,accumulation_fs_source)));

    pass3_pass_trough_shader_program_ = device_->create_program(boost::assign::list_of(device_->create_shader(scm::gl::STAGE_VERTEX_SHADER, pass_trough_vs_source))
                                                                (device_->create_shader(scm::gl::STAGE_FRAGMENT_SHADER, pass_trough_fs_source)));

    pass_filling_program_ = device_->create_program(boost::assign::list_of(device_->create_shader(scm::gl::STAGE_VERTEX_SHADER, filling_vs_source))
                                                    (device_->create_shader(scm::gl::STAGE_FRAGMENT_SHADER, filling_fs_source)));

    bounding_box_vis_shader_program_ = device_->create_program(boost::assign::list_of(device_->create_shader(scm::gl::STAGE_VERTEX_SHADER, bounding_box_vs_source))
                                                               (device_->create_shader(scm::gl::STAGE_FRAGMENT_SHADER, bounding_box_fs_source)));


    render_target_shader_program_ = device_->create_program(boost::assign::list_of(device_->create_shader(scm::gl::STAGE_VERTEX_SHADER, render_target_vs_source))
                                                            (device_->create_shader(scm::gl::STAGE_FRAGMENT_SHADER, render_target_fs_source)));

    if (!pass1_visibility_shader_program_ || !pass2_accumulation_shader_program_ || !pass3_pass_trough_shader_program_ || !pass_filling_program_ || !bounding_box_vis_shader_program_) {
        scm::err() << "error creating shader programs" << scm::log::end;
        return false;
    }


    scm::out() << *device_ << scm::log::end;


    using namespace scm;
    using namespace scm::gl;
    using namespace scm::math;

    try
    {
        font_face_ptr output_font(new font_face(device_, std::string(LAMURE_FONTS_DIR) + "/Ubuntu.ttf", 18, 0, font_face::smooth_lcd));
        text_renderer_  =     scm::make_shared<text_renderer>(device_);
        renderable_text_    = scm::make_shared<scm::gl::text>(device_, output_font, font_face::style_regular, "sick, sad world...");

        mat4f   fs_projection = make_ortho_matrix(0.0f, static_cast<float>(win_x_),
                                                  0.0f, static_cast<float>(win_y_), -1.0f, 1.0f);
        text_renderer_->projection_matrix(fs_projection);

        renderable_text_->text_color(math::vec4f(1.0f, 1.0f, 0.0f, 1.0f));
        renderable_text_->text_kerning(true);
    }
    catch(const std::exception& e) {
        throw std::runtime_error(std::string("vtexture_system::vtexture_system(): ") + e.what());
    }

    return true;
}

void split_screen_renderer::reset_viewport(int w, int h)
{

    win_x_ = resize_value*w;
    win_y_ = resize_value*h;


    pass1_visibility_fbo_ = device_->create_frame_buffer();

    pass1_depth_buffer_           =device_->create_texture_2d(scm::math::vec2ui(win_x_, win_y_) * 1, scm::gl::FORMAT_D24, 1, 1, 1);




    pass1_visibility_fbo_->attach_depth_stencil_buffer(pass1_depth_buffer_);



    pass2_accumulation_fbo_ = device_->create_frame_buffer();

    pass2_accumulated_color_buffer_   = device_->create_texture_2d(scm::math::vec2ui(win_x_, win_y_) * 1, scm::gl::FORMAT_RGBA_32F , 1, 1, 1);



    pass2_accumulation_fbo_->attach_color_buffer(0, pass2_accumulated_color_buffer_);


    pass_filling_fbo_ = device_->create_frame_buffer();

    pass_filling_color_texture_ = device_->create_texture_2d(scm::math::vec2ui(win_x_, win_y_) * 1, scm::gl::FORMAT_RGBA_8 , 1, 1, 1);

    pass_filling_fbo_->attach_color_buffer(0, pass_filling_color_texture_);

    scm::math::mat4f   fs_projection = scm::math::make_ortho_matrix(0.0f, static_cast<float>(win_x_),
                                                  0.0f, static_cast<float>(win_y_), -1.0f, 1.0f);
    text_renderer_->projection_matrix(fs_projection);



    user_0_fbo_ = device_->create_frame_buffer();
    user_0_fbo_texture_ = device_->create_texture_2d(scm::math::vec2ui(win_x_, win_y_) * 1, scm::gl::FORMAT_RGBA_32F , 1, 1, 1);
    user_0_fbo_->attach_color_buffer(0, user_0_fbo_texture_);


    user_1_fbo_ = device_->create_frame_buffer();
    user_1_fbo_texture_ = device_->create_texture_2d(scm::math::vec2ui(win_x_, win_y_) * 1, scm::gl::FORMAT_RGBA_32F , 1, 1, 1);
    user_1_fbo_->attach_color_buffer(0, user_1_fbo_texture_);


}

void split_screen_renderer::
update_frustum_dependent_parameters(lamure::ren::camera const& camera)
{
    near_plane_ = camera.near_plane_value();
    far_minus_near_plane_ = camera.far_plane_value() - near_plane_;

    std::vector<scm::math::vec3d> corner_values = camera.get_frustum_corners();
    double top_minus_bottom = scm::math::length((corner_values[2]) - (corner_values[0]));

    height_divided_by_top_minus_bottom_ = win_y_ / top_minus_bottom;
}



void split_screen_renderer::
calc_rad_scale_factors()
{
    using namespace lamure::ren;

   uint32_t num_models = (model_database::get_instance())->num_models();

   if(rad_scale_fac_.size() < num_models)
      rad_scale_fac_.resize(num_models);

   scm::math::vec4f x_unit_vec = scm::math::vec4f(1.0,0.0,0.0,0.0);
   for(int model_id = 0; model_id < num_models; ++model_id)
   {
     rad_scale_fac_[model_id] = scm::math::length(model_transformations_[model_id] * x_unit_vec);
   }
}

//dynamic render adjustment functions

void split_screen_renderer::
switch_bounding_box_rendering()
{
    render_bounding_boxes_ = ! render_bounding_boxes_;

    std::cout<<"bounding box visualisation: ";
    if(render_bounding_boxes_)
        std::cout<<"ON\n\n";
    else
        std::cout<<"OFF\n\n";
};

void split_screen_renderer::
switch_surfel_rendering()
{
    render_splats_ = ! render_splats_;
    std::cout<<"splat rendering: ";
    if(render_splats_)
        std::cout<<"ON\n\n";
    else
        std::cout<<"OFF\n\n";
};


void split_screen_renderer::
change_pointsize(float amount)
{
    point_size_factor_ += amount;
    if(point_size_factor_ < 0.0001f)
    {
        point_size_factor_ = 0.0001;
    }

    std::cout<<"set point size factor to: "<<point_size_factor_<<"\n\n";
};


void split_screen_renderer::
switch_render_mode()
{
    render_mode_ = (render_mode_ + 1)%2;

    std::cout<<"switched render mode to: ";
    switch(render_mode_)
    {
        case 0:
            std::cout<<"COLOR\n\n";
            break;
        case 1:
            std::cout<<"NORMAL\n\n";
            break;
        case 2:
            std::cout<<"DEPTH\n\n";
            break;
        default:
            std::cout<<"UNKNOWN\n\n";
            break;
    }
};

void split_screen_renderer::
switch_ellipse_mode()
{
    ellipsify_ = ! ellipsify_;
    std::cout<<"splat mode: ";
    if(ellipsify_)
        std::cout<<"ELLIPTICAL\n\n";
    else
        std::cout<<"ROUND\n\n";
};

void split_screen_renderer::
switch_clamped_normal_mode()
{
    clamped_normal_mode_ = !clamped_normal_mode_;
    std::cout<<"clamp elliptical deform ratio: ";
    if(clamped_normal_mode_)
        std::cout<<"ON\n\n";
    else
        std::cout<<"OFF\n\n";
};

void split_screen_renderer::
change_deform_ratio(float amount)
{
    max_deform_ratio_ += amount;

    if(max_deform_ratio_ < 0.0f)
        max_deform_ratio_ = 0.0f;
    else if(max_deform_ratio_ > 5.0f)
        max_deform_ratio_ = 5.0f;

    std::cout<<"set elliptical deform ratio to: "<<max_deform_ratio_<<"\n\n";
};

void split_screen_renderer::
toggle_cut_update_info()
{
    is_cut_update_active_ = ! is_cut_update_active_;
}


void split_screen_renderer::
toggle_camera_info(const lamure::view_t left_cam_id, const lamure::view_t right_cam_id)
{
    current_cam_id_left_ = left_cam_id;
    current_cam_id_right_ = right_cam_id;
}

void split_screen_renderer::
toggle_display_info()
{
    display_info_ = ! display_info_;
}

