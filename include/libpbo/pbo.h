/* pbo.h - (C) 2015, Emir Marincic
 * libpbo - A library to work with Arma PBO files
 * See README for contact-, COPYING for license information. */

#ifndef LIBpbo_pbo_H
#define LIBpbo_pbo_H 1

typedef enum
{
    PBO_SUCCESS = 0,
    PBO_ERROR_NEXIST,
    PBO_ERROR_BROKEN,
    PBO_ERROR_MALLOC,
    PBO_ERROR_IO,
    PBO_ERROR_STATE,
} pbo_error;

typedef void (*pbo_listcb)(const char*, void*);

typedef struct pbo *pbo_t;

pbo_t pbo_init(const char *filename);
void pbo_clear(pbo_t d);
void pbo_dispose(pbo_t d);
pbo_error pbo_set_filename(pbo_t d, const char *filename);

pbo_error pbo_read_header(pbo_t d);
pbo_error pbo_write(pbo_t d);

size_t pbo_read_file(pbo_t d, const char *filename, void *buf, size_t size);

pbo_error pbo_init_new(pbo_t d);
pbo_error pbo_add_extension(pbo_t d, const char *e);
pbo_error pbo_add_file_d(pbo_t d, const char *name, void *data,  size_t size);
pbo_error pbo_add_file_f(pbo_t d, const char *name, FILE *file);
pbo_error pbo_add_file_p(pbo_t d, const char *name, const char *path);

void pbo_get_file_list(pbo_t d, pbo_listcb cb, void *user);
size_t pbo_get_file_size(pbo_t d, const char *filename);

pbo_error pbo_write_to_file(pbo_t d, const char *filename, FILE *file);
void pbo_dump_header(pbo_t d);

#endif /* LIBpbo_pbo_H */
