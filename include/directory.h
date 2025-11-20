#ifndef DIRECTORY_H
#define DIRECTORY_H

#include <stdbool.h>

bool is_directory(const char *path);
void list_directory(const char *path);

#endif