/* Hey emacs, show me this like 'c': -*- c -*-

 * xml-rpc-gen: a protocol compiler for the XDL definition language
 * Copyright (C) 2025 Advanced Software Production Line, S.L.
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

#ifndef __XML_RPC_C_STUB_H__
#define __XML_RPC_C_STUB_H__

#include <xml-rpc.h>

void     xml_rpc_support_write_function_type_prefix (axlNode * params);

char   * xml_rpc_support_get_function_type_prefix   (axlNode * params);

void     xml_rpc_support_write_function_parameters  (axlDoc * doc, 
						     axlNode * params);

axl_bool xml_rpc_c_stub_create                      (axlDoc  * doc, 
						     char    * out_dir, 
						     char    * comp_name);

void     xml_rpc_c_stub_write_native_type           (axlDoc     * doc, 
						     const char * type,
						     int          same_line);

axl_bool xml_rpc_c_stub_type_is_array               (axlDoc     * doc, 
						     const char * type);

axl_bool xml_rpc_c_stub_type_is_struct              (axlDoc     * doc, 
						     const char * type);

void     xml_rpc_c_stub_write_type_header           (char   * result, 
						     char   * comp_name, 
						     axlDoc * doc);

void     xml_rpc_c_stub_write_all_struct_and_array_defs (axlDoc * doc, 
							 char   * comp_name, 
							 char   * result);

#endif
