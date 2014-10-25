#include "../../nhttp/nhttp.h"

import(nbbs);

char* getconfig(GETPAGE **getpage, char *path, char *host)
{
    *getpage = nbbs_getpage;
    return path;
}
