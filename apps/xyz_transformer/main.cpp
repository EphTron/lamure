// Copyright (c) 2014-2018 Bauhaus-Universitaet Weimar
// This Software is distributed under the Modified BSD License, see license.txt.
//
// Virtual Reality and Visualization Research Group 
// Faculty of Media, Bauhaus-Universitaet Weimar
// http://www.uni-weimar.de/medien/vr

#include <lamure/types.h>
#include <lamure/pre/surfel.h>

#include <sstream>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <string>

#include <memory>

#include <scm/gl_core/math.h>
#include <scm/core/math.h>

char* get_cmd_option(char** begin, char** end, const std::string& option) {
  char** it = std::find(begin, end, option);
  if (it != end && ++it != end) {
    return *it;
  }
  return 0;
}

bool cmd_option_exists(char** begin, char** end, const std::string& option) {
  return std::find(begin, end, option) != end;
}

int32_t main(int argc, char* argv[]) {

  std::string xyz_file = "";
  
  bool terminate = false;
  
  if (cmd_option_exists(argv, argv+argc, "-f")) {
    xyz_file = get_cmd_option(argv, argv+argc, "-f");
  }
  else {
    terminate = true;
  }
  
  if (terminate) {
    std::cout << "Usage: " << argv[0] << "<flags>\n" <<
      "INFO: " << argv[0] << "\n" <<
      "\t-f: select xyz / xyz_all file\n" << 
      "\t-x: integer x-transform (default: 0.f)\n" <<
      "\t-y: integer y-transform (default: 0.f)\n" <<
      "\t-z: integer z-transform (default: 0.f)\n" <<
      "\n";
    return 0;
  }

  float x = 0.f;
  float y = 0.f;
  float z = 0.f;
  if (cmd_option_exists(argv, argv+argc, "-x")) x = atof(get_cmd_option(argv, argv+argc, "-x"));
  if (cmd_option_exists(argv, argv+argc, "-y")) y = atof(get_cmd_option(argv, argv+argc, "-y"));
  if (cmd_option_exists(argv, argv+argc, "-z")) z = atof(get_cmd_option(argv, argv+argc, "-z"));

  bool xyz_all = false;
  std::string out_filename = xyz_file.substr(0, xyz_file.size()-4) + "_TRF.xyz";

  if (xyz_file.substr(xyz_file.size()-7).compare("xyz_all") == 0) {
    out_filename = xyz_file.substr(0, xyz_file.size()-8) + "_TRF.xyz_all";
    xyz_all = true;
    std::cout << "(^___^)" << std::endl;
  }
  else if (xyz_file.substr(xyz_file.size()-3).compare("xyz") == 0) {
    std::cout << "(^_^)" << std::endl;
  }
  else {
    std::cout << "Unsupported input. (>__<)" << std::endl; exit(0);
  }

  std::ifstream xyz_file_stream(xyz_file);

  if (!xyz_file_stream.is_open())
      throw std::runtime_error("Unable to open input file: " + xyz_file);

  std::string line;

  lamure::real pos[3];
  float norm[3];
  unsigned int color[3];
  lamure::real radius;

  xyz_file_stream.seekg(0, std::ios::end);
  std::streampos end_pos = xyz_file_stream.tellg();
  xyz_file_stream.seekg(0, std::ios::beg);
  uint8_t percent_processed = 0;

  std::vector<lamure::pre::surfel> surfels;

  while (getline(xyz_file_stream, line)) {

    uint8_t new_percent_processed = (xyz_file_stream.tellg() / float(end_pos)) * 100;
    if (percent_processed + 1 == new_percent_processed) {
        percent_processed = new_percent_processed;
        std::cout << "\r" << "input: " << (int) percent_processed << "% processed" << std::flush;
    }

    std::stringstream sstream;

    sstream << line;

    sstream >> std::setprecision(LAMURE_STREAM_PRECISION) >> pos[0];
    sstream >> std::setprecision(LAMURE_STREAM_PRECISION) >> pos[1];
    sstream >> std::setprecision(LAMURE_STREAM_PRECISION) >> pos[2];
    if (xyz_all) {
      sstream >> std::setprecision(LAMURE_STREAM_PRECISION) >> norm[0];
      sstream >> std::setprecision(LAMURE_STREAM_PRECISION) >> norm[1];
      sstream >> std::setprecision(LAMURE_STREAM_PRECISION) >> norm[2];
    }
    sstream >> color[0];
    sstream >> color[1];
    sstream >> color[2];
    if (xyz_all) {
      sstream >> std::setprecision(LAMURE_STREAM_PRECISION) >> radius;
    }

    surfels.push_back(lamure::pre::surfel(lamure::vec3r(pos[0], pos[1], pos[2]),
                    lamure::vec3b(color[0], color[1], color[2]),
                    radius,
                    lamure::vec3f(norm[0], norm[1], norm[2])));
  }

  xyz_file_stream.close();

  std::ofstream out_file_stream(out_filename);

  percent_processed = 0;

  if (!out_file_stream.is_open())
      throw std::runtime_error("Unable to open file: " + out_filename);

  //scm::math::mat4f mat = scm::math::make_rotation(-90.f, scm::math::vec3f(1.f, 0.f, 0.f));

  for (const auto s: surfels) {

    uint8_t new_percent_processed = (out_file_stream.tellp() / float(end_pos)) * 100;
    if (percent_processed + 1 == new_percent_processed) {
        percent_processed = new_percent_processed;
        std::cout << "\r" << "output: " << (int) percent_processed << "% processed" << std::flush;
    }

    //auto s_pos = scm::math::vec4f(s.pos().x, s.pos().y, s.pos().z, 1.f) * mat;

      out_file_stream << std::setprecision(LAMURE_STREAM_PRECISION) 
        << s.pos().x + x << " " 
        << s.pos().y + y << " " 
        << s.pos().z + z << " ";
      if (xyz_all) {
        out_file_stream
          << s.normal().x << " "
          << s.normal().y << " "
          << s.normal().z << " ";
      }
      out_file_stream
        << int(s.color().r) << " " 
        << int(s.color().g) << " " 
        << int(s.color().b);
      if (xyz_all) {
        out_file_stream << " " << radius;
      }
      out_file_stream << "\r\n";
  }

  out_file_stream.close();

  std::cout << surfels.size() << " points written to " << out_filename << std::endl;
  surfels.clear();

  return 0;
}




