/*
   errs.c

   This file is part of GNU Anubis.
   Copyright (C) 2001, 2002, 2003, 2004, 2007 The Anubis Team.

   GNU Anubis is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   GNU Anubis is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Anubis; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "headers.h"
#include "extern.h"

void
anubis_verror (int error_code, const char *pfx, const char *fmt, va_list ap)
{
  if (options.termlevel != SILENT)
    {
      int n;
      char msg[LINEBUFFER + 1];
      size_t size = sizeof msg;

      if (pfx)
	{
	  n = snprintf (msg, size, "%s: ", pfx);
	  /* On some older systems *snprintf calls return -1 if the output
	     was truncated */
	  if (n < 0)
	    size = 0;
	  else
	    size -= n;
	}
      
      n = vsnprintf (msg, size, fmt, ap);
      if (error_code && (n >= 0 && n < sizeof msg))
	snprintf (msg + n, size - n, ": %s", strerror (error_code));
	  
#ifdef HAVE_SYSLOG
      if ((topt & T_DAEMON) && !(topt & T_FOREGROUND))
	{
	  syslog (LOG_ERR | LOG_MAIL, "%s", msg);
	  if (options.ulogfile && options.uloglevel >= FAILS)
	    filelog (options.ulogfile, msg);
	}
      else
#endif /* HAVE_SYSLOG */
      if (topt & T_FOREGROUND)
	mprintf ("[%lu] %s", (unsigned long) getpid (), msg);
      else
	mprintf ("%s", msg);
    }
}

void
anubis_error (int exit_code, int error_code, const char *fmt, ...)
{
  va_list ap;
  
  va_start (ap, fmt);
  anubis_verror (error_code, NULL, fmt, ap);
  va_end (ap);
  if (exit_code == EXIT_ABORT)
    abort ();
  else if (exit_code)
    quit (exit_code);
}  

void
anubis_warning (int error_code, const char *fmt, ...)
{
  va_list ap;
  
  va_start (ap, fmt);
  anubis_verror (error_code, _("warning"), fmt, ap);
  va_end (ap);
}

void
socket_error (const char *msg)
{
  if (msg)
    anubis_error (EXIT_FAILURE, 0, _("Could not write to socket: %s"), msg);
  else
    anubis_error (EXIT_FAILURE, errno, _("Could not write to socket"));
}

void
hostname_error (char *host)
{
  if (h_errno == 0)
    return;

  if (h_errno == HOST_NOT_FOUND)
    anubis_error (EXIT_FAILURE, 0, _("Unknown host %s."), host);
  else if (h_errno == NO_ADDRESS)
    anubis_error (EXIT_FAILURE, 0,
		  _("%s: host name is valid but does not have an IP address."),
		  host);
  else if (h_errno == NO_RECOVERY)
    anubis_error (EXIT_FAILURE, 0,
		  _("%s: unrecoverable name server error occured."), host);
  else if (h_errno == TRY_AGAIN)
    anubis_error (EXIT_FAILURE, 0,
		  _("%s: a temporary name server error occured. Try again later."),
		  host);
  else
    anubis_error (EXIT_FAILURE, 0, _("%s: unknown DNS error %d."), host, h_errno);
}

/* EOF */
