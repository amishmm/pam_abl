/*
 *   pam_abl - a PAM module and program for automatic blacklisting of hosts and users
 *
 *   Copyright (C) 2005-2012
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LOG_H
#define LOG_H

#include "config.h"
#include <security/pam_appl.h>

typedef struct log_context {
    short debug;
} log_context;

/*
  Create an empty log context that can be used with the functions below
*/
log_context *createLogContext();

/*
  release all the resources occupied by the given context
  After calling this function do not use the context ptr anymore
*/
void destroyLogContext(log_context *context);

/*
  Log a system error. This will also lookup the string representation of err
  Make sure err is a value specified in errno.h
*/
void log_sys_error(int err, const char *what);


/*
  Log an informational message
*/
void log_info(const char *format, ...);

/*
  Log a normal error message
*/
void log_error(const char *format, ...);

/*
  Log a normal warning message
*/
void log_warning(const char *format, ...);

/*
  If debugging output is requested, write the given message out
*/
void log_debug(const char *format, ...);

#if !defined(TOOLS)
void log_pam_error(pam_handle_t *handle, int err, const char *what);
#endif

extern int log_quiet_mode;

#endif
