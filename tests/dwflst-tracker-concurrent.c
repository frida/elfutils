/* Copyright (C) 2026 Trithem.

   Test concurrent libdwfl_stacktrace ELF tracker/cache handling.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3 of
   the License, or (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see
   <http://www.gnu.org/licenses/>.  */

#include <config.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <elf.h>
#include <libelf.h>
#include <libdw.h>
#include <libdwfl.h>
#include <libdwfl_stacktrace.h>

static const Dwfl_Callbacks callbacks = {
  .find_elf = NULL,
  .find_debuginfo = NULL,
  .section_address = NULL,
  .debuginfo_path = NULL
};

struct thread_context
{
  Dwflst_Process_Tracker *tracker;
  const char *module_name;
  int successes;
};

static void *
thread_work (void *arg)
{
  struct thread_context *ctx = arg;

  for (int i = 0; i < 2000; i++)
    {
      char *file_name = NULL;
      Elf *elf = NULL;

      int fd = dwflst_tracker_find_cached_elf
        (ctx->tracker, ctx->module_name, ctx->module_name,
         &file_name, &elf);

      if (fd < 0 || elf == NULL)
        return NULL;

      free (file_name);
      elf_end (elf);
      ctx->successes++;
    }

  return NULL;
}

struct replacer_context
{
  Dwflst_Process_Tracker *tracker;
  const char *module_name;
  int fd;
  int successes;
};

static void *
thread_replace (void *arg)
{
  struct replacer_context *ctx = arg;

  for (int i = 0; i < 1000; i++)
    {
      Elf *new_elf = elf_begin (ctx->fd, ELF_C_READ, NULL);

      if (new_elf == NULL)
        return NULL;

      if (!dwflst_tracker_cache_elf (ctx->tracker,
                                     ctx->module_name,
                                     ctx->module_name,
                                     new_elf,
                                     ctx->fd))
        {
          elf_end (new_elf);
          return NULL;
        }

      elf_end (new_elf);
      ctx->successes++;
    }

  return NULL;
}

int
main (int argc, char **argv)
{
  if (argc != 2)
    {
      fprintf (stderr, "Usage: %s ELF\n", argv[0]);
      return 1;
    }

  if (elf_version (EV_CURRENT) == EV_NONE)
    {
      fprintf (stderr, "elf_version: %s\n", elf_errmsg (-1));
      return 1;
    }

  int fd = open (argv[1], O_RDONLY);
  if (fd < 0)
    {
      fprintf (stderr, "open: %s\n", strerror (errno));
      return 1;
    }

  Elf *elf = elf_begin (fd, ELF_C_READ, NULL);
  if (elf == NULL)
    {
      fprintf (stderr, "elf_begin: %s\n", elf_errmsg (-1));
      close (fd);
      return 1;
    }

  Dwflst_Process_Tracker *tracker = dwflst_tracker_begin (&callbacks);
  if (tracker == NULL)
    {
      fprintf (stderr, "dwflst_tracker_begin failed\n");
      elf_end (elf);
      close (fd);
      return 1;
    }

  if (!dwflst_tracker_cache_elf (tracker, argv[1], argv[1], elf, fd))
    {
      fprintf (stderr, "dwflst_tracker_cache_elf failed\n");
      dwflst_tracker_end (tracker);
      elf_end (elf);
      close (fd);
      return 1;
    }

  elf_end (elf);

  char *found_file_name = NULL;
  Elf *found_elf = NULL;

  int found_fd = dwflst_tracker_find_cached_elf
    (tracker, argv[1], argv[1], &found_file_name, &found_elf);

  if (found_fd < 0 || found_elf == NULL)
    {
      fprintf (stderr, "dwflst_tracker_find_cached_elf failed\n");
      free (found_file_name);
      if (found_elf != NULL)
        elf_end (found_elf);
      dwflst_tracker_end (tracker);
      return 1;
    }

  free (found_file_name);
  elf_end (found_elf);

  pthread_t threads[2];
  struct thread_context contexts[2];
  int num_created = 0;
  int test_failed = 0;

  /* Finder/finder: exercise concurrent retention of the cached Elf. */
  for (int i = 0; i < 2; i++)
    {
      contexts[i].tracker = tracker;
      contexts[i].module_name = argv[1];
      contexts[i].successes = 0;
    }

  for (int i = 0; i < 2; i++)
    {
      int ret = pthread_create (&threads[i], NULL, thread_work, &contexts[i]);
      if (ret != 0)
        {
          fprintf (stderr, "Failed to create thread: %s\n", strerror (ret));
          test_failed = 1;
          break;
        }

      num_created++;
    }

  for (int i = 0; i < num_created; i++)
    {
      int ret = pthread_join (threads[i], NULL);
      if (ret != 0)
        {
          fprintf (stderr, "Failed to join thread: %s\n", strerror (ret));
          return 1;
        }
    }

  if (test_failed)
    {

      dwflst_tracker_end (tracker);
      return 1;
    }

  if (contexts[0].successes != 2000
      || contexts[1].successes != 2000)
    {
      fprintf (stderr, "thread test failed: %d %d\n",
               contexts[0].successes, contexts[1].successes);

      dwflst_tracker_end (tracker);
      return 1;
    }

  pthread_t finder;
  pthread_t replacer;

  struct thread_context finder_context;
  struct replacer_context replacer_context;

  finder_context.tracker = tracker;
  finder_context.module_name = argv[1];
  finder_context.successes = 0;

  replacer_context.tracker = tracker;
  replacer_context.module_name = argv[1];
  replacer_context.fd = fd;
  replacer_context.successes = 0;

  /* Finder/replacer: exercise concurrent access to the cache entry. */
  int ret = pthread_create (&finder, NULL, thread_work, &finder_context);
  if (ret != 0)
    {
      fprintf (stderr, "Failed to create finder thread: %s\n", strerror (ret));

      dwflst_tracker_end (tracker);
      return 1;
    }

  ret = pthread_create (&replacer, NULL, thread_replace, &replacer_context);
  if (ret != 0)
    {
      fprintf (stderr, "Failed to create replacer thread: %s\n",
               strerror (ret));

      ret = pthread_join (finder, NULL);
      if (ret != 0)
        {
          fprintf (stderr, "Failed to join finder thread: %s\n",
                   strerror (ret));
          return 1;
        }


      dwflst_tracker_end (tracker);
      return 1;
    }

  ret = pthread_join (finder, NULL);
  if (ret != 0)
    {
      fprintf (stderr, "Failed to join finder thread: %s\n", strerror (ret));
      return 1;
    }

  ret = pthread_join (replacer, NULL);
  if (ret != 0)
    {
      fprintf (stderr, "Failed to join replacer thread: %s\n", strerror (ret));
      return 1;
    }

  if (finder_context.successes != 2000
      || replacer_context.successes != 1000)
    {
      fprintf (stderr, "finder/replacer test failed: %d %d\n",
               finder_context.successes, replacer_context.successes);

      dwflst_tracker_end (tracker);
      return 1;
    }


  dwflst_tracker_end (tracker);

  return 0;
}
