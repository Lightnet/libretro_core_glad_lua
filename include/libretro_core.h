#ifndef LIBRETRO_CORE_H
#define LIBRETRO_CORE_H

#include <stdbool.h>

// Extract asset from zip file
bool extract_asset_from_zip(const char *asset_name, char **asset_data, size_t *asset_size);

#endif // LIBRETRO_CORE_H