/* pbo.h - (C) 2015, Emir Marincic
 * libpbo - A library to work with Arma PBO files
 * See README for contact-, COPYING for license information. */

#ifndef LIBpbo_pbo_H
#define LIBpbo_pbo_H 1

typedef void (*pbo_listcb)(const char*, void*);

typedef struct pbo *pbo_t;

pbo_t pbo_init(const char *filename);
void pbo_clear(pbo_t d);
void pbo_dispose(pbo_t d);
void pbo_set_filename(pbo_t d, const char *filename);
void pbo_read_header(pbo_t d);
size_t pbo_read_file(pbo_t d, const char *file, void *buf, size_t size);
void pbo_dump_header(pbo_t d);
size_t pbo_get_file_size(pbo_t d, const char *filename);
void pbo_write_to_file(pbo_t d, const char *filename, FILE *file);
void pbo_get_file_list(pbo_t d, pbo_listcb cb, void *user);

#endif /* LIBpbo_pbo_H */
