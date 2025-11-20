#include "../../include/directory.h"
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>

bool is_directory(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

void list_directory(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        perror("[list_directory] Error al abrir carpeta");
        return;
    }

    struct dirent *e;
    while ((e = readdir(dir)) != NULL) {
        printf(" - %s\n", e->d_name);
    }

    closedir(dir);
}