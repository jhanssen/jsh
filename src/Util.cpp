#include "Util.h"
#include <pwd.h>
#include <stdlib.h>

namespace util {

Path homeDirectory(const String& user)
{
    if (char* homeEnv = getenv("HOME"))
        return Path(homeEnv);

    struct passwd pwent;
    struct passwd* pwentp;
    int ret;
    char buf[8192];

    if (user.isEmpty())
        ret = getpwuid_r(getuid(), &pwent, buf, sizeof(buf), &pwentp);
    else
        ret = getpwnam_r(user.constData(), &pwent, buf, sizeof(buf), &pwentp);
    if (ret)
        return Path();
    return Path(pwent.pw_dir);
}

Path homeify(const String& path)
{
    Path copy = path;
    int idx = 0, slash;
    for (;;) {
        idx = copy.indexOf('~', idx);
        if (idx == -1)
            return copy;
        slash = copy.indexOf('/', idx + 1);
        if (slash == -1)
            slash = copy.size();
        copy.replace(idx, slash - idx, homeDirectory(copy.mid(idx + 1, slash - idx - 1)));
        idx += slash - idx;
    }
    return copy;
}

Path findFile(const String& path, const String filename)
{
    const List<String> candidates = path.split(':');
    for (const String& cand : candidates) {
        Path pcand = homeify(path + "/" + filename);
        if (pcand.exists())
            return pcand;
    }
    return Path();
}

}
