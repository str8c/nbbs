#include <string.h>

char* getlibname(char *dest, char *path, char *host)
{
    if(!memcmp(path, "talk/", 5)) {
        strcpy(dest, "./nbbs.so");
        return path + 5;
    } else {
        strcpy(dest, "./static.so");
        return path;
    }
}
