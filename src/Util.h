#ifndef UTIL_H
#define UTIL_H

#include <rct/Path.h>
#include <string>
#include <wchar.h>

namespace util {
Path homeify(const String& path);
Path findFile(const String& path, const String filename);
Path homeDirectory(const String& user = String());
String wcharToUtf8(const wchar_t* string, ssize_t size = -1);
String wcharToUtf8(const std::wstring& string);
std::wstring utf8ToWChar(const char* data, ssize_t size = -1);
std::wstring utf8ToWChar(const String& data);
}

#endif
