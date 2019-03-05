// Copyright (c) 2014-2018 Bauhaus-Universitaet Weimar
// This Software is distributed under the Modified BSD License, see license.txt.
//
// Virtual Reality and Visualization Research Group 
// Faculty of Media, Bauhaus-Universitaet Weimar
// http://www.uni-weimar.de/medien/vr

#include <lamure/pre/io/format_bin.h>

#include <stdexcept>

namespace lamure
{
namespace pre
{

void format_bin::
read(const std::string &filename, surfel_callback_funtion callback)
{
    throw std::runtime_error("Not implemented yet!");
}

void format_bin::
write(const std::string &filename, buffer_callback_function callback)
{

    surfel_file file;
    surfel_vector buffer;
    size_t count = 0;

    file.open(filename, true);
    while (true) {
        bool ret = callback(buffer);
        if (!ret)
            break;

        file.append(&buffer);
        count += buffer.size();
    }
    file.close();
    LOGGER_TRACE("Output surfels: " << count);
}

}
}

