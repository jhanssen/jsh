#ifndef UTIL_H
#define UTIL_H

#include <rct/Path.h>

namespace util {
Path findFile(const String& path, const String filename);
Path homeDirectory();
}

#endif
