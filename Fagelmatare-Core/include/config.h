/*
 *  config.h
 *    Parse configuration file used by this program.
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

#ifndef CONFIG_H
#define CONFIG_H

#define DELIM " \n"

// configuration struct used to store data in configuration file
struct config {
 char *serv_addr;
 char *username;
 char *passwd;
 char *fagelmatare_log;
 char *sock_path;
 char *state_path;
 char *start_hook;
 char *stop_hook;
 char *subtitle_hook;
 int pir_input;
};

/*
 * Read configuration file into configuration struct
 */
int get_config(char *filename, struct config *configuration);

/*
 * Destroys resources used by configuration library
 */
void free_config(struct config *configuration);

#endif
