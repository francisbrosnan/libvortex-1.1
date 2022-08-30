/* Hey emacs, show me this like 'c': -*- c -*-
 *
 * xml-rpc-gen: a protocol compiler for the XDL definition language
 * Copyright (C) 2022 Advanced Software Production Line, S.L.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 */
#ifndef __XML_RPC_AUTOCONF_C_STUB_CREATE_H__
#define __XML_RPC_AUTOCONF_C_STUB_CREATE_H__

#include <xml-rpc.h>

void xml_rpc_autoconf_c_stub_create (axlDoc * doc, 
				     char   * out_dir, 
				     char   * comp_name);

void xml_rpc_autoconf_c_server_create (axlDoc * doc,
				       char   * out_dir,
				       char   * comp_name);

#endif
