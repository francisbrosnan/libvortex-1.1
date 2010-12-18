/* 
 *  LibVortex:  A BEEP (RFC3080/RFC3081) implementation.
 *  Copyright (C) 2010 Advanced Software Production Line, S.L.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307 USA
 *  
 *  You may find a copy of the license under this software is released
 *  at COPYING file. This is LGPL software: you are welcome to develop
 *  proprietary applications using this library without any royalty or
 *  fee but returning back any change, improvement or addition in the
 *  form of source code, project image, documentation patches, etc.
 *
 *  For commercial support on build BEEP enabled solutions contact us:
 *          
 *      Postal address:
 *         Advanced Software Production Line, S.L.
 *         C/ Antonio Suarez Nº 10, 
 *         Edificio Alius A, Despacho 102
 *         Alcalá de Henares 28802 (Madrid)
 *         Spain
 *
 *      Email address:
 *         info@aspl.es - http://www.aspl.es/vortex
 */

#ifndef __VORTEX_CONNECTION_PRIVATE_H__
#define __VORTEX_CONNECTION_PRIVATE_H__

/** 
 * @internal
 * @brief Internal VortexConnection representation.
 */
struct _VortexConnection {
	/** 
	 * @brief Vortex context where the connection was created.
	 */
	VortexCtx  * ctx;

	/** 
	 * @brief Vortex Connection unique identifier.
	 */
	int          id;

	/** 
	 * @brief Host name this connection is actually connected to.
	 */
	char       * host;

	/** 
	 * @brief Port this connection is actually connected to.
	 */
	char       * port;

	/** 
	 * @brief Contains the local address that is used by this connection.
	 */
	char       * local_addr;
	/** 
	 * @brief Contains the local port that is used by this connection.
	 */
	char       * local_port;

	/** 
	 * @brief Allows to hold and report current connection status.
	 */
	axl_bool     is_connected;

	/** 
	 * @brief App level message to report current connection
	 * status using a textual message.
	 */
	char       * message;
	/** 
	 * @brief Error code to report to the application.
	 */
	VortexStatus status;

	/** 
	 * @brief Underlying transport descriptor.
	 * This is actually the network descriptor used to send and
	 * received data.
	 */
	VORTEX_SOCKET  session;

	/** 
	 * @brief Allows to configure if the given session (actually
	 * the socket) associated to the given connection should be
	 * closed once __vortex_connection_set_not_connected function.
	 */
	axl_bool       close_session;

	/**
	 * Profiles supported by the remote peer.
	 */
	axlList      * remote_supported_profiles;

	/**
	 * A list of installed masks.
	 */ 
	axlList      * profile_masks;
	/** 
	 * Mutex used to protect profile mask mutex.
	 */
	VortexMutex    profile_masks_mutex;

	/** 
	 * Channels already created inside the given VortexConnection.
	 */
	VortexHash * channels;
	/** 
	 * Hash to hold miscellaneous data, sometimes used by the
	 * Vortex Library itself, but also exposed to be used by the
	 * Vortex Library programmers through the functions:
	 *   - \ref vortex_connection_get_data 
	 *   - \ref vortex_connection_set_data
	 */
	VortexHash * data;
	/** 
	 * @brief Keeps track for the last channel created on this
	 * session. The connection role modifies the way channel
	 * number are allocated. In the case the role is initiator,
	 * client role, odd channel numbers are generated. In the case
	 * the role is listener, server role, even channel numbers are
	 * generated.
	 */
	int          last_channel;

	/** 
	 * @brief Remote features supported/requested.
	 *
	 * This variable holds possible features content set at the
	 * greetings element received from the remote peer.
	 */
	char       * features;

	/** 
	 * @brief Remote localize supported/requested.
	 * 
	 * This variable holds possible localization values the remote
	 * peer is requesting.
	 */
	char       * localize;

	/** 
	 * @brief The channel_mutex
	 *
	 * This mutex is used at vortex_connection_get_next_channel to
	 * avoid race condition of various threads calling in a
	 * reentrant way on this function getting the same value.
	 * 
	 * Although this function use vortex_hash_table, a
	 * thread-safe hash table, this is not enough to ensure only
	 * one thread can execute inside this function.
	 * 
	 * The vortex_connection_add_channel adds a channel into a
	 * existing connection. Inside the implementation, this
	 * function first look up if channel already exists which means
	 * a program error and the insert the new channel if no
	 * channel was found.
	 *
	 * This mutex is also used on that function to avoid re-entrant
	 * conditions on the same function and between function which
	 * access to channel hash table.
	 */
	VortexMutex     channel_mutex;

	/** 
	 * @brief The ref_count.
	 *
	 * This enable a vortex connection to keep track of the
	 * reference counting.  The reference counting is controlled
	 * thought the vortex_connection_ref and
	 * vortex_connection_unref.
	 * 
	 */
	int  ref_count;
	
	/** 
	 * @brief The ref_mutex
	 * This mutex is used at vortex_connection_ref and
	 * vortex_connection_unref to avoid race conditions especially
	 * at vortex_connection_unref. Because several threads can
	 * execute at the same time this unref function this mutex
	 * ensure only one thread will execute the vortex_connection_free.
	 * 
	 */
	VortexMutex  ref_mutex;

	/** 
	 * @brief The op_mutex
	 * This mutex allows to avoid race condition on operating with
	 * the same connection from different threads.
	 */
	VortexMutex op_mutex;
	
	/** 
	 * @brief The handlers mutex
	 *
	 * This mutex is used to secure code sections destined to
	 * handle handlers stored associated to the connection.
	 */
	VortexMutex handlers_mutex;

	/**
	 * @brief The channel_pool_mutex
	 * This mutex is used by the vortex_channel_pool while making
	 * operations with channel pool over this connection. This is
	 * necessary to make mutually exclusive the function from that
	 * module while using the same connection.
	 */
	VortexMutex channel_pool_mutex;

	/**
	 * @brief The next_channel_pool_id
	 * This value is used by the channel pool module to hold the
	 * channel pool identifiers that will be used.
	 */
	int  next_channel_pool_id;

	/**
	 * @brief The channel_pools
	 * This hash actually holds every channel pool created over
	 * this connection. Every channel pool have an id that unique
	 * identifies the channel pool from each over inside this
	 * connection and it is used as key.
	 */
	VortexHash * channel_pools;

	/** 
	 * @brief The peer role
	 * 
	 * This is used to know current connection role.	
	 */
	VortexPeerRole role;

	/** 
	 * @brief Writer function used by the Vortex Library to actually send data.
	 */
	VortexSendHandler    send;

	/** 
	 * @brief Writer function used by the Vortex Library to actually received data
	 */
	VortexReceiveHandler receive;

	/** 
	 * @brief On close handler
	 */
	axlList * on_close;

	/** 
	 * @brief On close handler, extended version.
	 */
	axlList * on_close_full;

	/** 
	 * @brief Internal reference to the serverName value.
	 */
	char    * serverName;

	/** 
	 * @brief Stack storing pending channel errors found.
	 */ 
	axlStack * pending_errors;

	/** 
	 * @brief Mutex used to open the pending errors list.
	 */
	VortexMutex pending_errors_mutex;

	/** 
	 * @brief Two lists that allows keeping reference to handlers
	 * to be called once channels are added removed.
	 */
	axlList  * add_channel_handlers;
	axlList  * remove_channel_handlers;

	/** 
	 * @brief Mutex used to protect previous lists.
	 */
	VortexMutex channel_update_mutex;

	/** 
	 * @internal Handler used to decide how to split frames.
	 */
	VortexChannelFrameSize  next_frame_size;
	
	/** 
	 * @internal Reference to user defined pointer to be provided
	 * to the function once executed.
	 */
	axlPointer              next_frame_size_data;
	
	/** 
	 * @internal Reference to implement connection I/O block.
	 */
	axl_bool                is_blocked;

	/** 
	 * @internal Value to track connection activity.
	 */
	long                    last_idle_stamp;

	/** 
	 * @internal Value to track now many bytes has being received
	 * on this connection.
	 */
	long                    bytes_received;

	/** 
	 * @internal Value that makes the connection to be
	 * unwatched from the reader process.
	 */
	axl_bool                reader_unwatch;

	/** 
	 * @internal Value to signal initial accept stage associated
	 * to a connection in the middle of the greetings.
	 */
	axl_bool                initial_accept;

	/** 
	 * @internal Reference to a line that wasn't totally read when
	 * call vortex_frame_readline.
	 */ 
	char                  * pending_line;

	/** 
	 * @internal Value that signals if seq frame emision is
	 * disabled.
	 */
	axl_bool                seq_frames_disabled;

	/** 
	 * @internal Pointer to the currently configured pre accept
	 * handler.
	 */ 
	VortexConnectionOnPreRead    pre_accept_handler;

	/** 
	 * @internal Pointer used to store the buffer that is holding
	 * the content of the next frame.
	 */
	char                       * buffer;
	
	/** 
	 * @internal Pointer to the last frame being read at the
	 * connection.
	 */
	VortexFrame                * last_frame;

	/** 
	 * @internal Variable that is used by vortex_frame_get_next to
	 * track status for partial reads.
	 */
	int                          remaining_bytes;
	int                          bytes_read;
};

#endif /* __VORTEX_CONNECTION_PRIVATE_H__ */
