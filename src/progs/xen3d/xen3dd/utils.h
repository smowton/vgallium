/*\
 *  Copyright (C) International Business Machines  Corp., 2005
 *  Author(s): Anthony Liguori <aliguori@us.ibm.com>
 *
 *  Xen Console Daemon
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 2 of the License.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
\*/

#ifndef __UTILS_H__
#define __UTILS_H__

#include <syslog.h>
#include <stdio.h>

#if 1
#define dolog(val, fmt, ...) do {				\
	if ((val) == LOG_ERR)					\
		fprintf(stderr, fmt "\n", ## __VA_ARGS__);	\
	syslog(val, fmt, ## __VA_ARGS__);			\
} while (0)
#else
#define dolog(val, fmt, ...) fprintf(stderr, fmt "\n", ## __VA_ARGS__)
#endif

#endif
