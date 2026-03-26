/* Copyright 2011-2025 Bernhard R. Fischer, 4096R/8E24F29D <bf@abenteuerland.at>
 *
 * This file is part of libhpxml.
 *
 * Libhpxml is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * Libhpxml is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libhpxml. If not, see <http://www.gnu.org/licenses/>.
 */

/*! \file libhpxml.h
 * This file contains all declarations and prototypes for libhpxml.
 * \author Bernhard R. Fischer, <bf@abenteuerland.at>
 * \version 2025/03/14
 */

#ifndef HPXML_H
#define HPXML_H

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "bstring.h"


#define IS_XML1CHAR(x) (isalpha(x) || (x == '_') || (x == ':'))
#define IS_XMLCHAR(x) (isalpha(x) || isdigit(x) || (x == '.') || (x == '-') || (x == '_') || (x == ':'))

/*! Maximum number of attributes stored per tag. */
#define HPX_MAX_ATTR 64

/*! Maximum number of subtags stored in an hpx_tree_t node. */
#define HPX_MAX_SUBTAGS 64

/*! Upper bound for the main parse loop iterations (NASA fixed-bound rule). */
#define HPX_MAX_PARSE_ITER 0x7FFFFFFF

/*! Upper bound for read() EINTR retries. */
#define HPX_MAX_EINTR_RETRY 1024

#define MMAP_PAGES (1L << 15)


typedef struct hpx_ctrl
{
   //! data buffer containing pointer and number of bytes in buffer
   struct bstringl buf;
   //! file descriptor of input file
   int fd;
   //! flag set if eof
   short eof;
   //! total length of buffer
   long len;
   //! current working position
   long pos;
   //! flag to determine if next element is in or out of tag
   int in_tag;
   //! flag set if data should be read from file
   short empty;
   //! flag set if data is memory mapped
   short mmap;
   //! pointer to madvise()'d region (MADV_WILLNEED)
   char *madv_ptr;
   //! system page size
   long pg_siz;
   //! length of advised region (multiple of sysconf(_SC_PAGESIZE))
   long pg_blk_siz;
   //! structure contains a pointer to the latest tag name if it was an open tag
   bstringl_t last_open;
   //! line number counter
   long lineno;
} hpx_ctrl_t;

typedef struct hpx_attr
{
   bstring_t name;   //! name of attribute
   bstring_t value;  //! value of attribute
   char delim;       //! delimiter character of attribute value
} hpx_attr_t;

/*! Fixed-size tag structure. attr[] is sized to HPX_MAX_ATTR; mattr records
 *  how many slots the caller wishes to use (must be <= HPX_MAX_ATTR). */
typedef struct hpx_tag
{
   bstring_t tag;
   int type;
   int nattr;
   int mattr;
   hpx_attr_t attr[HPX_MAX_ATTR];
} hpx_tag_t;

/*! Fixed-size tree node. subtag[] is sized to HPX_MAX_SUBTAGS; msub records
 *  the capacity and nsub the number of currently occupied slots. */
typedef struct hpx_tree
{
   hpx_tag_t *tag;
   int nsub;
   int msub;
   struct hpx_tree *subtag[HPX_MAX_SUBTAGS];
} hpx_tree_t;

enum
{
   HPX_ILL, HPX_OPEN, HPX_SINGLE, HPX_CLOSE, HPX_LITERAL, HPX_ATT, HPX_INSTR, HPX_COMMENT, HPX_CDATA, _HPX_LAST
};


long hpx_lineno(void);

/*! Initialize a caller-allocated hpx_tag_t.  n is the desired attribute
 *  capacity; it is clamped to HPX_MAX_ATTR. */
void hpx_tag_init(hpx_tag_t *t, int n);

/*! No-op kept for API compatibility: static tags need no freeing. */
void hpx_tm_free(hpx_tag_t *t);

/*! No-op kept for API compatibility: static trees need no freeing. */
void hpx_tm_free_tree(hpx_tree_t *tlist);

int hpx_process_elem(bstring_t b, hpx_tag_t *p);

/*! Initialize a caller-allocated hpx_ctrl_t for block I/O from fd.
 *  buf and buflen describe the caller-provided read buffer (no malloc). */
void hpx_init_static(hpx_ctrl_t *ctl, char *buf, long buflen, int fd);

/*! Initialize a caller-allocated hpx_ctrl_t for parsing an in-memory buffer. */
void hpx_init_membuf(hpx_ctrl_t *ctl, void *buf, int len);

/*! Initialize a caller-allocated hpx_ctrl_t to memory-map fd (no malloc).
 *  filelen is the byte length of the file.  Returns 0 on success, -1 on error. */
int hpx_init_mmap(hpx_ctrl_t *ctl, int fd, long filelen);

/*! Release resources: calls munmap() when mmap was used.  Does not free ctl. */
void hpx_free(hpx_ctrl_t *ctl);

int hpx_get_elem(hpx_ctrl_t *ctl, bstring_t *b, int *in_tag, long *lno);
long hpx_get_eleml(hpx_ctrl_t *ctl, bstringl_t *b, int *in_tag, long *lno);
int hpx_fprintf_tag(FILE *f, const hpx_tag_t *p);

int fprint_hpx_attr(FILE *, const hpx_attr_t *, int);
int fprint_hpx_tag(FILE *, const hpx_tag_t *);

#endif

