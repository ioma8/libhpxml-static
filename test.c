/* Copyright 2025 contributors
 *
 * This file is part of libhpxml.
 *
 * Libhpxml is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 */

/*! \file test.c
 * Tests for the static-memory API of libhpxml.
 * All control structures, tag structures, and I/O buffers are declared as
 * static (no dynamic memory allocation).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bstring.h"
#include "libhpxml.h"


/* -------------------------------------------------------------------------
 * Small test infrastructure
 * ------------------------------------------------------------------------- */

static int tests_run = 0;
static int tests_failed = 0;

static void check(int cond, const char *expr, const char *file, int line)
{
   tests_run++;
   if (!cond)
   {
      tests_failed++;
      fprintf(stderr, "FAIL [%s:%d]: %s\n", file, line, expr);
   }
}

#define CHECK(cond) check((cond), #cond, __FILE__, __LINE__)


/* -------------------------------------------------------------------------
 * Test 1: hpx_tag_init() sets mattr correctly and zeroes the structure
 * ------------------------------------------------------------------------- */

static void test_tag_init(void)
{
   hpx_tag_t tag;

   /* initialize with explicit capacity */
   hpx_tag_init(&tag, 16);
   CHECK(tag.mattr == 16);
   CHECK(tag.nattr == 0);
   CHECK(tag.type  == 0);

   /* capacity is clamped to HPX_MAX_ATTR */
   hpx_tag_init(&tag, HPX_MAX_ATTR + 1);
   CHECK(tag.mattr == HPX_MAX_ATTR);

   /* n == 0 defaults to HPX_MAX_ATTR */
   hpx_tag_init(&tag, 0);
   CHECK(tag.mattr == HPX_MAX_ATTR);
}


/* -------------------------------------------------------------------------
 * Test 2: hpx_init_static() / hpx_init_membuf() – control structure init
 * ------------------------------------------------------------------------- */

static char sbuf[256];
static hpx_ctrl_t sctl;

static void test_ctrl_init(void)
{
   hpx_init_static(&sctl, sbuf, (long)sizeof(sbuf), 1);
   CHECK(sctl.fd      == 1);
   CHECK(sctl.lineno  == 1);
   CHECK(sctl.buf.buf == sbuf);
   CHECK(sctl.len     == (long)sizeof(sbuf));
   CHECK(sctl.empty   == 1);
   CHECK(sctl.mmap    == 0);

   /* hpx_init_membuf with a const string buffer */
   {
      const char mem[] = "<root/>";
      hpx_ctrl_t mctl;
      hpx_init_membuf(&mctl, (void *)(mem), (int)sizeof(mem) - 1);
      CHECK(mctl.fd  == -1);
      CHECK(mctl.len == (long)(sizeof(mem) - 1));
      CHECK(mctl.buf.buf == mem);
   }
}


/* -------------------------------------------------------------------------
 * Test 3: hpx_free() is a no-op for non-mmap ctl (no crash expected)
 * ------------------------------------------------------------------------- */

static void test_hpx_free_noop(void)
{
   hpx_ctrl_t ctl;
   char tmp[64];
   hpx_init_static(&ctl, tmp, (long)sizeof(tmp), -1);
   /* must not crash */
   hpx_free(&ctl);
   CHECK(1);
}


/* -------------------------------------------------------------------------
 * Test 4: Parse a simple XML snippet from an in-memory buffer
 * ------------------------------------------------------------------------- */

static void test_membuf_simple(void)
{
   static const char xml[] =
      "<?xml version=\"1.0\"?>"
      "<root attr1=\"val1\" attr2=\"val2\">"
      "<child/>"
      "<leaf>text content</leaf>"
      "</root>";

   hpx_ctrl_t ctl;
   hpx_tag_t  tag;
   bstring_t  b;
   long lno;
   int open_cnt = 0, close_cnt = 0, single_cnt = 0, instr_cnt = 0, lit_cnt = 0;

   hpx_init_membuf(&ctl, (void *)xml, (int)(sizeof(xml) - 1));
   hpx_tag_init(&tag, HPX_MAX_ATTR);

   while (hpx_get_elem(&ctl, &b, NULL, &lno) > 0)
   {
      if (hpx_process_elem(b, &tag) != 0)
         continue;

      switch (tag.type)
      {
         case HPX_INSTR:   instr_cnt++;  break;
         case HPX_OPEN:    open_cnt++;   break;
         case HPX_CLOSE:   close_cnt++;  break;
         case HPX_SINGLE:  single_cnt++; break;
         case HPX_LITERAL: lit_cnt++;    break;
         default: break;
      }
   }

   CHECK(ctl.eof == 1);
   CHECK(instr_cnt  == 1);
   CHECK(open_cnt   == 2);   /* <root>, <leaf> */
   CHECK(single_cnt == 1);   /* <child/> */
   CHECK(close_cnt  == 2);   /* </root>, </leaf> */
   CHECK(lit_cnt    >= 1);   /* "text content" */
}


/* -------------------------------------------------------------------------
 * Test 5: Attribute parsing – verify name/value of parsed attributes
 * ------------------------------------------------------------------------- */

static void test_attr_parsing(void)
{
   static const char xml[] = "<tag foo=\"bar\" baz='qux'/>";
   hpx_ctrl_t ctl;
   hpx_tag_t  tag;
   bstring_t  b;
   long lno;

   hpx_init_membuf(&ctl, (void *)xml, (int)(sizeof(xml) - 1));
   hpx_tag_init(&tag, HPX_MAX_ATTR);

   CHECK(hpx_get_elem(&ctl, &b, NULL, &lno) > 0);
   CHECK(hpx_process_elem(b, &tag) == 0);
   CHECK(tag.type  == HPX_SINGLE);
   CHECK(tag.nattr == 2);

   /* first attribute: foo="bar" */
   CHECK(tag.attr[0].name.len  == 3);
   CHECK(strncmp(tag.attr[0].name.buf,  "foo", 3) == 0);
   CHECK(tag.attr[0].value.len == 3);
   CHECK(strncmp(tag.attr[0].value.buf, "bar", 3) == 0);
   CHECK(tag.attr[0].delim == '"');

   /* second attribute: baz='qux' */
   CHECK(tag.attr[1].name.len  == 3);
   CHECK(strncmp(tag.attr[1].name.buf,  "baz", 3) == 0);
   CHECK(tag.attr[1].value.len == 3);
   CHECK(strncmp(tag.attr[1].value.buf, "qux", 3) == 0);
   CHECK(tag.attr[1].delim == '\'');
}


/* -------------------------------------------------------------------------
 * Test 6: Comment and CDATA nodes are recognised
 * ------------------------------------------------------------------------- */

static void test_comment_cdata(void)
{
   static const char xml[] =
      "<r><!-- a comment --><![CDATA[raw <data>]]></r>";
   hpx_ctrl_t ctl;
   hpx_tag_t  tag;
   bstring_t  b;
   long lno;
   int comment_cnt = 0, cdata_cnt = 0;

   hpx_init_membuf(&ctl, (void *)xml, (int)(sizeof(xml) - 1));
   hpx_tag_init(&tag, HPX_MAX_ATTR);

   while (hpx_get_elem(&ctl, &b, NULL, &lno) > 0)
   {
      if (hpx_process_elem(b, &tag) != 0)
         continue;
      if (tag.type == HPX_COMMENT) comment_cnt++;
      if (tag.type == HPX_CDATA)   cdata_cnt++;
   }

   CHECK(comment_cnt == 1);
   CHECK(cdata_cnt   == 1);
}


/* -------------------------------------------------------------------------
 * Test 7: hpx_tree_t uses fixed-size array – no heap allocation needed
 * ------------------------------------------------------------------------- */

static void test_tree_static(void)
{
   hpx_tree_t tree;
   hpx_tag_t  tag1, tag2;

   memset(&tree, 0, sizeof(tree));
   tree.msub = HPX_MAX_SUBTAGS;

   hpx_tag_init(&tag1, 4);
   hpx_tag_init(&tag2, 4);

   /* manually insert two subtag pointers */
   tree.subtag[0] = NULL;
   tree.subtag[1] = NULL;
   tree.tag  = &tag1;
   tree.nsub = 0;

   CHECK(tree.msub == HPX_MAX_SUBTAGS);
   CHECK(tree.nsub == 0);

   /* no-op free must not crash */
   hpx_tm_free(&tag1);
   hpx_tm_free_tree(&tree);
   CHECK(1);
}


/* -------------------------------------------------------------------------
 * Test 8: Parse test.xml from file using block I/O (hpx_init_static)
 * ------------------------------------------------------------------------- */

#define FILE_BUF_SIZE (64 * 1024)

static hpx_ctrl_t file_ctl;
static char       file_buf[FILE_BUF_SIZE];
static hpx_tag_t  file_tag;

static void test_file_parse(void)
{
   FILE *f;
   int fd;
   bstring_t b;
   long lno;
   int elem_count = 0;

   f = fopen("test.xml", "r");
   if (f == NULL)
   {
      fprintf(stderr, "SKIP test_file_parse: cannot open test.xml\n");
      return;
   }
   fd = fileno(f);

   hpx_init_static(&file_ctl, file_buf, FILE_BUF_SIZE, fd);
   hpx_tag_init(&file_tag, HPX_MAX_ATTR);

   while (hpx_get_elem(&file_ctl, &b, NULL, &lno) > 0)
   {
      if (hpx_process_elem(b, &file_tag) == 0)
         elem_count++;
   }

   CHECK(file_ctl.eof == 1);
   /* test.xml contains at least 20 parsed elements */
   CHECK(elem_count >= 20);

   fclose(f);
}


/* -------------------------------------------------------------------------
 * Test 9: Sizes of structs are as expected (no FAM, fixed arrays)
 * ------------------------------------------------------------------------- */

static void test_struct_sizes(void)
{
   /* hpx_tag_t must be large enough to hold HPX_MAX_ATTR attributes */
   CHECK(sizeof(hpx_tag_t) >= HPX_MAX_ATTR * sizeof(hpx_attr_t));

   /* hpx_tree_t must be large enough to hold HPX_MAX_SUBTAGS pointers */
   CHECK(sizeof(hpx_tree_t) >= HPX_MAX_SUBTAGS * sizeof(hpx_tree_t *));
}


/* -------------------------------------------------------------------------
 * Test 10: mattr clamping – values above HPX_MAX_ATTR are clamped
 * ------------------------------------------------------------------------- */

static void test_mattr_clamping(void)
{
   hpx_tag_t tag;

   hpx_tag_init(&tag, HPX_MAX_ATTR * 2);
   CHECK(tag.mattr == HPX_MAX_ATTR);

   hpx_tag_init(&tag, -5);
   CHECK(tag.mattr == HPX_MAX_ATTR);

   hpx_tag_init(&tag, 1);
   CHECK(tag.mattr == 1);
}


/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */

int main(void)
{
   test_tag_init();
   test_ctrl_init();
   test_hpx_free_noop();
   test_membuf_simple();
   test_attr_parsing();
   test_comment_cdata();
   test_tree_static();
   test_file_parse();
   test_struct_sizes();
   test_mattr_clamping();

   if (tests_failed == 0)
      printf("All %d tests passed.\n", tests_run);
   else
      printf("%d/%d tests FAILED.\n", tests_failed, tests_run);

   return tests_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
