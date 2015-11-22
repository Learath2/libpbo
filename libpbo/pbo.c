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
#include <time.h>

#include "sha.h"

#include <libpbo/pbo.h>

#define MAXNAMELEN 512

#define WRITE_N_SHA(P,S,N,F,C) \
    fwrite((P), (S), (N), (F)); \
    SHA1Input((C), (uint8_t *)(P), (S) * (N));

typedef enum
{
    CLEAR = 0,
    EXISTING,
    NEW,
} pbo_state;

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
    unsigned char *data;
};

struct list_entry {
    struct list_entry *next;
    struct pbo_entry *data;
};

struct pbo {
    size_t headersz;
    struct list_entry *root;
    struct list_entry *last;
    char *filename;
    pbo_state state;
};

static void pbo_add_header_extension(struct header_extension *he, const char *e);
static void pbo_list_add_entry(pbo_t d, struct pbo_entry *pe);
static void pbo_clear_list(pbo_t d);
static struct list_entry *pbo_find_file(pbo_t d, const char *file);
static int pbo_util_getdelim(unsigned char *dst, FILE *src, size_t dstsz, char delim);
static char *pbo_util_strdup(const char *src);

pbo_t pbo_init(const char *filename)
{
    struct pbo *d = malloc(sizeof *d);
    if(!d)
        goto cleanup; //Malloc Error

    d->filename = pbo_util_strdup(filename);
    if(!d->filename)
        goto cleanup;

    d->root = NULL;
    d->last = NULL;    
    d->state = CLEAR;
    return d;

cleanup:
    free(d);
    return NULL;
}

void pbo_clear(pbo_t d)
{
    if(!d)
        return;

    pbo_clear_list(d);

    free(d->filename);
    d->filename = NULL;
    d->state = CLEAR;
}

void pbo_dispose(pbo_t d)
{
    if(!d)
        return;
    pbo_clear(d);
    free(d);
}

pbo_error pbo_set_filename(pbo_t d, const char *filename)
{
    if(!d || !filename)
        return PBO_ERROR_NEXIST;
    if(d->state != CLEAR)
        return PBO_ERROR_STATE;

    free(d->filename);
    d->filename = NULL;
    d->filename = pbo_util_strdup(filename);

    if(!d->filename)
        return PBO_ERROR_MALLOC;
    return PBO_SUCCESS;
}

pbo_error pbo_read_header(pbo_t d)
{
    if(!d)
        return PBO_ERROR_NEXIST;
    if(d->state != EXISTING)
        return PBO_ERROR_STATE;

    FILE *file = fopen(d->filename, "r");
    if(!file)
        return PBO_ERROR_IO; //I/O Error

    char buf[MAXNAMELEN];
    size_t file_offset = 0;

    struct pbo_entry *pe = NULL;
    for(int i = 0;; i++) {
        int sz = pbo_util_getdelim(buf, file, sizeof buf, '\0');
        if(sz < 0)
            return PBO_ERROR_BROKEN; //Broken Pbo header

        pe = NULL;
        pe = malloc(sizeof *pe);
        if(!pe)
            goto cleanup; //Malloc error

        pe->name = pbo_util_strdup(buf);
        if(!pe->name)
            goto cleanup;

        fread(pe->properties, 4, 5, file); //Get all properties
        pe->file_offset = file_offset;
        file_offset += pe->properties[DATA_SIZE];
        if(!sz && !i) { //Header Extension
            pe->ext = malloc(sizeof *pe->ext);
            if(!pe->ext)
                goto cleanup; //Malloc Error
            while(pbo_util_getdelim(buf, file, sizeof buf, '\0') > 0)
                pbo_add_header_extension(pe->ext, buf);
            pbo_add_header_extension(pe->ext, "\0");
        }
        pbo_list_add_entry(d, pe);
        if(!sz && i)
            break;
    }
    d->headersz = ftell(file);
    d->state = EXISTING;
    fclose(file);
    return PBO_SUCCESS;

cleanup:
    fclose(file);
    free(pe->name);
    free(pe);
    return PBO_ERROR_MALLOC;
}

pbo_error pbo_write(pbo_t d)
{
    if(!d)
        return PBO_ERROR_NEXIST;
    if(d->state != NEW)
        return PBO_ERROR_STATE;

    FILE *file = fopen(d->filename, "w");
    if(!file)
        return PBO_ERROR_IO;

    //Add the dummy entry to indicate end of header
    struct pbo_entry *pe = malloc(sizeof *pe);
    if(!pe)
        goto cleanup; //Malloc Error

    pe->name = "";
    pe->properties[PACKING_METHOD] = 0;
    pe->properties[ORIGINAL_SIZE] = 0;
    pe->properties[RES] = 0;
    pe->properties[TIME_STAMP] = 0;
    pe->properties[DATA_SIZE] = 0;
    pe->data = NULL;
    pbo_list_add_entry(d, pe);

    SHA1Context ctx;
    SHA1Reset(&ctx);

    //First write the header
    for(struct list_entry *e = d->root; e; e = e->next) {
        WRITE_N_SHA(e->data->name, 1, strlen(e->data->name) + 1, file, &ctx);
        WRITE_N_SHA(e->data->properties, 4, 5, file, &ctx);
        if(e->data->ext) {
            for(int i = 0; i < e->data->ext->len; i++) {
                WRITE_N_SHA(e->data->ext->entries[i], 1, strlen(e->data->ext->entries[i]) + 1, file, &ctx);
            }
        }
    }

    //Then write the data block
    for(struct list_entry *e = d->root; e; e = e->next){
        if(*e->data->name == '\0'){
            WRITE_N_SHA(e->data->data, 1, e->data->properties[DATA_SIZE], file, &ctx);
        }
    }

    //Finalize SHA and write it at the end
    uint8_t sha[SHA1HashSize];
    SHA1Result(&ctx, sha);
    fwrite(sha, 1, SHA1HashSize, file);

    fclose(file);
    return PBO_SUCCESS;

cleanup:
    return PBO_ERROR_MALLOC;
}

//TODO: Add some other way of reading files. Possibly a fread like API.
size_t pbo_read_file(pbo_t d, const char *filename, void *buf, size_t size)
{
    if(!d || !filename || d->state != EXISTING)
        return 0;

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

pbo_error pbo_init_new(pbo_t d)
{
    if(!d)
        return PBO_ERROR_NEXIST;
    if(d->state != CLEAR)
        return PBO_ERROR_STATE;

    //Add the dummy entry with the extension
    struct pbo_entry *pe = malloc(sizeof *pe);
    if(!pe)
        goto cleanup; //Malloc Error

    pe->name = "";
    pe->properties[PACKING_METHOD] = 0x56657273;
    pe->properties[ORIGINAL_SIZE] = 0;
    pe->properties[RES] = 0;
    pe->properties[TIME_STAMP] = 0;
    pe->properties[DATA_SIZE] = 0;

    pe->ext = malloc(sizeof *pe->ext);
    if(!pe->ext)
        goto cleanup; //Malloc Error

    pe->data = NULL;

    pbo_list_add_entry(d, pe);
    d->state = NEW;
    return PBO_SUCCESS;

cleanup:
    free(pe);
    return PBO_ERROR_MALLOC;
}

pbo_error pbo_add_extension(pbo_t d, const char *e)
{
    if(!d)
        return PBO_ERROR_NEXIST;
    if(d->state != NEW)
        return PBO_ERROR_STATE;

    pbo_add_header_extension(d->root->data->ext, e);
    return PBO_SUCCESS;
}

pbo_error pbo_add_file_d(pbo_t d, const char *name, void *data,  size_t size)
{
    if(!d)
        return PBO_ERROR_NEXIST;
    if(d->state != NEW)
        return PBO_ERROR_STATE;

    struct pbo_entry *pe = malloc(sizeof *pe);
    if(!pe)
        goto cleanup; //Malloc Error

    pe->name = pbo_util_strdup(name);
    if(!pe->name)
        goto cleanup; //Malloc Error

    pe->data = malloc(size);
    if(!pe->data)
        goto cleanup; //Malloc Error

    memcpy(pe->data, data, size);

    pe->properties[PACKING_METHOD] = 0;
    pe->properties[ORIGINAL_SIZE] = size;
    pe->properties[RES] = 0;
    pe->properties[TIME_STAMP] = (uint32_t)time(NULL);
    pe->properties[DATA_SIZE] = size;

    pbo_list_add_entry(d, pe);
    return PBO_SUCCESS;

cleanup:
    free(pe->name);
    free(pe);
    return PBO_ERROR_MALLOC;
}

pbo_error pbo_add_file_f(pbo_t d, const char *name, FILE *file)
{
    if(!d || !file)
        return PBO_ERROR_NEXIST;
    if(d->state != NEW)
        return PBO_ERROR_STATE;

    struct pbo_entry *pe = malloc(sizeof *pe);
    if(!pe)
        goto cleanup;

    pe->name = pbo_util_strdup(name);
    if(!pe->name)
        goto cleanup;

    //Get file size
    fseek(file, 0, SEEK_END);
    size_t filesz = ftell(file);
    rewind(file);

    pe->data = malloc(filesz);
    if(!pe->data)
        goto cleanup;

    fread(pe->data, 1, filesz, file);
    fclose(file);

    pe->properties[PACKING_METHOD] = 0;
    pe->properties[ORIGINAL_SIZE] = filesz;
    pe->properties[RES] = 0;
    pe->properties[TIME_STAMP] = (uint32_t)time(NULL);
    pe->properties[DATA_SIZE] = filesz;

    pbo_list_add_entry(d, pe);
    return PBO_SUCCESS;

cleanup:
    free(pe->name);
    free(pe);
    return PBO_ERROR_MALLOC;
}

pbo_error pbo_add_file_p(pbo_t d, const char *name, const char *path)
{
    FILE *file = fopen(path, "r");
    if(!file)
        return PBO_ERROR_IO;
    return pbo_add_file_f(d, name, file);
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
    if(!d)
        return 0;

    struct list_entry *e = pbo_find_file(d, filename);
    if(!e)
        return 0; //Doesn't Exist

    return e->data->properties[DATA_SIZE];
}

pbo_error pbo_write_to_file(pbo_t d, const char *filename, FILE *file)
{
    if(!d)
        return PBO_ERROR_NEXIST;

    struct list_entry *le = pbo_find_file(d, filename);
    if(!le)
        return PBO_ERROR_NEXIST; //Doesn't exist

    FILE *f = fopen(d->filename, "r");
    if(!f)
        return PBO_ERROR_IO;

    fseek(f, le->data->file_offset + d->headersz, SEEK_SET);
    size_t sz = le->data->properties[DATA_SIZE];
    char buf[sz];
    fread(buf, 1, sz, f);
    fwrite(buf, 1, sz, file);
    fclose(f);
}

void pbo_dump_header(pbo_t d)
{
    if(!d)
        return;

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

static void pbo_list_add_entry(pbo_t d, struct pbo_entry *pe)
{
    if(!d)
        return;

    struct list_entry *le = malloc(sizeof *le);
    if(!le)
        return; //Malloc Error
    le->data = pe;

    if(!d->root)
        d->root = le;
    else
        d->last->next = le;
    d->last = le;
}

static void pbo_clear_list(pbo_t d)
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
    d->last = NULL;
}

static struct list_entry *pbo_find_file(pbo_t d, const char *file)
{
    if(!d)
        return NULL;

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