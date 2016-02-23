/*
 *  config.h
 *    Parse configuration file used by this program.
 *    Copyright (C) 2015 Linus Styrén
 *****************************************************************************
 *  This file is part of Fågelmataren:
 *    https://github.com/Linkaan/Fagelmatare/
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *****************************************************************************
 */

#ifndef CONFIG_H
#define CONFIG_H

#define DELIM " \n"

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

int get_config(char *filename, struct config *configuration);
void free_config(struct config *configuration);
#endif
