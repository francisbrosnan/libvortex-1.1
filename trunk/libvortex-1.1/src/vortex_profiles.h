/*
 *  LibVortex:  A BEEP (RFC3080/RFC3081) implementation.
 *  Copyright (C) 2005 Advanced Software Production Line, S.L.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of 
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307 USA
 *  
 *  You may find a copy of the license under this software is released
 *  at COPYING file. This is LGPL software: you are welcome to
 *  develop proprietary applications using this library without any
 *  royalty or fee but returning back any change, improvement or
 *  addition in the form of source code, project image, documentation
 *  patches, etc. 
 *
 *  For commercial support on build BEEP enabled solutions contact us:
 *          
 *      Postal address:
 *         Advanced Software Production Line, S.L.
 *         C/ Dr. Michavila N� 14
 *         Coslada 28820 Madrid
 *         Spain
 *
 *      Email address:
 *         info@aspl.es - http://fact.aspl.es
 */
#ifndef __VORTEX_PROFILES_H__
#define __VORTEX_PROFILES_H__

#include <vortex.h>


void     vortex_profiles_register                (const char            * uri,
						  VortexOnStartChannel    start,
						  axlPointer              start_user_data,
						  VortexOnCloseChannel    close,
						  axlPointer              close_user_data,
						  VortexOnFrameReceived   received,
						  axlPointer              received_user_data);

void     vortex_profiles_unregister              (const char            * uri);

void     vortex_profiles_set_mime_type           (const char            * uri,
						  const char            * mime_type,
						  const char            * transfer_encoding);

const char   * vortex_profiles_get_mime_type           (const char  * uri);

const char   * vortex_profiles_get_transfer_encoding   (const char  * uri);

void      vortex_profiles_register_extended_start (const char                         * uri,
						   VortexOnStartChannelExtended   extended_start,
						   axlPointer                     extended_start_user_data);

bool      vortex_profiles_invoke_start            (char  * uri, int  channel_num, VortexConnection * connection,
						   char  * serverName, char  * profile_content, 
						   char  ** profile_content_reply, VortexEncoding encoding);

bool      vortex_profiles_is_defined_start        (const char  * uri);

bool      vortex_profiles_invoke_close            (char  * uri,
						   int  channel_nu,
						   VortexConnection * connection);

bool      vortex_profiles_is_defined_close        (const char  * uri);

bool      vortex_profiles_invoke_frame_received   (char             * uri,
						   int                channel_num,
						   VortexConnection * connection,
						   VortexFrame      * frame);

bool      vortex_profiles_is_defined_received     (const char  * uri);

axlList * vortex_profiles_get_actual_list         ();

axlList * vortex_profiles_get_actual_list_ref     ();

int       vortex_profiles_registered              ();

bool      vortex_profiles_is_registered           (const char  * uri);

void      vortex_profiles_init                    (VortexCtx   * ctx);

void      vortex_profiles_cleanup                 (VortexCtx   * ctx);


#endif
