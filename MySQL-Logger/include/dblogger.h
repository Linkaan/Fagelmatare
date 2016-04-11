/*
 *  dblogger.h
 *    Library to send error/info logs to Fågelmatare Server
 *****************************************************************************
 *  This file is part of Fågelmataren, an advanced bird feeder equipped with
 *  many peripherals. See <https://github.com/Linkaan/Fagelmatare>
 *  Copyright (C) 2015-2016 Linus Styrén
 *
 *  Fågelmataren is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the Licence, or
 *  (at your option) any later version.
 *
 *  Fågelmataren is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public Licence for more details.
 *
 *  You should have received a copy of the GNU General Public Licence
 *  along with Fågelmataren.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************
 */

#ifndef DBLOGGER_H
#define DBLOGGER_H

#include "log_entry.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int connect_to_database(const char *address, const char *user, const char *pwd);

extern int log_to_database(log_entry *ent);

extern int disconnect(void);

#ifdef __cplusplus
}
#endif

#endif
