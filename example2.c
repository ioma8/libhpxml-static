#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bstring.h"
#include "libhpxml.h"


#define BUF_SIZE (4 * 1024 * 1024)

static hpx_ctrl_t ctl;
static char buf[BUF_SIZE];
static hpx_tag_t tag;


int is_doctype(const hpx_tag_t *t)
{
   return t->type == HPX_ATT && t->tag.len >= 8 && isspace(t->tag.buf[7]) && !strncasecmp(t->tag.buf, "DOCTYPE", 7);
}


int proc_subset(hpx_tag_t *t)
{
   hpx_ctrl_t _ctl;
   int i, sqcnt;
   bstring_t b;
   long lno;

   printf("===== BEGIN PROCESS SUBSET=====\n");

   b = t->tag;

   // skip until opening tag '['
   for (; b.len && *b.buf != '['; b.buf++, b.len--);

   // no data or start tag
   if (b.len < 2 || *b.buf != '[')
      return -1;

   // find end tag ']'
   for (i = 0, sqcnt = 0; i < b.len; i++)
   {
      if (b.buf[i] == '[')
      {
         sqcnt++;
      }
      else if (b.buf[i] == ']')
      {
         sqcnt--;
         if (!sqcnt)
            break;
      }
   }

   // no closing tag or too short?
   if (i >= b.len || i < 2)
      return -1;

   // remove enclosing square brackets
   b.len = i - 2;
   b.buf++;

   hpx_init_membuf(&_ctl, b.buf, b.len);

   // loop as long as XML elements are available
   while (hpx_get_elem(&_ctl, &b, NULL, &lno) > 0)
   {
      // parse XML element
      if (!hpx_process_elem(b, t))
      {
         // element successfully parsed, do something with it
         // ...
         // ...

         printf("[%ld] type=%d, name=%.*s, nattr=%d\n", lno, t->type, t->tag.len, t->tag.buf, t->nattr);
      }
      else
         printf("[%ld] ERROR in element: %.*s\n", lno, b.len, b.buf);
   }

   printf("===== END PROCESS SUBSET =====\n");
   return 0;
}


int main(int argc, char *argv[])
{
   bstring_t b;
   long lno;

   (void) argc; (void) argv;

   // initialize control structure from stdin using a static buffer
   hpx_init_static(&ctl, buf, BUF_SIZE, 0);
   // initialize tag structure with maximum 64 attributes
   hpx_tag_init(&tag, HPX_MAX_ATTR);

   // loop as long as XML elements are available
   while (hpx_get_elem(&ctl, &b, NULL, &lno) > 0)
   {
      // parse XML element
      if (!hpx_process_elem(b, &tag))
      {
         // element successfully parsed, do something with it
         // ...
         // ...

         printf("[%ld] type=%d, name=%.*s, nattr=%d\n", lno, tag.type, tag.tag.len, tag.tag.buf, tag.nattr);

         // process doctype subtypes
         if (is_doctype(&tag))
            proc_subset(&tag);
      }
      else
         printf("[%ld] ERROR in element: %.*s\n", lno, b.len, b.buf);
   }

   if (!ctl.eof)
      perror("hpx_get_elem"), exit(EXIT_FAILURE);

   exit(EXIT_SUCCESS);
}

