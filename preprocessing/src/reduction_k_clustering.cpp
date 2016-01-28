// Copyright (c) 2014 Bauhaus-Universitaet Weimar
// This Software is distributed under the Modified BSD License, see license.txt.
//
// Virtual Reality and Visualization Research Group
// Faculty of Media, Bauhaus-Universitaet Weimar
// http://www.uni-weimar.de/medien/vr

#include <lamure/pre/reduction_k_clustering.h>

#include <lamure/pre/basic_algorithms.h>
#include <lamure/utils.h>

#include <queue>
#include <array>
#include <utility>   // std::pair
#include <algorithm>    // std::max
#include <math.h>  //floor
#include <cmath> //std::fabs

#include <forward_list>
#include <iterator> 

namespace lamure {
namespace pre {


int reduction_k_clustering::
get_largest_dim(vec3f const& avg_normal) const{

    //compare normal absolute values of the 3 normal components 

    if (!((avg_normal.x == avg_normal.y)&&(avg_normal.z == avg_normal.y))) {

        if (avg_normal.x == std::max(std::fabs(avg_normal.x), std::fabs(avg_normal.y)) && std::fabs(avg_normal.y) >= std::fabs(avg_normal.z)) {
            return 0;
        }
        else if (avg_normal.y == std::max(std::fabs(avg_normal.x), std::fabs(avg_normal.y)) && std::fabs(avg_normal.x) >= std::fabs(avg_normal.z)) {
            return 1;
        }

        else if (avg_normal.z == std::max(std::fabs(avg_normal.z), std::fabs(avg_normal.y)) && std::fabs(avg_normal.y) >= std::fabs(avg_normal.x)) {
            return 2;
        } else {
            return 0;
        }

    } else {
        return 0; 
    }


}

shared_cluster_surfel_vector reduction_k_clustering::
get_initial_cluster_seeds(vec3f const& avg_normal, shared_cluster_surfel_vector const& input_surfels) const{
    //hash-based grouping
    //reference: http://www.ifi.uzh.ch/vmml/publications/older-puclications/DeferredBlending.pdf 

    const int group_num = 8; //set as member var. if user-defind value needed  but then consider different index distribution function depending on this mun.
    std::array <shared_cluster_surfel_vector, group_num> cluster_array; // container with 8 (in this case) subgroups


    int avg_dim = get_largest_dim(avg_normal); 

        /*for (int i = 0; i < group_num; ++i){

        }*/

    for (auto current_surfel : input_surfels){

        uint16_t x_coord, y_coord ;  //variables to store 2D coordinate mapping

        x_coord = std::floor((current_surfel->contained_surfel->pos()[(avg_dim + 1) % 3] )/(current_surfel->contained_surfel->radius()));
        y_coord = std::floor((current_surfel->contained_surfel->pos()[(avg_dim + 2) % 3] )/(current_surfel->contained_surfel->radius()));
        uint16_t group_id = (x_coord*3 + y_coord) % group_num; // formula might need to be reconsidered for different group_num
        cluster_array[group_id].push_back(current_surfel);
    }

    //determine which array member hast biggest simber of elememts
    int max_size_group_id = -1;
    int32_t max_num_elements = -1;
    for (int i = 0; i < group_num; ++i){

        int32_t temp_num_elements = cluster_array[i].size();

        if (max_num_elements < temp_num_elements){
            max_num_elements = temp_num_elements;
            max_size_group_id = i;
        }

        //std::cout << "Group: " << i << " contains " << cluster_array[i].size() << " surfels \n";

    }

        //std::cout << "Selected group is "  <<  max_size_group_id << "\n";   

    for(auto const& current_surfel : cluster_array[max_size_group_id]){
       // current_surfel->contained_surfel->color() = vec3b (255, 255, 255);
        update_cluster_membership(current_surfel, true);
    }

    return cluster_array[max_size_group_id];

}


vec3f reduction_k_clustering::  
compute_avg_normal(shared_cluster_surfel_vector const& input_surfels) const{

    vec3f avg_normal (0.0, 0.0, 0.0);
    if( input_surfels.size() != 0){

        for (auto const& current_cluster_surfel_ptr : input_surfels) {
           avg_normal += current_cluster_surfel_ptr->contained_surfel->normal();
        }

        avg_normal /= input_surfels.size();

        if( scm::math::length(avg_normal) != 0.0) {
            avg_normal = scm::math::normalize(avg_normal);
        }
    }
       

    return avg_normal;
}


void reduction_k_clustering:: //functionality taken from entropy reduction strategy
assign_locally_overlapping_neighbours(shared_cluster_surfel current_surfel_ptr,
                                      shared_cluster_surfel_vector& input_surfel_ptr_array) const{

    shared_cluster_surfel_vector neighbours_found;
    shared_surfel target_surfel = current_surfel_ptr->contained_surfel;

    for(auto const& input_sufrel_ptr : input_surfel_ptr_array){

        // avoid overlaps with the surfel itself
        if (current_surfel_ptr->surfel_id != input_sufrel_ptr->surfel_id ||
            current_surfel_ptr->node_id   != input_sufrel_ptr->node_id) {

            shared_surfel contained_surfel_ptr = input_sufrel_ptr->contained_surfel;

            if( surfel::intersect(*target_surfel, *contained_surfel_ptr) ) {
                neighbours_found.push_back( input_sufrel_ptr );
            } 

            /*if( scm::math::length(target_surfel->pos() - contained_surfel_ptr->pos()) <= (target_surfel->radius() + contained_surfel_ptr->radius()) ) {
                neighbours_found.push_back( input_sufrel_ptr );
            }*/
        }
    }

    current_surfel_ptr->neighbours.insert(current_surfel_ptr->neighbours.end(),
                                        neighbours_found.begin(), neighbours_found.end());

}

void reduction_k_clustering::  
compute_overlap(shared_cluster_surfel current_surfel_ptr, bool look_in_M) const {
    //use only neighbours belonging to either set M or set S-M in order to compute overlap

    real overlap = 0;
    shared_surfel target_neighbour_surfel;
    real distance = 0.0;

    for(auto const& current_neighbour : current_surfel_ptr->neighbours){
        if (current_neighbour->cluster_seed == look_in_M){
        target_neighbour_surfel = current_neighbour->contained_surfel;
        distance = compute_distance(current_surfel_ptr, current_neighbour);
        overlap += ((target_neighbour_surfel->radius() + current_surfel_ptr->contained_surfel->radius()) - distance);
        }
        
    }

    current_surfel_ptr->overlap = overlap;
}

real reduction_k_clustering::
compute_distance(shared_cluster_surfel first_surfel_ptr, shared_cluster_surfel second_surfel_ptr) const {
    return scm::math::length(first_surfel_ptr->contained_surfel->pos() - second_surfel_ptr->contained_surfel->pos()); //
}


// compute deviation to all neighbours, independent on their set membership 
void reduction_k_clustering:: 
compute_deviation(shared_cluster_surfel current_surfel_ptr) const {

    real deviation = 0;
    shared_surfel target_neighbour_surfel;


    for(auto const& current_neighbour : current_surfel_ptr->neighbours){
        target_neighbour_surfel = current_neighbour->contained_surfel;
        deviation += 1 - std::fabs( scm::math::dot(current_surfel_ptr->contained_surfel->normal(), target_neighbour_surfel->normal()));
    }

    current_surfel_ptr->deviation = deviation;
}

void reduction_k_clustering::
resolve_oversampling(shared_cluster_surfel_vector& surfel_ptr_set_m) const {
    std::sort(surfel_ptr_set_m.begin(), surfel_ptr_set_m.end(), max_overlap_order());
    real min_overlap = 0;
    shared_cluster_surfel_vector temp_neighbour_list;

    //maxial overlap computed
    shared_cluster_surfel m_member =  surfel_ptr_set_m.back();
    
    while( !surfel_ptr_set_m.empty() ) {

        shared_cluster_surfel m_member =  surfel_ptr_set_m.back();

        if(m_member->overlap > min_overlap){
            temp_neighbour_list = surfel_ptr_set_m.back()->neighbours;

            update_cluster_membership(m_member, false);
            surfel_ptr_set_m.pop_back();
            for(auto const& current_temp_neighbour : temp_neighbour_list){
                compute_overlap(current_temp_neighbour, true);
            }
            std::sort(surfel_ptr_set_m.begin(), surfel_ptr_set_m.end(), max_overlap_order());
        } else {
            break;
        }
    }
    

}

void reduction_k_clustering::
resolve_undersampling(shared_cluster_surfel_vector& input_surfel_ptr_array) const{


    shared_cluster_surfel_vector new_set_m;

    for(auto const& current_surfel_ptr : input_surfel_ptr_array){
        shared_cluster_surfel_vector neighbours = current_surfel_ptr->neighbours;

        bool member_neighbours = false; 
        if(current_surfel_ptr->cluster_seed == false){
            for(auto const& current_neighbour : neighbours){
                if(current_neighbour->cluster_seed == true){
                     member_neighbours = true;
                    /*if(current_neighbour->node_id != current_surfel_ptr->node_id){
                        member_neighbours = true;
                    }
                    else if (current_neighbour->surfel_id != current_surfel_ptr->surfel_id){
                         member_neighbours = true;
                    }*/
                }
                    
                }

        }  

        if(!member_neighbours){
            new_set_m.push_back(current_surfel_ptr);
            update_cluster_membership(current_surfel_ptr, true);
        }
    }

    input_surfel_ptr_array.insert(input_surfel_ptr_array.end(), new_set_m.begin(), new_set_m.end());    
}

void reduction_k_clustering::
update_cluster_membership(shared_cluster_surfel current_surfel_ptr,
                          bool is_member) const {

    current_surfel_ptr->cluster_seed = is_member;

}


/*void reduction_k_clustering::
assign_locally_nearest_neighbours(shared_cluster_surfel current_surfel_ptr,
                                 const uint32_t number_of_neighbours,
                                 shared_cluster_surfel_vector& input_surfel_ptr_array){

    std::vector<std::pair<shared_cluster_surfel, real>> candidates;
    real max_candidate_distance = std::numeric_limits<real>::infinity();

} */

void reduction_k_clustering::
remove_surfel(shared_cluster_surfel_vector& surfel_ptr_set_M) const {
    std::sort(surfel_ptr_set_M.begin(), surfel_ptr_set_M.end(),  min_deviation_order());
    surfel_ptr_set_M.pop_back();
}

void reduction_k_clustering:: 
add_surfel(shared_cluster_surfel_vector& surfel_ptr_set_M,
           shared_cluster_surfel_vector& total_surfel_set) const {

    
    shared_cluster_surfel_vector complement_set;
   // bool has_neighbours_in_M = false; 

                                //test if all surfels in set M are already memberes of set M
                                uint counter_M = 0; 
                                for (auto& current_M : surfel_ptr_set_M){

                                    if (current_M->cluster_seed == false) {
                                        //std::cout << "ERROR: false membership in M"; 
                                    }
                                    else{
                                        counter_M +=1;
                                    }
                                }

                                uint counter_not_M = 0;  

                                 for (auto& current: total_surfel_set){

                                    if (current->cluster_seed == false)
                                       counter_not_M += 1; 
                                }

       

                                
        for (auto& current_surfel_ptr : total_surfel_set){
            if(current_surfel_ptr->cluster_seed == false){
                 complement_set.push_back(current_surfel_ptr);
            }
        }

        for (auto& complement_set_current_cluster_surfel : complement_set){

            compute_overlap(complement_set_current_cluster_surfel, true);
        }    
         

        shared_cluster_surfel cluster_surfel_to_add; 

        std::sort(complement_set.begin(), 
                  complement_set.end(), 
                  min_overlap_order() );

        
        cluster_surfel_to_add = complement_set.back();
        surfel_ptr_set_M.push_back(complement_set.back());
        update_cluster_membership(cluster_surfel_to_add, true);
        complement_set.pop_back();    

}

void reduction_k_clustering::
merge(shared_cluster_surfel_vector& final_cluster_surfel_set) const
{   
    vec3r avg_position = vec3r(0.0, 0.0, 0.0);    
    vec3r avg_color = vec3r(0.0, 0.0, 0.0);

    shared_cluster_surfel_vector current_temp_neighbour_vector;
    for(auto& current_surfel_ptr : final_cluster_surfel_set){
        avg_position = current_surfel_ptr->contained_surfel->pos();
        avg_color = current_surfel_ptr->contained_surfel->color();
        current_temp_neighbour_vector = current_surfel_ptr->neighbours;

        for(auto& current_neighbour : current_temp_neighbour_vector){
            avg_position += current_neighbour->contained_surfel->pos();
            avg_color += current_neighbour->contained_surfel->color();
          
        }
        avg_position /= (current_temp_neighbour_vector.size() + 1 );
        avg_color /= (current_temp_neighbour_vector.size() + 1 );
        current_surfel_ptr->contained_surfel->pos() = avg_position;
        current_surfel_ptr->contained_surfel->color() = vec3b(avg_color[0], avg_color[1], avg_color[2]) ;
    }
} 


surfel_mem_array reduction_k_clustering::
create_lod(real& reduction_error,
           const std::vector<surfel_mem_array*>& input,
           const uint32_t surfels_per_node,
           const bvh& tree,
           const size_t start_node_id) const {

    //create output array
    surfel_mem_array mem_array(std::make_shared<surfel_vector>(surfel_vector()), 0, 0);


    //container for all input surfels including [total set S]
    shared_cluster_surfel_vector cluster_surfel_array;


    //cluster set M
    shared_cluster_surfel_vector cluster_surfel_output_array;

    
    uint32_t radius_discarded_surfels = 0;
    // wrap all surfels of the input array to cluster_surfels
    for (size_t node_id = 0; node_id < input.size(); ++node_id) {
        for (size_t surfel_id = input[node_id]->offset();
                    surfel_id < input[node_id]->offset() + input[node_id]->length();
                    ++surfel_id){

            //this surfel will be referenced in the cluster surfel
            auto current_surfel = input[node_id]->mem_data()->at(input[node_id]->offset() + surfel_id);

            // ignore outlier radii of any kind
            if (current_surfel.radius() == 0.0) {
                ++radius_discarded_surfels;
                continue;
            }
            //create new cluster surfel
             

        // only place where shared pointers should be created
        cluster_surfel_array.push_back( std::make_shared<cluster_surfel>( cluster_surfel(current_surfel, surfel_id, node_id)  ) );
       }
    }

   
    for (auto target_surfel : cluster_surfel_array ){

        assign_locally_overlapping_neighbours(target_surfel, cluster_surfel_array);
        compute_overlap(target_surfel, false);
        compute_deviation(target_surfel);
    }

     //average nomal
    vec3f avg_normal = compute_avg_normal(cluster_surfel_array);

    cluster_surfel_output_array = get_initial_cluster_seeds (avg_normal, cluster_surfel_array);

    for (auto target_surfel : cluster_surfel_output_array ){    
        compute_overlap(target_surfel, true);       
    }
    
    resolve_oversampling(cluster_surfel_output_array);   
    resolve_undersampling(cluster_surfel_output_array);

    
    //make sure desired number of output surfels is reached
     while(cluster_surfel_output_array.size() > surfels_per_node) {
        remove_surfel(cluster_surfel_output_array);
    }

    while(cluster_surfel_output_array.size() < surfels_per_node) {
        add_surfel (cluster_surfel_output_array, cluster_surfel_array);
    }
    

    merge(cluster_surfel_output_array);
    
  
    //write surfels for output
    for (auto const& final_cluster_surfel : cluster_surfel_output_array) {

        mem_array.mem_data()->push_back(*(final_cluster_surfel->contained_surfel) );
        
    }

    mem_array.set_length(mem_array.mem_data()->size());  

    return mem_array;
}

} // namespace pre
} // namespace lamure
