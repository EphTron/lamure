// Copyright (c) 2014 Bauhaus-Universitaet Weimar
// This Software is distributed under the Modified BSD License, see license.txt.
//
// Virtual Reality and Visualization Research Group 
// Faculty of Media, Bauhaus-Universitaet Weimar
// http://www.uni-weimar.de/medien/vr

#ifndef REN_CONTROLLER_H_
#define REN_CONTROLLER_H_

#include <unordered_map>
#include <lamure/utils.h>
#include <lamure/types.h>

#include <lamure/ren/platform.h>
#include <lamure/ren/cut_update_pool.h>
#include <lamure/ren/gpu_context.h>


namespace lamure {
namespace ren {

class RENDERING_DLL controller
{
public:
    typedef size_t gua_context_desc_t;
    typedef view_t gua_view_desc_t;
    typedef std::string gua_model_desc_t;

    static controller* get_instance();

                        controller(const controller&) = delete;
                        controller& operator=(const controller&) = delete;
    virtual             ~controller();

    void                signal_system_reset();
    void                reset_system();
    const bool          is_system_reset_signaled(const context_t context_id);

    context_t           deduce_context_id(const gua_context_desc_t context_desc);
    view_t              deduce_view_id(const gua_context_desc_t context_desc, const gua_view_desc_t view_desc);
    model_t             deduce_model_id(const gua_model_desc_t& model_desc);

    const bool          is_model_present(const gua_model_desc_t model_desc);
    const context_t     num_contexts_registered();

    void                dispatch(const context_t context_id, scm::gl::render_device_ptr device);
    const bool          is_cut_update_in_progress(const context_t context_id);

    scm::gl::buffer_ptr get_context_buffer(const context_t context_id, scm::gl::render_device_ptr device);
    scm::gl::vertex_array_ptr get_context_memory(const context_t context_id, scm::gl::render_device_ptr device);

    size_t              ms_since_last_node_upload() {return ms_since_last_node_upload_; };
    void                reset_ms_since_last_node_upload() { ms_since_last_node_upload_ = 0;};

protected:
                        controller();
    static bool         is_instanced_;
    static controller*  single_;

private:
    static std::mutex   mutex_;

    std::unordered_map<context_t, cut_update_pool*> cut_update_pools_;
    std::unordered_map<context_t, gpu_context*> gpu_contexts_;

    std::unordered_map<gua_context_desc_t, context_t> context_map_;
    context_t num_contexts_registered_;

    std::unordered_map<context_t, std::unordered_map<gua_view_desc_t, view_t>> view_map_;
    std::vector<view_t> num_views_registered_;

    std::unordered_map<gua_model_desc_t, model_t> model_map_;
    model_t             num_models_registered_;

    std::unordered_map<context_t, std::queue<bool>> reset_flags_history_;

    size_t ms_since_last_node_upload_;
    std::chrono::time_point<std::chrono::system_clock> latest_timestamp_;
};


} } // namespace lamure


#endif // REN_CONTROLLER_H_
