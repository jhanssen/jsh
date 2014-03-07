#ifndef UTIL_H
#define UTIL_H

#include <rct/Path.h>

namespace util {
Path homeify(const String& path);
Path findFile(const String& path, const String filename);
Path homeDirectory(const String& user = String());
}

#endif
