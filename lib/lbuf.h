/* GNU Mailutils -- a suite of utilities for electronic mail
   Copyright (C) 2003 Free Software Foundation, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA  */

#include <mailutils/types.h>

struct _line_buffer;

int _auth_lb_create (struct _line_buffer **s);
void _auth_lb_destroy (struct _line_buffer **s);
void _auth_lb_drop (struct _line_buffer *s);

int _auth_lb_grow (struct _line_buffer *s, const char *ptr, size_t size);
int _auth_lb_read (struct _line_buffer *s, const char *ptr, size_t size);
int _auth_lb_readline (struct _line_buffer *s, const char *ptr,
		       size_t size);
int _auth_lb_writelines (struct _line_buffer *s, const char *iptr,
			 size_t isize,
			 int (*wr) (void *data, char *start, char *end),
			 void *data, size_t *nbytes);
int _auth_lb_level (struct _line_buffer *s);
char *_auth_lb_data (struct _line_buffer *s);

