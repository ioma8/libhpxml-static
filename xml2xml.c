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

/*! \file xml2xml.c
 * This is an example which parse the XML input data from stdin and rewrites it
 * to stdout.
 * \author Bernhard R. Fischer, <bf@abenteuerland.at>
 * \date 2025/03/14
 */

#include <stdio.h>
#include <stdlib.h>

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

         fprint_hpx_tag(stdout, &tag);
      }
      else
         fprintf(stderr, "[%ld] ERROR in element: %.*s\n", lno, b.len, b.buf);
   }

   if (!ctl.eof)
      perror("hpx_get_elem"), exit(EXIT_FAILURE);

   exit(EXIT_SUCCESS);
}

