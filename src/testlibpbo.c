/* testlibpbo.c - (C) 2015, Emir Marincic
 * libpbo - A library to work with Arma PBO files
 * See README for contact-, COPYING for license information. */
 
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <libpbo/pbo.h>

void create_directories(char *path)
{    
    for(int i = 0; path[i] != '\0'; i++) {
        if(path[i] == '/') {
            path[i] = '\0';
            mkdir(path, 0755);
            path[i] = '/';
        }
    }
}

void list_callback(const char *filename, void *user)
{
    if(filename == '\0')
        return;

    pbo_t d = (pbo_t)user;

    char buf[512];
    strcpy(buf, filename);

    for(int i = 0; buf[i] != '\0'; i++)
        if(buf[i] == '\\')
            buf[i] = '/';

    create_directories(buf);

    FILE *file = fopen(buf, "w");
    if(!file)
        return;
    pbo_write_to_file(d, filename, file);
    fclose(file);
}

int main(void)
{
    pbo_t d = pbo_init("test.pbo");
    pbo_read_header(d);
    pbo_dump_header(d);
    pbo_get_file_list(d, list_callback, d);
    pbo_clear(d);
    pbo_set_filename(d, "kek.pbo");
    pbo_init_new(d);
    pbo_add_file_p(d, "test.txt", "test.txt");
    pbo_write(d);
    pbo_dispose(d);
    return 0;
}