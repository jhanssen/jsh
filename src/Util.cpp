#include "Util.h"
#include <pwd.h>
#include <stdlib.h>

namespace util {

Path homeDirectory()
{
    if (char* homeEnv = getenv("HOME"))
        return Path(homeEnv);

    struct passwd pwent;
    struct passwd* pwentp;
    char buf[8192];

    if (getpwuid_r(getuid(), &pwent, buf, sizeof(buf), &pwentp))
        return Path();
    return Path(pwent.pw_dir);
}

Path homeify(const String& path)
{
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
