/* pbo.c - (C) 2015, Emir Marincic
 * libpbo - A library to work with Arma PBO files
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "pbo.h"

#define MAXNAMELEN 512

enum{
    PACKING_METHOD = 0,
    ORIGINAL_SIZE,
    RES,
    TIME_STAMP,
    DATA_SIZE,
};

struct header_extension {
    size_t len;
    char **entries;
};

struct pbo_entry {
    char *name;
    uint32_t properties[5];
    struct header_extension *ext;
    size_t file_offset;
};

struct list_entry {
    struct list_entry *next;
    struct pbo_entry *data;
};

struct pbo {
    size_t headersz;
    struct list_entry *root;
    struct list_entry *last;
    const char *filename;
};

static struct list_entry *pbo_find_file(pbo_t d, const char *file);
static int pbo_util_getdelim(unsigned char *dst, FILE *src, size_t dstsz, char delim);
static char *pbo_util_strdup(const char *src);
static void pbo_list_add_entry(pbo_t d, struct pbo_entry *pe);
static void pbo_add_header_extension(struct header_extension *he, const char *e);

pbo_t pbo_init(const char *filename)
{
    struct pbo *d = malloc(sizeof *d);
    if(!d)
        return NULL; //Malloc Error
    d->root = NULL;
    d->last = NULL;
    d->filename = filename;
    return d;
}

void pbo_clear(pbo_t d)
{
    struct list_entry *e = d->root;
    while(e) {
        struct list_entry *t = e->next;
        free(e->data->name);
        if(e->data->ext) {
            free(e->data->ext->entries);
            free(e->data->ext);
        }
        free(e->data);
        free(e);
        e = t;
    }
    d->root = NULL;
}

void pbo_dispose(pbo_t d)
{
    pbo_clear(d);\
    free(d);
}

void pbo_set_filename(pbo_t d, const char *filename)
{
    if(!d || !filename)
        return;

    d->filename = filename;
}

void pbo_read_header(pbo_t d)
{
    FILE *file = fopen(d->filename, "r");
    if(!file)
        return; //I/O Error

    char buf[MAXNAMELEN];
    size_t file_offset = 0;
    for(int i = 0; 1; i++) {
        int sz = pbo_util_getdelim(buf, file, sizeof buf, '\0');
        if(sz < 0)
            return; //Broken Pbo header
        struct pbo_entry *pe = malloc(sizeof *pe);
        pe->name = pbo_util_strdup(buf);
        fread(pe->properties, 4, 5, file); //Get all properties
        pe->file_offset = file_offset;
        file_offset += pe->properties[DATA_SIZE];
        if(!sz && !i) { //Header Extension
            pe->ext = malloc(sizeof *pe->ext);
            if(!pe->ext)
                return; //Malloc Error
            while(pbo_util_getdelim(buf, file, sizeof buf, '\0') > 0)
                pbo_add_header_extension(pe->ext, buf);
            pbo_add_header_extension(pe->ext, "\0");
        }
        pbo_list_add_entry(d, pe);
        if(!sz && i)
            break;
    }
    d->headersz = ftell(file);
    fclose(file);
}

void pbo_dump_header(pbo_t d)
{
    int i = 0;
    for(struct list_entry *e = d->root; e; e = e->next) {
        struct pbo_entry *pe = e->data;
        printf("Entry(%d): %s\n", i, pe->name);
        for(int i = 0; i <= DATA_SIZE; i++)
            printf("\tproperties[%d] = %d\n", i, pe->properties[i]);
        if(pe->ext) {
            printf("\tHeaderExtension:\n");
            for(int i = 0; i < pe->ext->len; i++)
                printf("\t\tHEntry: %s\n", pe->ext->entries[i]);
        }
        i++;
    }
}

size_t pbo_read_file(pbo_t d, const char *filename, void *buf, size_t size)
{
    struct list_entry *e = pbo_find_file(d, filename);
    if(!e)
        return 0; //Doesn't exist

    if(e->data->properties[DATA_SIZE] > size)
        return 0; //Doesn't fit

    FILE *file = fopen(d->filename, "r");
    if(!file)
        return 0; //I/O Error

    fseek(file, e->data->file_offset + d->headersz, SEEK_SET);
    size_t sz = fread(buf, 1, e->data->properties[DATA_SIZE], file);
    fclose(file);
    return sz;
}

void pbo_get_file_list(pbo_t d, pbo_listcb cb, void *user)
{
    if(!d)
        return;

    for(struct list_entry *e = d->root; e; e = e->next)
        cb(e->data->name, user);
}

size_t pbo_get_file_size(pbo_t d, const char *filename)
{
    struct list_entry *e = pbo_find_file(d, filename);
    if(!e)
        return 0; //Doesn't Exist

    return e->data->properties[DATA_SIZE];
}

void pbo_write_to_file(pbo_t d, const char *filename, FILE *file)
{
    struct list_entry *le = pbo_find_file(d, filename);
    if(!le)
        return; //Doesn't exist

    FILE *f = fopen(d->filename, "r");
    if(!f)
        return; //Doesn't exist

    fseek(f, le->data->file_offset + d->headersz, SEEK_SET);
    size_t sz = le->data->properties[DATA_SIZE];
    char buf[sz];
    fread(buf, 1, sz, f);
    fwrite(buf, 1, sz, file);
    fclose(f);
}

static void pbo_list_add_entry(pbo_t d, struct pbo_entry *pe)
{
    struct list_entry *le = malloc(sizeof *le);
    le->data = pe;

    if(!d->root)
        d->root = le;
    else
        d->last->next = le;
    d->last = le;
}

static void pbo_add_header_extension(struct header_extension *he, const char *e)
{
    if(he->len % 4 == 0) {
        char **new = realloc(he->entries, (he->len + 4) * sizeof *new);
        if(!new)
            return; //Malloc Error
        he->entries = new;
    }
    he->entries[he->len++] = pbo_util_strdup(e);
}

static struct list_entry *pbo_find_file(pbo_t d, const char *file)
{
    for(struct list_entry *e = d->root; e; e = e->next)
        if(!strcmp(e->data->name, file))
            return e;
    return NULL;
}

static int pbo_util_getdelim(unsigned char *dst, FILE *src, size_t dstsz, char delim)
{
    if(!dstsz || !dst)
        return -1;

    char c = 0;
    int sz = 0;

    while(dstsz && (c = fgetc(src)) != EOF && c != delim)
        sz++, dstsz--, *dst++ = c;

    if(dstsz && !feof(src))
        *dst = delim;
    else
        sz = -1;
    return sz;
}

static char *pbo_util_strdup(const char *src)
{
    size_t len = strlen(src) + 1;
    char *new = malloc(len);

    if(!new)
        return NULL;
    return (char *) memcpy(new, src, len);
}