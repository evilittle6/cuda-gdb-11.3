/* Exception (throw catch) mechanism, for GDB, the GNU debugger.

   Copyright (C) 1986-2019 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "defs.h"
#include "exceptions.h"
#include "breakpoint.h"
#include "target.h"
#include "inferior.h"
#include "annotate.h"
#include "ui-out.h"
#include "serial.h"
#include "gdbthread.h"
#include "top.h"
#include "common/gdb_optional.h"

static void
print_flush (void)
{
  struct ui *ui = current_ui;
  struct serial *gdb_stdout_serial;

  if (deprecated_error_begin_hook)
    deprecated_error_begin_hook ();

  gdb::optional<target_terminal::scoped_restore_terminal_state> term_state;
  /* While normally there's always something pushed on the target
     stack, the NULL check is needed here because we can get here very
     early during startup, before the target stack is first
     initialized.  */
  if (current_top_target () != NULL && target_supports_terminal_ours ())
    {
      term_state.emplace ();
      /* CUDA - this used to be `target_terminal::ours_for_output`

         Changed this to `ours` since otherwise inferior's group stays
         in the foreground and in cases where GDB's sigmask is empty
         (e.g. during inferior startup) `tcdrain` below would lead
         to SIGTTOU and GDB stopping in the background in shell */
      target_terminal::ours ();
    }

  /* We want all output to appear now, before we print the error.  We
     have 3 levels of buffering we have to flush (it's possible that
     some of these should be changed to flush the lower-level ones
     too):  */

  /* 1.  The _filtered buffer.  */
  if (filtered_printing_initialized ())
    wrap_here ("");

  /* 2.  The stdio buffer.  */
  gdb_flush (gdb_stdout);
  gdb_flush (gdb_stderr);

  /* 3.  The system-level buffer.  */
  gdb_stdout_serial = serial_fdopen (fileno (ui->outstream));
  if (gdb_stdout_serial)
    {
      serial_drain_output (gdb_stdout_serial);
      serial_un_fdopen (gdb_stdout_serial);
    }

  annotate_error_begin ();
}

static void
print_exception (struct ui_file *file, struct gdb_exception e)
{
  /* KLUGE: cagney/2005-01-13: Write the string out one line at a time
     as that way the MI's behavior is preserved.  */
  const char *start;
  const char *end;

  for (start = e.message; start != NULL; start = end)
    {
      end = strchr (start, '\n');
      if (end == NULL)
	fputs_filtered (start, file);
      else
	{
	  end++;
	  ui_file_write (file, start, end - start);
	}
    }					    
  fprintf_filtered (file, "\n");

  /* Now append the annotation.  */
  switch (e.reason)
    {
    case RETURN_QUIT:
      annotate_quit ();
      break;
    case RETURN_ERROR:
      /* Assume that these are all errors.  */
      annotate_error ();
      break;
    default:
      internal_error (__FILE__, __LINE__, _("Bad switch."));
    }
}

void
exception_print (struct ui_file *file, struct gdb_exception e)
{
  if (e.reason < 0 && e.message != NULL)
    {
      print_flush ();
      print_exception (file, e);
    }
}

void
exception_fprintf (struct ui_file *file, struct gdb_exception e,
		   const char *prefix, ...)
{
  if (e.reason < 0 && e.message != NULL)
    {
      va_list args;

      print_flush ();

      /* Print the prefix.  */
      va_start (args, prefix);
      vfprintf_filtered (file, prefix, args);
      va_end (args);

      print_exception (file, e);
    }
}

/* See exceptions.h.  */

int
exception_print_same (struct gdb_exception e1, struct gdb_exception e2)
{
  const char *msg1 = e1.message;
  const char *msg2 = e2.message;

  if (msg1 == NULL)
    msg1 = "";
  if (msg2 == NULL)
    msg2 = "";

  return (e1.reason == e2.reason
	  && e1.error == e2.error
	  && strcmp (msg1, msg2) == 0);
}
