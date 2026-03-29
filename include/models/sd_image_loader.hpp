#pragma once
#include "global.hpp"
#include "models/image.hpp"

class SDImageLoader {
  public:
    static bool load_jpeg(const char * filename, Image::ImageData & image_data, Dim max_dim);
  private:
    static constexpr char const * TAG = "SDImageLoader";
};
