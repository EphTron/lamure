// Copyright (c) 2014-2018 Bauhaus-Universitaet Weimar
// This Software is distributed under the Modified BSD License, see license.txt.
//
// Virtual Reality and Visualization Research Group 
// Faculty of Media, Bauhaus-Universitaet Weimar
// http://www.uni-weimar.de/medien/vr

#include <lamure/types.h>
#include <lamure/prov/auxi.h>
#include <lamure/prov/octree.h>

#include <scm/core/math.h>
#include <scm/gl_core/math.h>

#include <iostream>
#include <string>
#include <map>

char *get_cmd_option(char **begin, char **end, const std::string &option) {
    char **it = find(begin, end, option);
    if(it != end && ++it != end)
        return *it;
    return 0;
}

bool cmd_option_exists(char **begin, char **end, const std::string &option) { 
    return find(begin, end, option) != end; 
}

bool check_file_extensions(std::string name_file, const char *pext) {
    std::string ext(pext);
    if (name_file.substr(name_file.size() - ext.size()).compare(ext) != 0) {
        std::cout << "Please specify " + ext + " file as input" << std::endl;
        return true;
    }
    return false;
}

template <class InIt, class OutIt>
void split(InIt begin, InIt end, OutIt splits) {
  InIt current = begin;
  while (begin != end) {
    if (*begin == ',') {
      *splits++ = std::string(current,begin);
      current = ++begin;
    }
    else
      ++begin;
  }
  *splits++ = std::string(current,begin);
}

int main(int argc, char *argv[]) {
    if (argc == 1 || 
      cmd_option_exists(argv, argv+argc, "-h") ||
      !cmd_option_exists(argv, argv+argc, "-f") ||
      !cmd_option_exists(argv, argv+argc, "-l") ||
      !cmd_option_exists(argv, argv+argc, "-o")) {
      std::cout << "Usage: " << argv[0] << std::endl <<
         "INFO: aux_import " << std::endl <<
         "\t-f input .auxi file (required)" << std::endl <<
         "\t-l input .log file from last_atlas_compiler (required)" << std::endl <<
         "\t-o output .auxi file (required)" << std::endl <<
         std::endl;
      return 0;
    }

    std::string aux_file_in = std::string(get_cmd_option(argv, argv + argc, "-f"));
    std::string log_file_in = std::string(get_cmd_option(argv, argv + argc, "-l"));
    std::string aux_file_out = std::string(get_cmd_option(argv, argv + argc, "-o"));

    lamure::prov::auxi aux_in(aux_file_in);
    lamure::prov::auxi aux_out;

    uint32_t num_views = aux_in.get_num_views();
    std::cout << aux_in.get_num_views() << " views, " << aux_in.get_num_sparse_points() << " points" << std::endl;

    if (aux_in.get_num_atlas_tiles() != 0) {
      std::cout << "Warning: the input file already contains " << aux_in.get_num_atlas_tiles() << " atlas tiles. These will be overriden now!" << std::endl;
    }

    uint32_t num_atlas_tiles = aux_in.get_num_atlas_tiles();

    struct tile {
      uint32_t x_;
      uint32_t y_;
      uint32_t width_;
      uint32_t height_;
      uint32_t rotated_;
    };
    std::map<std::string, tile> tile_map;


    std::cout << "loading atlas log file..." << std::endl;
    std::ifstream log_file(log_file_in.c_str());

    if (!log_file.is_open()) {
      std::cout << "could not open log file" << std::endl;
      exit(-1);
    }

    uint32_t atlas_width = 0;
    uint32_t atlas_height = 0;
    uint32_t rotated = 0;
  
    std::string line;
    uint32_t line_no = 0;
    while(std::getline(log_file, line)) {
      if(line.length() >= 2) {
        if (line[0] == '#') {
          continue;
        }
        std::vector<std::string> values;
        split(line.begin(), line.end(), std::back_inserter(values));
        if (line_no > 0) {        
          tile t{
            atoi(values[1].c_str()), 
            atoi(values[2].c_str()), 
            atoi(values[3].c_str()), 
            atoi(values[4].c_str()), 
            atoi(values[5].c_str())};
          rotated = t.rotated_;
          tile_map[values[values.size()-1]] = t;
        }
        else {
          atlas_width = atoi(values[0].c_str());
          atlas_height = atoi(values[1].c_str());
        }

      }
      ++line_no;
    }

    log_file.close();

    std::cout << "muxing atlas..." << std::endl;
    std::cout << "atlas width: " << atlas_width << std::endl;
    std::cout << "atlas height: " << atlas_height << std::endl;


    lamure::prov::auxi::atlas atlas{aux_in.get_num_views(), atlas_width, atlas_height, rotated};

    aux_out.set_atlas(atlas);

    for (uint32_t i = 0; i < aux_in.get_num_views(); ++i) {
      auto view = aux_in.get_view(i);
      auto it = tile_map.find(view.image_file_);
      if (it == tile_map.end()) {
        std::cout << "Error: tile_map does not contain image file " << view.image_file_ << std::endl;
        exit(1);
      }
      lamure::prov::auxi::atlas_tile atlas_tile{i, it->second.x_, it->second.y_, it->second.width_, it->second.height_};
      aux_out.add_atlas_tile(atlas_tile);
      view.atlas_tile_id_ = i;
      aux_out.add_view(view);

    }

    std::cout << "copy sparse points..." << std::endl;
    for (uint32_t i = 0; i < aux_in.get_num_sparse_points(); ++i) {
      aux_out.add_sparse_point(aux_in.get_sparse_point(i));
    }

    std::cout << "copy octree..." << std::endl;
    aux_out.set_octree(aux_in.get_octree());
    
    std::cout << "writing auxi file..." << std::endl;
    aux_out.write_aux_file(aux_file_out);

    
  

    return 0;
}
