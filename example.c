#include <stdio.h>

#include "bstring.h"
#include "libhpxml.h"


#define BUF_SIZE (4 * 1024 * 1024)

static hpx_ctrl_t ctl;
static char buf[BUF_SIZE];
static hpx_tag_t tag;


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
      }
      else
         printf("[%ld] ERROR in element: %.*s\n", lno, b.len, b.buf);
   }

   if (!ctl.eof)
      perror("hpx_get_elem"), exit(EXIT_FAILURE);

   exit(EXIT_SUCCESS);
}

