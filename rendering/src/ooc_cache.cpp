// Copyright (c) 2014 Bauhaus-Universitaet Weimar
// This Software is distributed under the Modified BSD License, see license.txt.
//
// Virtual Reality and Visualization Research Group 
// Faculty of Media, Bauhaus-Universitaet Weimar
// http://www.uni-weimar.de/medien/vr

#include <lamure/ren/ooc_cache.h>


namespace lamure
{

namespace ren
{

std::mutex ooc_cache::mutex_;
bool ooc_cache::is_instanced_ = false;
ooc_cache* ooc_cache::single_ = nullptr;

ooc_cache::
ooc_cache(const slot_t num_slots)
: cache(num_slots),
  maintenance_counter_(0) {
    model_database* database = model_database::get_instance();

    cache_data_ = new char[num_slots * database->size_of_surfel() * database->surfels_per_node()];

    pool_ = new ooc_pool(LAMURE_CUT_UPDATE_NUM_LOADING_THREADS,
                        database->surfels_per_node() * database->size_of_surfel());

#ifdef LAMURE_ENABLE_INFO
    std::cout << "PLOD: ooc-cache init" << std::endl;
#endif

}

ooc_cache::
~ooc_cache() {
    std::lock_guard<std::mutex> lock(mutex_);

    is_instanced_ = false;

    if (pool_ != nullptr) {
        delete pool_;
        pool_ = nullptr;
    }

    if (cache_data_ != nullptr) {
        delete[] cache_data_;
        cache_data_ = nullptr;

#ifdef LAMURE_ENABLE_INFO
        std::cout << "PLOD: ooc-cache shutdown" << std::endl;
#endif
    }

}

ooc_cache* ooc_cache::
get_instance() {
    if (!is_instanced_) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!is_instanced_) {
            policy* policy = policy::get_instance();
            model_database* database = model_database::get_instance();
            size_t size_of_node_in_bytes = database->size_of_surfel() * database->surfels_per_node();
            size_t out_of_core_budget_in_nodes = (policy->out_of_core_budget_in_mb()*1024*1024) / size_of_node_in_bytes;
            single_ = new ooc_cache(out_of_core_budget_in_nodes);
            is_instanced_ = true;
        }

        return single_;
    }
    else {
        return single_;
    }
}

void ooc_cache::
register_node(const model_t model_id, const node_t node_id, const int32_t priority) {
    if (is_node_resident(model_id, node_id)) {
        return;
    }

    cache_queue::query_result query_result = pool_->acknowledge_query(model_id, node_id);

    switch (query_result) {
        case cache_queue::query_result::NOT_INDEXED:
        {
            slot_t slot_id = index_->reserve_slot();
            cache_queue::job job(model_id, node_id, slot_id, priority, cache_data_ + slot_id * slot_size());
            if (!pool_->acknowledge_request(job)) {
                index_->unreserve_slot(slot_id);
            }
            break;
        }

        case cache_queue::query_result::INDEXED_AS_WAITING:
            pool_->acknowledge_update(model_id, node_id, priority);
            break;

        case cache_queue::query_result::INDEXED_AS_LOADING:
            //note: this means the queue is either not updateable at all
            //or the node cannot be updated anymore, so we do nothing
            break;

        default: break;
    }

}

char* ooc_cache::
node_data(const model_t model_id, const node_t node_id) {
    return cache_data_ + index_->get_slot(model_id, node_id) * slot_size();

}

const bool ooc_cache::
is_node_resident_and_aquired(const model_t model_id, const node_t node_id) {
    return index_->is_node_aquired(model_id, node_id);

}

void ooc_cache::
refresh() {
    pool_->lock();
    pool_->resolve_cache_history(index_);


#ifdef LAMURE_CUT_UPDATE_ENABLE_CACHE_MAINTENANCE
    //if (!in_core_mode_)
    {
        ++maintenance_counter_;

        if (maintenance_counter_ > LAMURE_CUT_UPDATE_CACHE_MAINTENANCE_COUNTER) {
            pool_->perform_queue_maintenance(index_);
            maintenance_counter_ = 0;
        }
    }
#endif


    pool_->unlock();
}

void ooc_cache::
lock_pool() {
    pool_->lock();
    pool_->resolve_cache_history(index_);
}

void ooc_cache::
unlock_pool() {
    pool_->unlock();

}

void ooc_cache::
begin_measure() {
  pool_->begin_measure();
}

void ooc_cache::
end_measure() {
  pool_->end_measure();
}


} // namespace ren

} // namespace lamure


