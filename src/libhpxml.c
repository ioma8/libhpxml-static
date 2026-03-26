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

/*! \file libhpxml.c
 * This file contains the complete source for the parser.
 * \author Bernhard R. Fischer, <bf@abenteuerland.at>
 * \date 2025/04/03
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#ifdef WITH_MMAP
#include <sys/mman.h>
#ifndef MAP_NORESERVE
// MAP_NORESERVE is not defined an all systems is not necessary
#define MAP_NORESERVE 0
#endif
#endif

#ifndef MADV_WILLNEED
#ifdef POSIX_MADV_WILLNEED
#define MADV_WILLNEED POSIX_MADV_WILLNEED
#else
#warning "cannot define MADV_WILLNEED, will undef madvise()"
#undef HAVE_MADVISE
#undef HAVE_POSIX_MADVISE
#endif
#endif
#ifndef MADV_DONTNEED
#ifdef POSIX_MADV_DONTNEED
#define MADV_DONTNEED POSIX_MADV_DONTNEED
#else
#warning "cannot define MADV_MADV_DONTNEED, will undef madvise()"
#undef HAVE_MADVISE
#undef HAVE_POSIX_MADVISE
#endif
#endif

#include "bstring.h"
#include "libhpxml.h"


/*! 
 *  @param b Pointer to bstring.
 *  @return Number of remaining characters in b.
 */
int skip_bblank(bstring_t *b)
{
   for (; isspace((unsigned) *b->buf) && b->len; bs_advance(b));
   return b->len;
}


/*! Test if character c is XML string delimiter ["'] and return it.
 * @param c Character to test.
 * @return If c is one of ["'] return it, otherwise 0 is returned.
 */
int is_delim(char c)
{
   switch (c)
   {
      case '"':
      case '\'':
         return c;
   }
   return 0;
}


/*! No-op: static hpx_tag_t values need no freeing. Kept for API compatibility. */
void hpx_tm_free(hpx_tag_t *t)
{
   (void) t;
}


/*! No-op: static hpx_tree_t values need no freeing. Kept for API compatibility. */
void hpx_tm_free_tree(hpx_tree_t *tlist)
{
   (void) tlist;
}


/*! Initialize a caller-allocated hpx_tag_t with space for up to n attributes.
 *  n is clamped to HPX_MAX_ATTR. */
void hpx_tag_init(hpx_tag_t *t, int n)
{
   memset(t, 0, sizeof(*t));
   t->mattr = (n > 0 && n <= HPX_MAX_ATTR) ? n : HPX_MAX_ATTR;
}


/*!
 *  @param b Pointer to bstring buffer which should be parsed.
 *  @param n Destination bstring.
 *  @return number of valid characters found.
 */
int hpx_parse_name(bstring_t *b, bstring_t *n)
{
   if (!IS_XML1CHAR((unsigned) *b->buf))
      return 0;

   n->buf = b->buf;
   bs_advance(b);
   for (n->len = 1; IS_XMLCHAR((unsigned) *b->buf) && b->len; bs_advance(b), n->len++);
   return n->len;
}


int hpx_parse_attr_list(bstring_t *b, hpx_tag_t *t)
{
   for (t->nattr = 0; t->nattr < t->mattr; t->nattr++)
   {
      if (!skip_bblank(b))
         break;

      if (!hpx_parse_name(b, &t->attr[t->nattr].name))
         break;

      if (!skip_bblank(b))
         break;

      if (*b->buf != '=')
      {
         t->attr[t->nattr].value.buf = NULL;
         t->attr[t->nattr].value.len = 0;
         break;
      }

      if (!bs_advance(b))
         break;

      if (!skip_bblank(b))
         break;

      t->attr[t->nattr].delim = *b->buf;

      if ((t->attr[t->nattr].delim != '"') && (t->attr[t->nattr].delim != '\''))
         break;

      if (!bs_advance(b))
         break;

      t->attr[t->nattr].value.buf = b->buf;
      for (t->attr[t->nattr].value.len = 0; b->len && (*b->buf != t->attr[t->nattr].delim); bs_advance(b), t->attr[t->nattr].value.len++);

      if (!b->len)
         break;

      bs_advance(b);
   }

   return t->nattr;
}

 
/*! Parses bstring into hpx_tag_t structure. The bstring buffer must contain a
 * single XML element with correct boundaries. This is either a tag (<....>) or
 * just text.
 * @param b Bstring containing pointer to an element.
 * @param p Pointer to valid hpx_tag_t structure. The structure will we filled
 * out.
 * @return Returns 0 if the bstring could be successfully parsed, otherwise -1.
 */
int hpx_process_elem(bstring_t b, hpx_tag_t *p)
{
   p->nattr = 0;
   if (b.len && (*b.buf != '<'))
   {
      p->type = HPX_LITERAL;
      p->tag = b;
      return 0;
   }

   p->type = HPX_ILL;

   if (!bs_advance(&b))
      return -1;

   if (!skip_bblank(&b))
      return -1;

   //if (isalpha(*b.buf) || (*b.buf == '_') || (*b.buf == ':'))
   if (IS_XML1CHAR((unsigned) *b.buf))
   {
      hpx_parse_name(&b, &p->tag);
      hpx_parse_attr_list(&b, p);

      if (!skip_bblank(&b))
         return -1;

      if (*b.buf == '>')
      {
         p->type = HPX_OPEN;
         // call tag processor
         return 0;
      }

      if (*b.buf != '/')
         return -1;

      if (!bs_advance(&b))
         return -1;

      if (!skip_bblank(&b))
         return -1;

      if (*b.buf != '>')
         return -1;

      p->type = HPX_SINGLE;
      //call tag processor
      return 0;
   }

   if (*b.buf == '/')
   {
      if (!bs_advance(&b))
         return -1;

      if (!skip_bblank(&b))
         return -1;

      hpx_parse_name(&b, &p->tag);

      if (!skip_bblank(&b))
         return -1;

      if (*b.buf != '>')
         return -1;

      p->type = HPX_CLOSE;
      //call tag processor
      return 0;
   }

   if (*b.buf == '!')
   {
      bs_advance(&b);

      // check for comment
      if ((b.len >= 2) && !strncmp(b.buf, "--", 2))
      {
         b.buf += 2;
         b.len -= 2;
         p->tag.buf = b.buf;

         // find end marker
         for (p->tag.len = 0; (b.len >= 3) && strncmp(b.buf, "-->", 3); bs_advance(&b), p->tag.len++);

         if (b.len < 3)
            return -1;

         p->type = HPX_COMMENT;
         //call tag processor
         return 0;
      }

      // check for CDATA
      if ((b.len >= 7) && !strncmp(b.buf, "[CDATA[", 7))
      {
         b.buf += 7;
         b.len -= 7;
         p->tag.buf = b.buf;

         // find end marker
         for (p->tag.len = 0; (b.len >= 3) && strncmp(b.buf, "]]>", 3); bs_advance(&b), p->tag.len++);

         if (b.len < 3)
            return -1;

         p->type = HPX_CDATA;
         //call tag processor
         return 0;
      }

      if (b.len)
         b.len--;
      p->tag = b;
      p->type = HPX_ATT;
      //call tag processor
      return 0;
   }

   if (*b.buf == '?')
   {
      bs_advance(&b);
      hpx_parse_name(&b, &p->tag);
      hpx_parse_attr_list(&b, p);

      if (!skip_bblank(&b))
         return -1;

      if ((b.len >= 2) && !strncmp(b.buf, "?>", 2))
      {
         p->type = HPX_INSTR;
         // call tag processor
         return 0;
      }
      return -1;
   }

   // FIXME: return value correct?
   return -1;
}


/*! Checks for XML white spaces ([\t\n\r]) and increases the line number counter.
 * @param c Pointer to character.
 * @return Returns 0 if character contains any of [ \t\r\n], otherwise 1.
 */
int cblank(const char *c, long *lno)
{
   switch (*c)
   {
      case '\n':
         if (lno != NULL)
            (*lno)++;
         /* fall through */
      case '\t':
      case '\r':
      case ' ':
         return 0;
   }

   return 1;
}


/*! Returns length of tag.
 *  @param buf Pointer to buffer.
 *  @param len Length of buffer.
 *  @param lno Pointer to line number counter. May be NULL.
 *  @return Lendth of tag content including '<' and '>'. If return value > len,
 *  the tag is unclosed.
 */
int count_tag(bstringl_t b, long *lno)
{
#define HPX_DOCTYPE 0x100
   int i = 0, c = HPX_ILL, sqcnt = 0, d;

   // manage comments
   if ((b.len >= 7) && !strncmp(b.buf + 1, "!--", 3))
      c = HPX_COMMENT, i = 4;
   // manage CDATA
   if ((b.len >= 12) && !strncmp(b.buf + 1, "![CDATA[", 8))
      c = HPX_CDATA, i = 9;
   // manage DOCTYPE sub entities
   if ((b.len >= 10) && !strncasecmp(b.buf + 1 , "!DOCTYPE", 8) && (isspace(b.buf[9]) || (b.buf[9] == '>')))
      c = HPX_DOCTYPE, i = 9;

   for (b.buf += i, d = 0; i < b.len; i++, b.buf++)
   {
      // check for string delimiter if outside of delimited string
      if (!d)
         d = is_delim(*b.buf);
      // check for end delimiter if inside of a delimited string
      else if (d == is_delim(*b.buf))
         d = 0;

      if (*b.buf == '>')
      {
         if (c == HPX_ILL && !d)
            break;
         if ((c == HPX_COMMENT) && (i >= 7) && !strncmp(b.buf - 2, "--", 2))
            break;
         if ((c == HPX_CDATA) && (i >= 12) && !strncmp(b.buf - 2, "]]", 2))
            break;
         if ((c == HPX_DOCTYPE) && !sqcnt)
            break;
         else
            continue;
      }
      // count square brackets in DOCTYPE
      else if (c == HPX_DOCTYPE)
         switch (*b.buf)
         {
            case '[':
               sqcnt++;
               break;

            case ']':
               sqcnt--;
               break;
         }

      (void) cblank(b.buf, lno);
   }

   return i + 1;
}


/*! Returns length of literal.
 *  @param b Bstring_t of buffer to check.
 *  @param nbc Pointer to integer which counts non-blank characters.
 *  @param lno Pointer to line number counter. May be NULL.
 *  @return Length of literal. Return value == len if literal is unclosed.
 */
int count_literal(bstringl_t b, int *nbc, long *lno)
{
   int i, t;

   if (nbc != NULL)
      *nbc = 0;
   else
      nbc = &t;

   for (i = 0; i < b.len; i++, b.buf++)
   {
      if (*b.buf == '<')
         break;

      *nbc += cblank(b.buf, lno);
   }

   return i;
}


/*! Parse XML element into bstring.
 *  @param ctl Hpx control structure.
 *  @param b Pointer to bstring.
 *  @param lno Pointer to integer which will receive the line number of the
 *  element. lno may be NULL.
 *  @return Length of element or -1 if element is unclosed.
 */
int hpx_proc_buf(hpx_ctrl_t *ctl, bstringl_t *b, long *lno)
{
   int i, s, n;

   if (ctl->in_tag)
   {
      if (lno != NULL)
         *lno = ctl->lineno;
      s = count_tag(*b, &ctl->lineno);
      if (s > b->len)
         return -1;
      b->len = s;

      // check if it is a regular opening tag and preserve its name
      if (IS_XML1CHAR(b->buf[1]))
      {
         ctl->last_open.buf = b->buf + 1;
         for (ctl->last_open.len = 1; IS_XMLCHAR((unsigned) ctl->last_open.buf[ctl->last_open.len]) && (ctl->last_open.len < b->len - 2); ctl->last_open.len++);
      }
      else
         ctl->last_open.len = 0;
   }
   else
   {
      if (lno != NULL)
         *lno = ctl->lineno;

      s = count_literal(*b, &n, &ctl->lineno);
      // check if literal had no end tag (i.e. '<')
      if (s == b->len)
         return -1;

      // !(check if we are within an opening tag and there's the corresponding closing tag)
      if (!ctl->last_open.len || (b->len - s <= ctl->last_open.len + 2) || (b->buf[s + 1] != '/') || strncmp(&b->buf[s + 2], ctl->last_open.buf, ctl->last_open.len))
      {
         // reset line number
         if (lno != NULL)
            ctl->lineno = *lno;
         // skip leading white spaces
         for (i = 0; i < b->len && !cblank(b->buf, &ctl->lineno); i++)
            bs_advancel(b);
         if (i == b->len)
            return -1;

         if (lno != NULL)
            *lno = ctl->lineno;

         // cut trailing white spaces
         for (b->len = s - i; b->len && isspace((unsigned) b->buf[b->len - 1]); b->len--);
      }
      else
         b->len = s;

      ctl->last_open.len = 0;
   }

   return s;
}


/*! This function is wrapper for either madvise() or posix_madvise().
 */
static int hpx_madvise(void *addr, size_t length, int advice)
{
#ifdef HAVE_MADVISE
   return madvise(addr, length, advice);
#elif HAVE_POSIX_MADVISE
   return posix_madvise(addr, length, advice);
#else
   return 0;
#endif
}


/*! Initialize a caller-allocated hpx_ctrl_t for block I/O.
 *  @param ctl  Caller-allocated control structure (no malloc is performed).
 *  @param buf  Caller-provided read buffer.
 *  @param buflen  Number of bytes in buf.
 *  @param fd   Input file descriptor.
 */
void hpx_init_static(hpx_ctrl_t *ctl, char *buf, long buflen, int fd)
{
   memset(ctl, 0, sizeof(*ctl));
   ctl->fd = fd;
   ctl->lineno = 1;
   ctl->buf.buf = buf;
   ctl->len = buflen;
   ctl->empty = 1;
}


/*! This function initializes a hpx_ctrl_t structure to be used for input from
 * a memory buffer (instead of a file).
 * @param ctl Pointer to hpx_ctrl_t structure which will be initialized
 *    properly.
 * @param buf Pointer to memory buffer.
 * @param len Number of bytes within memory buffer.
 */
void hpx_init_membuf(hpx_ctrl_t *ctl, void *buf, int len)
{
   memset(ctl, 0, sizeof(*ctl));
   ctl->buf.len = len;
   ctl->buf.buf = buf;
   ctl->fd = -1;
   ctl->len = len;
   ctl->lineno = 1;
}


/*! Initialize a caller-allocated hpx_ctrl_t to memory-map fd (no malloc).
 *  @param ctl      Caller-allocated control structure.
 *  @param fd       Input file descriptor.
 *  @param filelen  Byte length of the file to map.
 *  @return 0 on success, -1 on error (errno is set).
 */
int hpx_init_mmap(hpx_ctrl_t *ctl, int fd, long filelen)
{
#ifdef WITH_MMAP
   memset(ctl, 0, sizeof(*ctl));
   ctl->fd = fd;
   ctl->lineno = 1;
   ctl->len = filelen;
   ctl->buf.len = filelen;
   if ((ctl->buf.buf = mmap(NULL, ctl->len, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, fd, 0)) == MAP_FAILED)
      return -1;
   ctl->mmap = 1;
   ctl->madv_ptr = ctl->buf.buf;
   if ((ctl->pg_siz = sysconf(_SC_PAGESIZE)) == -1)
      ctl->pg_siz = 0;
   ctl->pg_blk_siz = ctl->pg_siz * MMAP_PAGES;
   hpx_madvise(ctl->madv_ptr, ctl->pg_blk_siz <= ctl->len ? ctl->pg_blk_siz : ctl->len, MADV_WILLNEED);
   return 0;
#else
   (void) ctl;
   (void) fd;
   (void) filelen;
   errno = EINVAL;
   return -1;
#endif
}


/*! Release resources: calls munmap() when mmap was used.
 *  ctl itself is caller-owned (no heap memory to free). */
void hpx_free(hpx_ctrl_t *ctl)
{
#ifdef WITH_MMAP
   if (ctl->mmap)
      (void) munmap(ctl->buf.buf, ctl->len);
#else
   (void) ctl;
#endif
}


/*!
 *  @param ctl Pointer to valid hpx_ctrl_t structure.
 *  @param b Pointer to bstring_t. This structure will be filled out by this
 *  function.
 *  @param in_tag is set to 1 or 0, either it is a tag or not. It is optional
 *  and may be NULL.
 *  @param lno Pointer to integer which will contain the starting line number
 *  of b. lno may be NULL.
 *  @return Length of element (always >= 1) if everything is ok. b will contain
 *  a valid bstring to the element. -1 is returned in case of error. On eof, 0
 *  is returned.
 */
long hpx_get_eleml(hpx_ctrl_t *ctl, bstringl_t *b, int *in_tag, long *lno)
{
   long s;
   long iter;

   for (iter = 0; iter < HPX_MAX_PARSE_ITER; iter++)
   {
#ifdef WITH_MMAP
      if (ctl->mmap)
      {
         if ((ctl->buf.buf + ctl->pos) >= ctl->madv_ptr)
         {
            // pull in next block if it is available
            if (ctl->buf.buf + ctl->len > ctl->madv_ptr + ctl->pg_blk_siz)
            {
               hpx_madvise(ctl->madv_ptr + ctl->pg_blk_siz,
                     ctl->len - ctl->pos - ctl->pg_blk_siz >= ctl->pg_blk_siz ?
                     ctl->pg_blk_siz : ctl->len - ctl->pos - ctl->pg_blk_siz,
                     MADV_WILLNEED);
            }
            // mark previous block as unneeded
            if (ctl->madv_ptr - ctl->pg_blk_siz >= ctl->buf.buf)
               hpx_madvise(ctl->madv_ptr - ctl->pg_blk_siz, ctl->pg_blk_siz, MADV_DONTNEED);
            ctl->madv_ptr += ctl->pg_blk_siz;
         }
     }
#endif

      if (ctl->empty)
      {
         if (ctl->mmap)
         {
            ctl->eof = 1;
         }
         else
         {
            int retry;
            // move remaining data to the beginning of the buffer
            ctl->buf.len -= ctl->pos;
            memmove(ctl->buf.buf, ctl->buf.buf + ctl->pos, ctl->buf.len);
            ctl->pos = 0;

            // read new data from file (but not the mem buffer, i.e. fd == -1)
            for (s = 0, retry = 0; ctl->fd != -1 && retry < HPX_MAX_EINTR_RETRY; retry++)
            {
               if ((s = read(ctl->fd, ctl->buf.buf + ctl->buf.len, ctl->len - ctl->buf.len)) != -1)
                  break;

               if (errno != EINTR)
                  return -1;
            }

            if (!s)
               ctl->eof = 1;

            // adjust position pointers
            ctl->buf.len += s;
         }
         ctl->empty = 0;
      }

      // no more data available
      if (!ctl->buf.len)
         return 0;

      b->buf = ctl->buf.buf + ctl->pos;
      b->len = ctl->buf.len - ctl->pos;

      if ((s = hpx_proc_buf(ctl, b, lno)) >= 0)
      {
         if (in_tag != NULL)
            *in_tag = ctl->in_tag;

         ctl->in_tag ^= 1;
         ctl->pos += s;

         // test if empty element (literal)
         if (!b->len)
            continue;

         return b->len;
      }

      if (ctl->eof)
         return 0;

      ctl->empty = 1;
   }

   /* HPX_MAX_PARSE_ITER exhausted – distinguish from I/O error */
   errno = ELOOP;
   return -1;
}


int hpx_get_elem(hpx_ctrl_t *ctl, bstring_t *b, int *in_tag, long *lno)
{
   long e;
   bstringl_t bl;

   if ((e = hpx_get_eleml(ctl, &bl, in_tag, lno)) <= 0)
      return e;

   if (bl.len > INT_MAX)
   {
      errno = ERANGE;
      return -1;
   }

   b->len = bl.len;
   b->buf = bl.buf;
   return e;
}


int hpx_fprintf_attr(FILE *f, const hpx_attr_t *a, const char *lead)
{
   //FIXME: escaping of ['"] missing
   return fprintf(f, "%s%.*s=%c%.*s%c", lead == NULL ? "" : lead, a->name.len,
         a->name.buf, a->delim, a->value.len, a->value.buf, a->delim);
}

int hpx_fprintf_tag(FILE *f, const hpx_tag_t *p)
{
   int i, n;
   char *s = "";

   switch (p->type)
   {
      case HPX_CLOSE:
         return fprintf(f, "</%.*s>\n", p->tag.len, p->tag.buf);

      case HPX_SINGLE:
         s = "/";
         /* fall through */
      case HPX_OPEN:
         n = fprintf(f, "<%.*s", p->tag.len, p->tag.buf);
         for (i = 0; i < p->nattr; i++)
            n += hpx_fprintf_attr(f, &p->attr[i], " ");
         return n + fprintf(f, "%s>\n", s);

      case HPX_INSTR:
         n = fprintf(f, "<?%.*s", p->tag.len, p->tag.buf);
         for (i = 0; i < p->nattr; i++)
            n += hpx_fprintf_attr(f, &p->attr[i], " ");
         return n + fprintf(f, "?>\n");
 
   }
   return 0;
}


/*! This function outputs the list of attributes to the stream.
 * \param stream Stream of type FILE.
 * \param attr Pointer to the attribute list.
 * \param nattr Number of attributes in the attribute list.
 * \return Returns the number of bytes writte to stream as described in
 * fprintf(3). If an output error occurs after some bytes have already been
 * written to the stream, this number of bytes is returned. If the error occurs
 * immediately, a negative value is returned (see fprintf(3)).
 * \note Please note that this function outputs the attributes as they are. It
 * does not check for their validity which includes the delimiter, and it does
 * not escape the strings in any way. This must be accomlished by the caller.
 */
int fprint_hpx_attr(FILE *stream, const hpx_attr_t *attr, int nattr)
{
   int ret, res;

   for (res = 0; nattr > 0; nattr--, attr++, res += ret)
      if ((ret = fprintf(stream, " %.*s=%c%.*s%c", attr->name.len, attr->name.buf, attr->delim, attr->value.len, attr->value.buf, attr->delim)) < 0)
         return res ? res : ret;

   return res;
}


static const char *open_str_[] = {"", "<", "<", "</", "", "<!", "<?", "<!--", "<![CDATA["};
static const char *close_str_[] = {"", ">", "/>", ">", "", ">", "?>", "-->", "]]>"};


/*! This function outputs a complete tag in XML format. See also fprint_attr()
 * above for more information about the return value ans see the note there.
 * \param stream Stream of type FILE.
 * \param tag Pointer to a valid hpx_tag_t.
 * \return See fprint_attr() above for information about the return value.
 */
int fprint_hpx_tag(FILE *stream, const hpx_tag_t *tag)
{
   int ret, res = 0;

   if (tag->type <= HPX_ILL || tag->type >= _HPX_LAST)
      return 0;

   if ((ret = fprintf(stream, "%s%.*s", open_str_[tag->type], tag->tag.len, tag->tag.buf)) < 0)
      return ret;

   res += ret;
   if ((ret = fprint_hpx_attr(stream, tag->attr, tag->nattr)) < 0)
      return res;

   res += ret;
   if ((ret = fprintf(stream, "%s", close_str_[tag->type])) < 0)
      return res;

   res += ret;
   if (tag->type != HPX_LITERAL && tag->type != HPX_OPEN)
      if ((ret = fprintf(stream, "\n")) < 0)
         return res;

   res += ret;
   return res;
}

