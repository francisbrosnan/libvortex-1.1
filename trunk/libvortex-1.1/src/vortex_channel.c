/* 
 *  LibVortex:  A BEEP (RFC3080/RFC3081) implementation.
 *  Copyright (C) 2008 Advanced Software Production Line, S.L.
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

#define LOG_DOMAIN "vortex-channel"

/** 
 * @internal Keys used to check channel features.
 */
#define VORTEX_CHANNEL_WAIT_REPLY           "ch:wa:rp"
#define VORTEX_CHANNEL_CLOSE_HANDLER_CALLED "ch:co:ha:ca"

#include <vortex_channel.h>

/* local include */
#include <vortex_ctx_private.h>

/** 
 * @internal
 *
 * Here is the VortexChannel data structure. It is an opaque structure
 * handled by a set of functions to ensure data hiding principle and
 * to be easily bindable.
 *
 * If you modify this structure you are likely to be interested on \ref
 * vortex_channel_new_emtpy.
 */
struct _VortexChannel {
	/**
	 * @internal reference to the ctx under which the channel was
	 * created (which is the context under which the connection
	 * was created)
	 */
	VortexCtx             * ctx;
	int                     channel_num;

	int                     last_message_sent;
	int                     last_message_received;

	/** 
	 * @internal Internal counter which tracks the last reply
	 * number used to send a reply. It is not actually the last
	 * reply fully sent (prepared and written into the wire). See
	 * last_reply_written.
	 */
	int                     last_reply_sent;
	int                     last_reply_expected;

	/** 
	 * @internal Counter which tracks the last reply written to
	 * the wire. 
	 */
	int                     last_reply_written;
	
	/** 
	 * @internal Internal counter used to manage replies that we
	 * have received and we have fully delivered into the
	 * application user space.
	 */
	int                     last_reply_delivered;
	int                     last_msg_delivered;
	int                     last_ans_delivered;
	int                     last_ans_seqno_delivered;

	int                     last_seq_no;
	int                     last_seq_no_expected;

	int                     max_seq_no_accepted;
	int                     max_seq_no_remote_accepted;
	axlList               * pending_messages;

	int                     last_ansno_sent;
	int                     last_ansno_expected;

	/** 
	 * Channel reference counting support. See \ref
	 * vortex_channel_ref and \ref vortex_channel_unref.
	 */
	int                     ref_count;

	/** 
	 * Channel reference counting support mutex. This is used at
	 * \ref vortex_channel_ref and \ref vortex_channel_unref.
	 */
	VortexMutex             ref_mutex;

	/* the profile:
	 * 
	 * Defines the profile under the channel is working. */
	char                  * profile;
	
	/** 
	 * mime type:
	 *
	 * This variable is used to control which is the default mime
	 * type used on the given channel. If the mime type is the
	 * default one established by the BEEP RFC the mime type is
	 * then omitted. In the case it is different from the default,
	 * it is used automatically while creating frames.
	 */
	const char           * mime_type;
	
	/** 
	 * transfer encoding:
	 * ~~~~~~~~~~~~~~~~~~
	 *
	 * This variable is used to control which is the default
	 * transfer encoding value for the given channel. Again, if
	 * the transfer encoding used is different from the default,
	 * then it is placed a notification for the value used while
	 * building frames.
	 */
	const char            * transfer_encoding;

	VortexHash            * data;

	/* is_opened
	 *
	 * This value allows to detect if a channel is closed and
	 * cannot be used anymore for any type of transmission. This
	 * value is actually used by vortex_channel_send_msg,
	 * __vortex_channel_common_rpy. */
	bool                    is_opened;

	
	/* being_closed
	 * 
	 * These values is similar to is_opened but allows a different
	 * error detection. While a channel is closed (a process that
	 * can take a while) BEEP RFC 3080 says no msg can be sent but
	 * more msg can be received and that msg must be reply so,
	 * once a channel is requested to be closed, it can still
	 * receive more msg and must be replied. This value is actually
	 * used by vortex_channel_send_msg. */
	bool                    being_closed;


	/* window_size
	 *
	 * This value is used to control which is the size to
	 * advertise once a frame is received and a SEQ frame should
	 * be generated. The default value is 4096, which means that
	 * the window size advertised will 4096 plus the seq_no
	 * value. */
	int                     window_size;

	/* desired_window_size
	 *
	 * This value tracks changes the client has requested in the
	 * window_size. The actual window_size will not be adjusted until
	 * the current window fills up and a new SEQ frame is sent. */
	int                     desired_window_size;
	
	bool                    complete_flag;
	axlList               * previous_frame;

	/* connection associated to the channel */
	VortexConnection      * connection;

	/* close handlers for the channel */
	VortexOnCloseChannel    close;
	axlPointer              close_user_data;

	/* closed handlers for the channel */
	VortexOnClosedChannel   closed;
	axlPointer              closed_data;

	/* frame received handler */
	VortexOnFrameReceived   received;
	axlPointer              received_user_data;

	/* close notify handlers for the channel */
	VortexOnNotifyCloseChannel close_notify;
	axlPointer                 close_notify_user_data;

	/* the waiting_msgno
	 * 
	 * This thread safe queue is mainly used by
	 * vortex_channel_wait_reply and
	 * vortex_channel_invoke_received_handler. A detailed
	 * explanation can be found on at
	 * vortex_channel_invoke_received_handler. 
	 *
	 * As a brief, this waiting queue contains all waiting threads
	 * that are waiting to receive a reply no matter what type:
	 * ERR, RPY, ANS and NUL. */
	VortexQueue           * waiting_msgno;

	/* the send mutex:	
	 *
	 * This mutex is used to make the function
	 * vortex_channel_send_msg to be a critical section. This
	 * avoid race conditions on getting actual message number to
	 * use on other frame header while building and sending a
	 * message.
	 *
	 * This mutex it is also used by __vortex_channel_common_rpy
	 * which is a common implementation for err and rpy
	 * replies. Due to seqno BEEP specification, this value is
	 * shared among all message sent, no matter the type of the
	 * frame actually send, ERR, RPY MSG, ANS or NUL.
	 * 
	 * So this mutex allows to make critical section and mutually
	 * exclusive each function that send one of the previous
	 * message types. */
	VortexMutex             send_mutex;


	/* the receive_mutex:
	 *
	 * This mutex is used to make mutually exclusive some parts of
	 * the function vortex_channel_invoke_receive_handler and
	 * vortex_channel_wait_reply. 
	 * 
	 * The wait_reply function it is used to wait blocked for a
	 * specific reply to a msgno so it creates a async queue and
	 * insert it into hash_table that can be used by vortex reader
	 * thread to be able to figure out if there are a waiting
	 * thread or can directly execute the frame received handler.
	 *
	 * The vortex reader also use this mutex through the function
	 * vortex_channel_lock_to_receive and
	 * vortex_channel_unlock_to_receive so it can
	 */
	VortexMutex            receive_mutex;

	/* the queue_msgs:
	 *
	 * The queue_msgs is used by the vortex sequencer process as a
	 * buffer of frames to be sent.  The sequencer receive request
	 * of messages to be sent. Then, them are splitted if the
	 * message is larger than channel window size into one or
	 * several frames ready to sent.
	 *
	 * Once the sequencer have queue some frames to be sent it
	 * signal the vortex writer to sent pending frames. */
	VortexQueue           * queue_msgs;


	/* the queue_seqs:
	 *
	 * The queue_seqs is used to organize all SEQ frames to be
	 * sent having a greater priority. This queue and the
	 * queue_msgs are internally handled by the vortex library.
	 */
	VortexQueue           * queue_seqs;


	/* the close_cond:
	 *
	 * This conditional is used to get blocked close message
	 * process and is response. As described in RFC 3080 a channel
	 * can receive the channel close message at any time but, must
	 * still await to receive its MSG replies.
	 * 
	 * Once all replies have been received the channel can be
	 * closed and the ok or error message will be sent. */
	VortexCond              close_cond;

	
	/* the close_mutex:
	 *
	 * This mutex is used in conjunction with close_cond to ensure
	 * all message replies have been received during the close
	 * channel process. */
	VortexMutex           close_mutex;


	/* the waiting_replies
	 *
	 * This variable is used by the
	 * vortex_channel_0_frame_received_close_msg and
	 * __vortex_channel_block_until_replies_are_received to flag a
	 * channel that have been blocked on that function because it
	 * didn't still receive all replies to message already
	 * sent. */
	bool                   waiting_replies;

	/* the reply_processed:
	 *
	 * This variable is used by the vortex reader and close
	 * channel process to synchronize channel frame replies
	 * reception and its process and the possible on going channel
	 * closing. 
	 * 
	 * A channel can be closed until all replies to all message
	 * are receive and they are delivered to application level. */
	bool                   reply_processed;


	/* the being_sending */
	bool                   being_sending;

	/* the pending_mutex 
	 *
	 * This mutex is used to ensure we have sent all replies to
	 * all message receive during the close process before getting
	 * blocked waiting for the ok message.  This mutex is used at
	 * __vortex_channel_block_until_replies_are_sent and
	 * vortex_channel_signal_reply_sent_on_close_blocked */
	VortexMutex            pending_mutex;

	/* the pending_cond
	 *
	 * The conditional variable is used in conjunction with
	 * previous mutex pending_mutex. */
	VortexCond             pending_cond;

	/** 
	 * @internal Allows to serialize replies received on frame
	 * received for a particular channel. Because frame delivery
	 * is done using threads by default, this could be required to
	 * enforce a serial deliver order when frames are acknoledge
	 * on second level handlers.
	 */
	VortexMutex            serialize_msg_mutex;
	VortexMutex            serialize_rpy_mutex;
	VortexMutex            serialize_ans_mutex;
	bool                   serialize;

	/**
	 * @internal Hashes used to store temporaly messages and
	 * replies that must be hold until the proper message or reply
	 * is received.
	 */
	axlHash              * serialize_msg;
	axlHash              * serialize_rpy;
	axlHash              * serialize_ans;

	/* the pool
	 *
	 * If the channel was created inside a pool this variable will
	 * be set to point to the channel it belongs so on close
	 * operation this module will be able to notify vortex channel
	 * pool module to remove the channel reference. */
	VortexChannelPool     * pool;

	/** 
	 * @internal Hash storing replies that were found out of its
	 * corresponding order that are hold until its time is found.
	 */
	axlHash               * stored_replies;

	/** 
	 * @internal Handler used to decide how to split frames.
	 */
	VortexChannelFrameSize  next_frame_size;
	
	/** 
	 * @internal Reference to user defined pointer to be provided
	 * to the function once executed.
	 */
	axlPointer              next_frame_size_data;
};

typedef struct _VortexChannelData {
	VortexConnection        * connection;
	int                       channel_num;
	VortexOnChannelCreated    on_channel_created;
	char                    * serverName;
	char                    * profile;
	VortexEncoding            encoding;
	char                    * profile_content;
	int                       profile_content_size;
	axlPointer                user_data;
	VortexOnCloseChannel      close;
	axlPointer                close_user_data;
	VortexOnFrameReceived     received;
	axlPointer                received_user_data;
	bool                      threaded;
}VortexChannelData;

/** 
 * @internal
 *
 * @brief Free resources allocated by the structure used to create new
 * channels.
 * 
 * @param data The VortexChannelData to be destroyed.
 */
void vortex_channel_data_free (VortexChannelData * data)
{
	if (data == NULL)
		return;
	if (data->serverName)
		axl_free (data->serverName);
	if (data->profile)
		axl_free (data->profile); 
	if (data->profile_content)
		axl_free (data->profile_content);
	axl_free (data);
	
	return;
}

/** 
 * @internal
 *
 * @brief Allows to calculate which is the current size of the mime
 * headers that are placed for every message sent inside Vortex
 * Library.
 *
 * Because mime is handled per profile and them per channel, next
 * sequence number to be calculated must consider the size of the mime
 * headers plus the size of the message being sent.
 * 
 * @param channel The channel where the mime header size will be
 * reported.
 */
int  __vortex_channel_get_mime_headers_size (VortexChannel * channel)
{
#if defined(ENABLE_VORTEX_LOG)
	VortexCtx       * ctx               = vortex_channel_get_ctx (channel);
#endif
	/* mime configuration */
	const char      * mime_type         = vortex_channel_get_mime_type (channel);
	const char      * transfer_encoding = vortex_channel_get_transfer_encoding (channel);
	int               size              = 0;

	/* check for Content-Type header configuration */
	if (mime_type != NULL && !axl_cmp (mime_type, "application/octet-stream")) {
		/* Seems that the mime entity header content-type is
		 * defined so, count 16 bytes for the "Content-Type: "
		 * plus 2 bytes more due to ending \x0D\x0A. */
		size           = 16 + strlen (mime_type);
	}

	/* check for the Content-Transfer-Encoding header configuration */
	if (transfer_encoding != NULL && !axl_cmp (transfer_encoding, "binary")) {
		size          += 29 + strlen (transfer_encoding);
	}
	
 	/* add 2 more bytes for the ending \x0D\x0A MIME body
 	 * separation */
 	size += 2;

	vortex_log (VORTEX_LEVEL_DEBUG, "mime headers size calculated for channel %d was: %d",
	       channel->channel_num, size);
	
	/* return the result calculated */
	return size;
}

/** 
 * @internal
 *
 * Returns current mime headers that should be added to the message
 * being sent according to the channel that is goint to carry the
 * data.
 * 
 * @param channel The channel where the mime header configuration will
 * be consulted to get current mime headers.
 *
 * @param buffer The buffer to use to report current mime header
 * configuration.
 * 
 */
void __vortex_channel_get_mime_headers (VortexChannel * channel, char  * buffer)
{
	/* mime configuration */
	const char  * mime_type         = vortex_channel_get_mime_type (channel);
	const char  * transfer_encoding = vortex_channel_get_transfer_encoding (channel);
	int           size              = 0;

	/* check for Content-Type header configuration */
	if (mime_type != NULL && !axl_cmp (mime_type, "application/octet-stream")) {
		/* add mime Content-Type: entity header */
		memcpy (buffer, "Content-Type: ", 14);
		
		/* add mime type value */
		memcpy (buffer + 14, mime_type, strlen (mime_type));

		/* add trailing \x0D\x0A value */
		memcpy (buffer + 14 + strlen (mime_type), "\x0D\x0A", 2);

		/* get current size dumped */
		size           = 16 + strlen (mime_type);
	}

	/* check for the Content-Transfer-Encoding header configuration */
	if (transfer_encoding != NULL && !axl_cmp (transfer_encoding, "binary")) {
		/* add mime Content-Type: entity header */
		memcpy (buffer + size, "Content-Transfer-Encoding: ", 27);
		
		/* add mime type value */
		memcpy (buffer + size + 27, mime_type, strlen (mime_type));

		/* add trailing \x0D\x0A value */
		memcpy (buffer + size + 27 + strlen (mime_type), "\x0D\x0A", 2);

		size          += 29 + strlen (transfer_encoding);
	}
	
	/* add MIME header ending \x0D\x0A */
	memcpy (buffer + size, "\x0D\x0A", 2);

	/* mime header built */
	return;
	
}

/**
 * \defgroup vortex_channel Vortex Channel: Related function to create and manage Vortex Channels.
 */

/**
 * \addtogroup vortex_channel
 * @{
 */

/** 
 * @internal
 * @brief Set channel status to be connected.
 *
 * Private function to set channel connected status to true. The
 * channel connection status is used to know if a channel is actually
 * usable to send or receive data. This avoids using a channel that is
 * being created or closed but that process didn't finished yet.
 * 
 * @param channel the channel to operate.
 */
void __vortex_channel_set_connected (VortexChannel * channel)
{
	if (channel == NULL)
		return;

	channel->is_opened = true;

	return;
}

typedef struct _VortexStartReplyCache {
	char        * profile_content;
	VortexFrame * frame;
} VortexStartReplyCache;

void __vortex_channel_start_reply_free (VortexStartReplyCache * cache)
{
	/* do nothing if null reference is found */
	if (cache == NULL)
		return;

	axl_free (cache->profile_content);
	vortex_frame_free (cache->frame);
	axl_free (cache);

	return;

} /* end __vortex_channel_start_reply_free */

bool     __vortex_channel_validate_start_reply (VortexFrame * frame, char  * _profile, VortexChannel * channel)
{
	VortexCtx   * ctx = vortex_channel_get_ctx (channel);

	/* xml document variable declaration */
	axlDoc      * doc = NULL;
	axlNode     * node;
	axlError    * error;
	
	/* application variables */
	const char            * profile;
	const char            * profile_content = NULL;
	bool                    result;
	VortexStartReplyCache * cache = NULL;

	/* if no context no validation */
	if (ctx == NULL)
		return false;

	/* get the cache */
	vortex_mutex_lock (&ctx->channel_start_reply_cache_mutex);
	cache = axl_hash_get (ctx->channel_start_reply_cache, (axlPointer) vortex_frame_get_payload (frame));
	vortex_mutex_unlock (&ctx->channel_start_reply_cache_mutex);

	/* try to match the start reply with the cache */
	if (cache != NULL) {

		/* cache hit!, notify piggyback and return */
		result          = true;
		profile_content = cache->profile_content;

		/* free the frame */
		vortex_frame_free (frame);

		goto notify_start_validate_reply;

	} /* end if */

	vortex_log (VORTEX_LEVEL_DEBUG, "doing validate for start reply msg: '%s'", 
		    vortex_frame_get_payload (frame));

	/* parse xml document */
	doc = axl_doc_parse (vortex_frame_get_payload (frame), 
			     vortex_frame_get_payload_size (frame), &error);
	if (!doc) {
		/* drop a log */
		vortex_log (VORTEX_LEVEL_CRITICAL, 
		       "Unable to parse XML start channel reply document, unable to create the channel: %s",
		       axl_error_get (error));
		axl_error_free (error);
		       
		/* free resources */
		vortex_connection_remove_channel (channel->connection, channel);
		vortex_frame_unref (frame);
		return false;
	}

	/* check frame type response */
	if (vortex_frame_get_type (frame) != VORTEX_FRAME_TYPE_RPY) {
		vortex_log (VORTEX_LEVEL_CRITICAL, 
		       "received a negative reply to the start channel num=%d, profile=%s",
		       channel->channel_num, channel->profile);
		/* check the error received */
		node = axl_doc_get_root (doc);
		if (NODE_CMP_NAME (node, "error")) {
			/* get the error code and the content */
			vortex_connection_push_channel_error (
				/* the connection where to report */
				channel->connection, 
				/* the code to report */
				(int) (ATTR_VALUE (node, "code") != NULL ? vortex_support_strtod (ATTR_VALUE (node, "code"), NULL) : 0),
				/* the content to report */
				axl_node_get_content (node, NULL) != NULL ? axl_node_get_content_copy (node, NULL) : axl_strdup ("No error message reported by remote peer"));
		} /* end if */

		/* free resources */
		vortex_connection_remove_channel (channel->connection, channel);
		vortex_support_free (2, frame, vortex_frame_unref, doc, axl_doc_free);
		return false;
	}
	
	/* get the root element ( profile element ) */
	node = axl_doc_get_root (doc);
	if (! NODE_CMP_NAME (node, "profile")) {
		
		/* remove the channel */
		vortex_connection_remove_channel (channel->connection, channel);

		/* free resources */
		vortex_support_free (2, frame, vortex_frame_unref,
				     doc, axl_doc_free);
		return false;
	}
	/* check profile requested */
	profile = axl_node_get_attribute_value (node, "uri");
	if (! axl_cmp (profile, _profile)) {
		vortex_log (VORTEX_LEVEL_CRITICAL, 
		       "received a profile confirmation which is different from the requested=%s", profile);
		result = false;
	} else
		result = true;
	
	/* check if profile received is what we were expecting */
	if (result) {
		/* check for piggyback received */
		profile_content      = axl_node_get_content (node, NULL);

	notify_start_validate_reply:
		if (profile_content != NULL && strlen (profile_content) > 0) {
			/* log the profile received */
			vortex_log (VORTEX_LEVEL_DEBUG, "received profile content: '%s'", profile_content);
			
			/* set piggyback frame */
			vortex_channel_set_piggyback (channel, profile_content);
			
			/* it is not required to make a deallocation
			 * for the profile_content, because the
			 * reference received is not a copy */
		} /* end if */
	} /* end if */

	/* store the result in the case */
	if (cache == NULL) {
		vortex_mutex_lock (&ctx->channel_start_reply_cache_mutex);
		
		/* the cache */
		cache        = axl_new (VortexStartReplyCache, 1);

		/* store the profile content */
		if (profile_content)
			cache->profile_content = axl_strdup (profile_content);
		else
			cache->profile_content = NULL;

		/* store the frame */
		cache->frame = frame;

		/* store using as index the frame content */
		axl_hash_insert_full (ctx->channel_start_reply_cache,
				      /* pointer to the key and its destroy function */
				      (axlPointer) vortex_frame_get_payload (frame), NULL,
				      /* the value and its destroy function */
				      cache, (axlDestroyFunc) __vortex_channel_start_reply_free);

		vortex_mutex_unlock (&ctx->channel_start_reply_cache_mutex);
	} /* end if */

	/* free document */
	axl_doc_free (doc);
	
	if (!result) {
		/* remove the channel in the case something went wrong */
		vortex_connection_remove_channel (channel->connection, channel);
	}
	return result;
}


/** 
 * @internal
 *
 * Helper function for \ref vortex_channel_new.
 */
axlPointer __vortex_channel_new (VortexChannelData * data) 
{
	VortexChannel    * channel    = NULL;
	VortexChannel    * channel0   = NULL;
	VortexFrame      * frame      = NULL;
	VortexCtx        * ctx         = vortex_connection_get_ctx (data->connection);
	WaitReplyData    * wait_reply = NULL;
	char             * start_msg  = NULL;
	int                msg_no;
	bool               check_profiles = false;

	/* check if remote peer support actual profile */
	if (! vortex_conf_get (ctx, VORTEX_ENFORCE_PROFILES_SUPPORTED, &check_profiles)) {
		vortex_log (VORTEX_LEVEL_WARNING, "unable to get the current enforce profiles supported configuration");
		goto __vortex_channel_new_invoke_caller;
	} 

	/* according to previous configuration, check or not remote
	 * profiles supported */
	if (check_profiles && !vortex_connection_is_profile_supported (data->connection, data->profile)) {
		vortex_log (VORTEX_LEVEL_WARNING, "channel profile not supported by remote peer, channel can't be created");
		goto __vortex_channel_new_invoke_caller;
	} 

	/* check if channel to be created already exists */
	if ((data->channel_num != 0) && 
	    vortex_connection_channel_exists (data->connection, data->channel_num)) {
		vortex_log (VORTEX_LEVEL_CRITICAL, "creating a channel identified by %d that already exists",
		       data->channel_num);
		goto __vortex_channel_new_invoke_caller;
	}

	/* check if user wants to create a new channel and
	 * automatically allocate a channel number */
	if (data->channel_num == 0) {
		data->channel_num      = vortex_connection_get_next_channel (data->connection);
		if (data->channel_num == -1) {
			vortex_log (VORTEX_LEVEL_CRITICAL, 
			       "vortex connection get next channel have failed, this could be bad, I mean, really bad..");
			goto __vortex_channel_new_invoke_caller;
		}

	}
	vortex_log (VORTEX_LEVEL_DEBUG, "channel num returned=%d", data->channel_num);

	/* creates the channel */
	channel = vortex_channel_empty_new (data->channel_num, data->profile, data->connection);

	/* set default handlers */
	vortex_channel_set_close_handler    (channel, data->close, data->close_user_data);
	vortex_channel_set_received_handler (channel, data->received, data->received_user_data);
	
	/* get the channel 0 */
	channel0 = vortex_connection_get_channel (data->connection, 0);
	if (channel0 == NULL) {
		vortex_log (VORTEX_LEVEL_CRITICAL, "internal vortex error: unable to get channel 0 for a session");

		/* free allocated channel and exist */
		vortex_channel_unref (channel);
		channel = NULL;
		goto __vortex_channel_new_invoke_caller;
	}

	/* Add the channel to the connection to allow fast receive of
	 * frames. If the channel creation is not accepted just remove
	 * from the connection as usual.
	 *
	 * Now check profile associated to the channel
	 * requested. Check if user have forgot to register profile
	 * used. If does, register it for the user. */
	if (! vortex_profiles_is_registered (ctx, channel->profile)) {
		vortex_profiles_register (ctx, channel->profile,
					  /* use default start handler */
					  NULL, NULL, 
					  /* use default close handler */
					  NULL, NULL,
					  /* use no frame received handler */
					  NULL, NULL);
	} /* end if */
	
	/* add the channel created to the connection */
	vortex_connection_add_channel (data->connection, channel);

	/* create wait reply object. */
	wait_reply = vortex_channel_create_wait_reply ();

	/* build up the frame to send */
	start_msg  = vortex_frame_get_start_message (vortex_channel_get_number (channel),
						     data->serverName,
						     data->profile,
						     data->encoding,
						     data->profile_content,
						     data->profile_content_size);

	/* send start message */
	if (!vortex_channel_send_msg_and_wait (channel0, 
					       start_msg,
					       strlen (start_msg),
					       &msg_no, wait_reply)) {
		axl_free (start_msg);

		/* remove the channel and nullify */
		vortex_connection_remove_channel (data->connection, channel);
		channel = NULL;
		
		/* free pending data */
		vortex_channel_free_wait_reply (wait_reply);
		vortex_log (VORTEX_LEVEL_CRITICAL, "unable to send start message for channel %d and profile %s",
		       vortex_channel_get_number (channel),
		       vortex_channel_get_profile (channel));
		goto __vortex_channel_new_invoke_caller;
	}
	axl_free (start_msg);

	/* now, remote peer must accept channel creation or deny it
	 * this operation will block us until frame is received */
	frame      = vortex_channel_wait_reply (channel0, msg_no, wait_reply);
	if (frame == NULL) {
		vortex_log (VORTEX_LEVEL_CRITICAL, 
		       "something have failed while received start message response for channel %d under profile %s",
		       vortex_channel_get_number (channel), 
		       vortex_channel_get_profile (channel));

		/* remove the channel and nullify */
		vortex_connection_remove_channel (data->connection, channel);
		channel = NULL;
		goto __vortex_channel_new_invoke_caller;
	}
	vortex_log (VORTEX_LEVEL_DEBUG, "received reply to start message");

	/* check start reply data */
	if (!__vortex_channel_validate_start_reply (frame, channel->profile, channel)) {
		/* remove the channel and nullify */
		channel = NULL;
		goto __vortex_channel_new_invoke_caller;
	}

	vortex_log (VORTEX_LEVEL_DEBUG, "channel start reply validation stage finished");

	/* register serverName associated to the channel
	 * created. reached this point the channel was properly
	 * created */
	if (channel != NULL && channel->is_opened) {
		/* set the server name for the connection if not set
		 * before */
		vortex_connection_set_server_name (data->connection, data->serverName);
	} /* end if */

	__vortex_channel_new_invoke_caller:
	/* log a message if */
	if (channel == NULL) 
		vortex_log (VORTEX_LEVEL_CRITICAL, "channel=%d creation have failed", data->channel_num);
	
	/* finally, invoke caller with channel result */
	if (data->threaded) {
		if (channel) {
			/* ensure we do not lost the reference during the
			 * channel notification */
			vortex_channel_ref (channel);
			
			/* invoke the channel created handler */
			data->on_channel_created (channel->channel_num,
						  channel, 
						  data->user_data);
			
			/* if piggyback is defined, invoke the frame received
			 * using the piggyback */
			if (channel->is_opened && vortex_channel_have_piggyback (channel)) {
				/* get the piggyback */
				frame = vortex_channel_get_piggyback (channel);
				
				/* invoke the frame received */
				vortex_channel_invoke_received_handler (data->connection, channel, frame);
			}
			
			/* unref the channel here */
			vortex_channel_unref (channel);
		} else {
			/* notify null reference receievd */
			data->on_channel_created (-1, NULL, data->user_data);
		} /* end if */

		/* free no longer needed data */
		vortex_channel_data_free (data);
		return NULL;
	} /* end if */

	/* free no longer needed data */
	vortex_channel_data_free (data);
	return channel;
}

/** 
 * @brief Creates a new channel over the given connection.
 *
 * Creates a new channel over the session <i>connection</i> with the
 * channel num <i>channel_num</i> specified. If channel num is 0, the
 * next free channel number is used. The new channel will be created
 * under the terms of the profile defined. Both peers must support the
 * given profile, otherwise the channel will not be created.
 *
 * Because the channel is a <i>MSG/RPY</i> transaction between the
 * client and the server, it will block caller until channel is
 * created.
 * 
 * If you want to avoid getting blocked, you can use
 * \ref VortexOnChannelCreated "on_channel_created" handler. If you define this handler, the
 * function will unblock the caller and create the channel on a
 * separated thread. Once the channel is created, caller will be
 * notified through the callback defined by \ref VortexOnChannelCreated "on_channel_created".
 *
 * There are some events that happens during the channel life. They
 * are \ref VortexOnCloseChannel "close handler" and \ref VortexOnFrameReceived "received handler". They are
 * executed when the event they represent happens. A description for
 * each handler is the following:
 *
 * <ul>
 *
 * <li>
 * <i>close handler</i>: (see more info on vortex_handlers:
 * \ref VortexOnCloseChannel) This handler is executed when a channel close
 * petition have arrived from remote peer. 
 *
 * If you signal to close channel through \ref vortex_channel_close,
 * this handler is NOT executed, but it is executed when the given
 * channel receive a remote channel close request. This handler should
 * return true if channel can be closed or false if not.
 * 
 * This can be convenient if there are more data to be sent and to be
 * able to notify remote peer this is actually happening. 
 * 
 * This handler is optional. You can use NULL and the first level
 * handler for close event will be used. If the first level handler is
 * also not defined, a default handler will be used, which returns
 * always true. This means to agree to close the channel always.
 * 
 * You can also define user data to be passed into <i>close
 * handler</i> on its execution. This user data is
 * <i>close_user_data</i>.</li>
 * 
 * <li><i>received handler</i>: (see more info on vortex_handlers:
 * \ref VortexOnFrameReceived) This handler is executed when a channel
 * receive a frame. 
 * 
 * As the same as <i>close</i> handler definition, if you
 * provide a NULL value for this handler the first level handler will
 * be executed. If this first level handler is not defined, frame is
 * dropped.</li>
 *
 * </ul>
 * 
 * As you can see, there are 2 level of handlers to be executed when
 * events happens on channel life.  You can also see
 * \ref vortex_profiles_register documentation. Also check out
 * \ref vortex_manual_dispatch_schema "this section where is explained in more detail how frames are received". 
 * 
 * Later, to be able to close and free the channels created you must
 * use \ref vortex_channel_close. DO NOT USE \ref vortex_channel_free to close
 * or free channel resources. This function is actually called by
 * Vortex Library when the channel resources can be deallocated.
 *
 * On channel creation, it could happen an application do not register
 * the profile that is going to be used for the new channel. This is a
 * problem because if you don't define the close, start or frame
 * received many problems will appear. 
 * 
 * By default, if \ref vortex_channel_new detects you didn't register
 * a profile, then the function will do it for you. On this automatic profile
 * registration, vortex library will assign the default close and
 * start handler which always replies true to close and start
 * channels, but frame received handler will still not defined.
 * 
 * As a consequence you must ensure to register a <i>frame
 * received</i> handler at first level using vortex_profiles function
 * or ensure to register a frame received handler for this function.
 *
 * Again, you may be asking your self why not simply deny channel
 * creation for those petition which didn't define frame received at
 * any level. This is done because there \ref vortex_manual_dispatch_schema "are still more method" 
 * to retrieve frames from remote peers, bypassing
 * the frame received handlers (for the second and first levels). 
 *
 * This method can be used with \ref vortex_channel_wait_reply which
 * wait for a specific reply. Check out \ref vortex_manual_wait_reply "this section to know more about Wait Reply method".
 *
 * <i><b>NOTE:</b> in threaded mode, channel creation process can fail, as the
 * same as non-threaded mode, so you can also get a NULL value for
 * both invocation models. 
 * 
 * This executing model is slightly different from
 * \ref vortex_connection_new where the value returned must be checked with
 * \ref vortex_connection_is_ok.
 * 
 * You can check the error code returned by the remote peer if the
 * channel creation fails (NULL value returned) by calling to \ref
 * vortex_connection_pop_channel_error.  </i>
 *
 * <i><b>NOTE2:</b> especially under threaded-moded (activated if \ref
 * VortexOnChannelCreated "on_channel_created" is defined) the channel
 * reference can't be used until the channel reference is
 * notified.
 * 
 * During the channel preparation, a reference starts to be available
 * at the connection, and it is possible to get a reference to it
 * (i.e. \ref vortex_connection_get_channel or \ref
 * vortex_connection_get_channel_by_func). This is provided to avoid
 * race conditions with other running threads during its preparation
 * and to allow application level to do some provisioning on the
 * channel (\ref vortex_channel_set_data). 
 * 
 * In short, do use the channel reference to send data until it was
 * notified to be completely created.</i>
 *
 *
 * @param connection           session where channel will be created.
 * @param channel_num          the channel number. Using 0 automatically assigns the next channel number free.
 * @param profile              the profile under the channel will be created.
 * @param close                handler to manage channel closing.
 * @param close_user_data      user data to be passed in to close handler.
 * @param received             handler to manage frame reception on channel.
 * @param received_user_data   data to be passed in to <i>received</i> handler.
 * @param on_channel_created   a callback to be able to make channel process to be async.
 * @param user_data            user data to be passed in to \ref VortexOnChannelCreated "on_channel_created".
 * 
 * @return The newly created channel or NULL if fails under
 * non-threaded model.  Under threaded model returned value will
 * always be NULL and newly channel created will be notified on
 * \ref VortexOnChannelCreated "on_channel_created".
 */
VortexChannel * vortex_channel_new (VortexConnection      * connection, 
				    int                     channel_num, 
				    const char            * profile,
				    VortexOnCloseChannel    close,
				    axlPointer              close_user_data,
				    VortexOnFrameReceived   received,
				    axlPointer              received_user_data,
				    VortexOnChannelCreated  on_channel_created, axlPointer user_data)
{
	return vortex_channel_new_full (connection, channel_num,        
					NULL,                           /* serverName */ 
					profile,
					EncodingUnknown,                /* encoding value */
					NULL,                           /* profile content data */
					0,                              /* profile content size */
					close, close_user_data,         /* close handlers stuff */
					received, received_user_data,   /* frame received stuff */
					on_channel_created, user_data); /* on channel created   */
}

/** 
 * @brief Extended version for \ref vortex_channel_new, supporting all options available while creating new channels.
 *
 * This function allows to create a new channel like \ref
 * vortex_channel_new does, but providing support for all options that
 * are available described by the BEEP definition. Options provided by this function not provided by
 * \ref vortex_channel_new are:
 *
 *   - This function allows to set the <b>serverName</b> parameter for
 *   the start element. This parameter is used by some profiles
 *   definitions, as <b>TLS</b>, to establish default certificate to
 *   be used inside the session tuning.
 *
 *   - This function allows to configure the <b>profile element content</b> and
 *   its <b>encoding</b>. Again, some profiles definition needs to send
 *   additional data inside the profile element during the channel
 *   creation exchange. 
 *
 * You can check the full documentation on how to use the function at
 * \ref vortex_channel_new. Apart from previous parameters, this
 * function does the same operations.
 *
 * While sending profile content data you must provide its
 * encoding. Allowed encoding you have to use are none and
 * base64. Vortex Library allows you to report which encoding is used
 * for the profile content with \ref VortexEncoding. By default,
 * unless explicit use by the profile definition or by your
 * requirements, you should use \ref EncodingNone. 
 *
 * This function doesn't encode data received into the encoding
 * selected. This is a task reserved to the profile implementation or
 * the application level. Encoding value is used to report to the
 * remote peer which encoding is being used.
 *
 * For the content profile data and the serverName this function will
 * perform a local copy of the given data. If one of both is NULL it
 * will ignored.
 *
 * 
 * @param connection           Session where channel will be created.
 * @param channel_num          The channel number. Using 0 automatically assigns the next channel number free.
 * @param serverName           Data sent to the remote site as an initial data exchanged.
 * @param profile              The profile under the channel will be created.
 * @param encoding             Profile content encoding used.
 *
 * @param profile_content      The profile content data to be sent at the
 * channel creation. This data is also referred as piggyback. The
 * function will perform a copy of the content profile passed in.
 *
 * @param profile_content_size How huge is the content profile data.
 * @param close                Handler to manage channel closing.
 * @param close_user_data      User data to be passed in to close handler.
 * @param received             Handler to manage frame reception on channel.
 * @param received_user_data   Data to be passed in to <i>received</i> handler.
 * @param on_channel_created   A callback to be able to make channel process to be async.
 * @param user_data            User data to be passed in to \ref VortexOnChannelCreated "on_channel_created".
 * 
 * @return the newly created channel or NULL if fails under
 * non-threaded model.  Under threaded model returned value will
 * always be NULL and newly channel created will be notified on
 * \ref VortexOnChannelCreated "on_channel_created".
 */
VortexChannel * vortex_channel_new_full (VortexConnection      * connection,
					 int                     channel_num, 
					 const char            * serverName,
					 const char            * profile,
					 VortexEncoding          encoding,
					 const char            * profile_content,
					 int                     profile_content_size,
					 VortexOnCloseChannel    close,
					 axlPointer              close_user_data,
					 VortexOnFrameReceived   received,
					 axlPointer              received_user_data,
					 VortexOnChannelCreated  on_channel_created, axlPointer user_data)
{
	VortexChannelData * data;
	VortexCtx         * ctx   = vortex_connection_get_ctx (connection);

	v_return_val_if_fail (connection, NULL);
	v_return_val_if_fail (vortex_connection_is_ok (connection, false), NULL);
	v_return_val_if_fail (profile, NULL);

	/* creates data to be passed in to __vortex_channel_new */
	data                        = axl_new (VortexChannelData, 1);
	data->connection            = connection;
	data->channel_num           = channel_num;
	data->close                 = close;
	data->close_user_data       = close_user_data;
	data->received              = received;
	data->received_user_data    = received_user_data;

	/* server name data */
	data->serverName            = axl_strdup (serverName);

	/* profile stuff */
	data->profile               = axl_strdup(profile);
	data->encoding              = encoding;
	data->profile_content_size  = profile_content_size;
	
	/* profile content  */
	if (profile_content != NULL && profile_content_size > 0) {
		data->profile_content       = axl_new (char , profile_content_size + 1);
		memcpy (data->profile_content, profile_content, profile_content_size);
	}

	data->on_channel_created    = on_channel_created;
	data->user_data             = user_data;
	data->threaded              = (on_channel_created != NULL);

	/* launch threaded version if on_channel_created is defined */
	if (data->threaded) {
		vortex_thread_pool_new_task (ctx, (VortexThreadFunc) __vortex_channel_new, data);
		return NULL;
	}

	return __vortex_channel_new (data);
}

/** 
 * @brief Allows to create a new channel using all possible option and
 * making possible to define profile content using a printf-like
 * syntax.
 * 
 * This function behaves the same way than \ref
 * vortex_channel_new_full but allowing to define the profile content
 * using the printf-like format. This could be handy while sending
 * profile content that is more elaborated than just a simple string. 
 *
 * For more information see \ref vortex_channel_new_full and \ref
 * vortex_channel_new.
 * 
 * 
 * @param connection             The connection where the channel will be created.
 * @param channel_num            The channel num requested to create.
 * @param serverName             The serverName value for this channel request.
 * @param profile                The profile requested.
 * @param encoding               The content profile encoding.
 * @param close                  The close channel handler.
 * @param close_user_data        User space data to be passed in to the close channel handler.
 * @param received               The frame received handler.
 * @param received_user_data     User space data to be passed in to the frame received handler.
 * @param on_channel_created     Async notification callback to be executed once the channel is created or not.
 * @param user_data              User space data to be passed in to <b>on_channel_created</b>
 * @param profile_content_format The profile content using a printf-like format: "<blob>%s</blob>"
 * 
 * @return                       A new channel created or NULL if the channel wasn't possible to be created.
 */
VortexChannel     * vortex_channel_new_fullv                   (VortexConnection      * connection,
								int                     channel_num, 
								const char            * serverName,
								const char            * profile,
								VortexEncoding          encoding,
								VortexOnCloseChannel    close,
								axlPointer              close_user_data,
								VortexOnFrameReceived   received,
								axlPointer              received_user_data,
								VortexOnChannelCreated  on_channel_created, 
								axlPointer user_data,
								const char            * profile_content_format, 
								...)
{
	va_list         args;
	char          * profile_content;
	VortexChannel * channel;

	/* initialize the args value */
	va_start (args, profile_content_format);

	/* build the message */
	profile_content = axl_strdup_printfv (profile_content_format, args);

	/* end args values */
	va_end (args);

	/* create the channel */
	channel = vortex_channel_new_full (connection, 
					   channel_num,
					   serverName,
					   profile,
					   encoding,
					   profile_content,
					   strlen (profile_content),
					   close,
					   close_user_data,
					   received,
					   received_user_data,
					   on_channel_created,
					   user_data);
	/* free no longer needed profile content */
	axl_free (profile_content);

	/* return value returned by this function */
	return channel;
	
	
}

/** 
 * @brief Returns the next message number to be used on this channel.
 *
 * This message number is the one used to message sent. This have
 * nothing to do with next message number expected.
 *
 * This function is used by the Vortex Library for its internal
 * process to figure out what message number to use on next sending request.
 * 
 * @param channel the channel to work on
 * 
 * @return the message number or -1 if fail
 */
int             vortex_channel_get_next_msg_no (VortexChannel * channel)
{	
	if (channel == NULL)
		return -1;

	return channel->last_message_sent + 1;
}

/** 
 * @brief Returns actual message number expected to be received on this channel.
 *
 *
 * This function must be not confused with
 * \ref vortex_channel_get_next_msg_no. 
 * 
 * Explanation: Once a channel is created, both peer connected by a
 * session where the channel is created, are expecting messages from
 * each other and will send messages to each other. 
 * 
 * On this context, message that peer A sends to peer B are controlled
 * by vortex_get_next_expected_msg_no on B peer side. This means, B is
 * expecting message 12 to arrive and this '12' will be returned by
 * previous function. 
 * 
 * Once a message is properly received and processed, this previous
 * number will updated to 13.  Now, when A sends to peer B a message,
 * A needs to now what message number to use on its next message. That
 * why vortex_channel_get_next_msg_no stands for. 
 * 
 * This function returns actual message number to use or actual
 * message number is waiting other peer to receive.
 *
 * This explanation applies to all frame indication found on this
 * module. As a short path to remember how this function works think
 * as follow: 
 * 
 * <i>"If actual peer is going to send a message I will use
 * vortex_channel_get_next_* functions to build up it. If I need to
 * validate that incoming message is right I will use
 * vortex_channel_get_next_expected_* functions."</i>
 *
 * 
 * @param channel the channel to operate on.
 * 
 * @return expected message number to receive or -1 if fails
 */
int             vortex_channel_get_next_expected_msg_no (VortexChannel * channel)
{
	/* check channel reference */
	if (channel == NULL)
		return -1;

	return channel->last_message_received + 1;
}


/** 
 * @internal
 * @brief Creates a empty channel structure
 *
 * 
 * Creates a new empty channel struct, filling it with passed in
 * data. This function should not be used from outside of vortex
 * library. It is used for internal vortex library purposes.
 * 
 * Its basic behavior is to create a ready to use channel initialized
 * with the correct internal state to be used inside vortex library.
 * So, it can't be useful for vortex consumers.
 * 
 * @param channel_num the channel number to use.
 * @param profile the profile that will manage this channel.
 * @param connection the connection where channel is associated.
 * 
 * @return a new allocated channel.
 */
VortexChannel * vortex_channel_empty_new (int                channel_num,
					  const char       * profile,
					  VortexConnection * connection)
{
	VortexChannel * channel = NULL;
	VortexCtx     * ctx     = vortex_connection_get_ctx (connection);

	if (ctx == NULL) {
		vortex_log (VORTEX_LEVEL_CRITICAL, "failed to create channel, received a connection with a null context");
		return NULL;
	}

	if (profile == NULL || ! vortex_connection_is_ok (connection, false)) {
		vortex_log (VORTEX_LEVEL_CRITICAL, "failed to create channel, received NULL profile or a non-connected BEEP session");
		return NULL;
	}

	channel                                 = axl_new (VortexChannel, 1);
	channel->ctx                            = ctx;
	channel->channel_num                    = channel_num;
	channel->last_message_sent              = -1;
	channel->last_message_received          = -1;
	channel->last_reply_sent                = 0;
	channel->last_reply_expected            = 0;
	channel->last_reply_written             = -1;
	channel->last_seq_no                    = 0;
	channel->last_seq_no_expected           = 0;
	channel->is_opened                      = false;
	channel->being_closed                   = false;
	channel->complete_flag                  = true;
	channel->previous_frame                 = axl_list_new (axl_list_always_return_1, (axlDestroyFunc) vortex_frame_unref);
	channel->profile                        = axl_strdup (profile);
	channel->connection                     = connection;
	channel->window_size                    = 4096;
	channel->desired_window_size            = 4096;
	/* Attention commander: the following two values *must* be
	 * 4095 because they represent the maximum seq no value
	 * accepted at the channel start up, which is the same to say,
	 * I'm willing to accept 4096 bytes (counting from 0 up to
	 * 4095 yields 4096) */
	channel->max_seq_no_accepted            = 4095;
	channel->max_seq_no_remote_accepted     = 4095;
	channel->pending_messages               = axl_list_new (axl_list_always_return_1, NULL);
	channel->waiting_msgno                  = vortex_queue_new ();
	channel->queue_msgs                     = vortex_queue_new ();
	channel->queue_seqs                     = vortex_queue_new ();
	vortex_mutex_create (&channel->send_mutex);
	vortex_mutex_create (&channel->receive_mutex);
	vortex_cond_create  (&channel->close_cond);
	vortex_mutex_create (&channel->close_mutex);
	vortex_mutex_create (&channel->pending_mutex);
	vortex_cond_create  (&channel->pending_cond);
	vortex_mutex_create (&channel->ref_mutex);
	channel->ref_count                      = 1; /* one reference */
	vortex_mutex_create (&channel->serialize_msg_mutex);
	vortex_mutex_create (&channel->serialize_rpy_mutex);
	vortex_mutex_create (&channel->serialize_ans_mutex);
	channel->serialize                      = false;
	channel->waiting_replies                = false;
	channel->reply_processed                = true;
	channel->data                           = vortex_hash_new_full (axl_hash_string, axl_hash_equal_string, NULL, NULL);
	channel->stored_replies                 = axl_hash_new (axl_hash_int, axl_hash_equal_int);

	/* The following are a set of values implicitly accepted as
	 * default for the channel 0.  It includes:
	 * 
	 *  - setting the frame receive function so all message
	 *  received on channel 0 are handled by *
	 *  vortex_channel_0_frame_received.
	 *
	 *  - setting default mime content, letting transfer encoding
	 *  to be the default. */ 
	if (channel_num == 0) {
		channel->received              = vortex_channel_0_frame_received;
		channel->mime_type             = "application/beep+xml";
		channel->transfer_encoding     = "binary";

		/* some message number exceptions */
		channel->last_message_received = 0;
		channel->last_message_sent     = 0;
	}else {
		channel->mime_type         = vortex_profiles_get_mime_type (ctx, profile);
		channel->transfer_encoding = vortex_profiles_get_transfer_encoding (ctx, profile);
	}

	return channel;
}

/** 
 * @brief Allows to set close channel handler for the given channel.
 *
 * Set the handler to be executed on this channel when a close channel
 * event arrives.  Each call done to this function will repleace
 * previous close handler configuration.
 *
 * @param channel the channel to configure
 * @param close the close handler to execute
 * @param user_data the user data to pass to <i>close</i> execution
 */
void            vortex_channel_set_close_handler (VortexChannel * channel,
						  VortexOnCloseChannel close,
						  axlPointer user_data)
{
	/* check reference */
	if (channel == NULL)
		return;
	
	channel->close           = close;
	channel->close_user_data = user_data;
	
	return;
}

/** 
 * @brief Allows to configure the (already) closed handler.
 *
 * This handler will be used by the vortex engine to provided a
 * notification that the channel (the particular one) is about to be
 * unrefered and disconnected from its connection. This is useful to
 * get notifications about a channel that was not closed properly (so
 * the close handler can't be called: \ref vortex_channel_set_close_handler).
 * 
 * @param channel The channel to configure.
 *
 * @param closed The handler to configure.
 *
 * @param user_data User defined pointer to be provided to the handler
 * called.
 */
void               vortex_channel_set_closed_handler           (VortexChannel          * channel,
								VortexOnClosedChannel    closed,
								axlPointer               user_data)
{
	/* check the reference */
	if (channel == NULL)
		return;

	/* configure handlers */
	channel->closed      = closed;
	channel->closed_data = user_data;

	return;
}

/** 
 * @internal Function used by the vortex connection module to notify
 * that the channel is about to be unrefered by the connection.
 * 
 * @param channel The channel to be notified. If the channel do not
 * have closed handler, nothing is done.
 */
void               vortex_channel_invoke_closed                (VortexChannel          * channel)
{
	/* check null references */
	if (channel == NULL || channel->closed == NULL)
		return;

	/* notify */
	channel->closed (channel, channel->closed_data);

	return;
}

/** 
 * @brief Allows to configure the close notify handler on the provided
 * channel.
 * 
 * See \ref VortexOnNotifyCloseChannel and \ref
 * vortex_channel_notify_close to know more about this function. 
 * 
 * @param channel The channel that is configured to get close notification.
 *
 * @param close_notify The close notify handler to execute.
 *
 * @param user_data User defined data to be passed to the close notify
 * handler.
 */
void               vortex_channel_set_close_notify_handler     (VortexChannel              * channel,
								VortexOnNotifyCloseChannel   close_notify,
								axlPointer                   user_data)
{
	/* check incoming channel */
	if (channel == NULL)
		return;

	/* configure handlers */
	channel->close_notify           = close_notify;
	channel->close_notify_user_data = user_data;

	/* nothing more to do over here */
	return;
}

/** 
 * @brief Allows to set the frame received handler.
 *
 * Set the handler to be executed on this channel when a <i>received</i>
 * channel event arrives.  If you call several times to this function,
 * the handler and user data will be changed to passed in data. So you
 * can use this function to reset handlers definition on this channel.
 *
 * See also the \ref vortex_manual_implementing_request_response_pattern "following document to get useful information" about
 * using this function.
 *
 * 
 * @param channel the channel to configure.
 * @param received the start handler to execute.
 * @param user_data the user data to pass to <i>received</i> execution.
 */
void            vortex_channel_set_received_handler (VortexChannel * channel, 
						     VortexOnFrameReceived received,
						     axlPointer user_data)
{
	/* check reference */
	if (channel == NULL)
		return;

	channel->received           = received;
	channel->received_user_data = user_data;

	return;
}

/** 
 * @brief Allows to set complete frames flag
 *
 * BEEP protocol defines that an application level message must be
 * splitted into pieces equal to or less than the channel window
 * size. Those pieces are called frames.
 * 
 * This enable BEEP peers to avoid receiving large messages and other
 * problems relationed with channel starvation. As a consequence,
 * while receiving a message reply, represented by a RPY, ANS or NUL
 * frame type, it could be splitted into several frames.
 * 
 * If you don't want to handle frames segments and then join them back
 * into a single message you can set the complete flag. Once
 * activated, Vortex will join all frames into a single message and
 * deliver it to the application level using the default application
 * delivery defined (second and first level handlers or wait reply
 * method).
 *
 * By default, every channel created have the complete flag activated. 
 *
 * To clarify:
 * \code
 *      
 *       -----------------
 *      |    App level    | Sends a message
 *       -----------------
 *      |    A message    | Which is splitted into
 *       -----------------
 *      | Several frames  | that frames are sent..
 *       -----------------
 *      |  Over a channel |
 *       -----------------
 *              ||
 *              ||
 *       -----------------
 *      |  Over a channel | 
 *       -----------------
 *      | Several Frames  | are received from remote peer side..
 *       -----------------
 *      |    A message    | joined into a single message
 *       -----------------
 *      |   App level     | App level receive a complete message
 *       -----------------
 *    
 * \endcode
 *
 * @param channel the channel to configure.
 * @param value true activate complete flag, false deactivates it.
 */
void               vortex_channel_set_complete_flag            (VortexChannel * channel,
								bool     value)
{
	/* check reference */
	if (channel == NULL)
		return;

	channel->complete_flag = value;

	return;
}

/** 
 * @internal
 * @brief Returns if the given channel have stored a previous channel.
 *
 * This function is used by the vortex reader to be able to join frame
 * fragments which are bigger than the channel window size.
 *
 * This function should not be useful for Vortex Library consumers.
 *
 * @param channel the channel to operate on.
 * @param frame   New frame received to check if previous frame for this have been already received.
 * 
 * @return true if previous frame is defined or false if not
 */
bool               vortex_channel_have_previous_frame          (VortexChannel * channel)
{
	/* check reference */
	if (channel == NULL)
		return false;

	/* return if the previous frame is defined */ 
	return (axl_list_length (channel->previous_frame) > 0);
}


/** 
 * @internal
 * @brief Allows to get previous frame stored for the given channel.
 * 
 * @param channel the channel to operate on.
 * 
 * @return the previous frame or NULL if fails
 */
VortexFrame      * vortex_channel_get_previous_frame           (VortexChannel * channel)
{
	/* check reference */
	if (channel == NULL)
		return false;

	/* a reference to the previous frame */
	return axl_list_get_last (channel->previous_frame);
}

/** 
 * @internal
 * @brief Allows to set previous frame for the given channel.
 * 
 * @param channel        The channel to operate on.
 * @param previous_frame The previous frame to replace
 * @param new_frame      The frame to be used as a replacement.
 */
void               vortex_channel_store_previous_frame           (VortexChannel * channel, 
								  VortexFrame   * new_frame)
{
	/* check reference */
	if (channel == NULL || new_frame == NULL)
		return;
	
	/* configure new previous frame */
	axl_list_append (channel->previous_frame, new_frame);

	return;
}

/** 
 * @internal Allows to recover all previous frames stored, joined
 * together into a single frame. This internal function is used by the
 * vortex reader once a complete series of frames, with the more flag
 * activated are received, and it is required to join them together to
 * perform a single deliver.
 * 
 * @param channel The channel where the joing request will be
 * performed.
 * 
 * @return A newly allocated frame representing all previous
 * frames. All previous stored frames are deallocated.
 */
VortexFrame      * vortex_channel_build_single_pending_frame   (VortexChannel * channel)
{
	axlListCursor * cursor;
	int             size;
	VortexFrame   * frame;
	VortexFrame   * result;
	unsigned char * payload;
	VortexCtx     * ctx = vortex_channel_get_ctx (channel);

	/* create a cursor */
	cursor = axl_list_cursor_new (channel->previous_frame);

	/* count size to hold */
	size = 0;
	while (axl_list_cursor_has_item (cursor)) {

		/* get the size */
		frame = axl_list_cursor_get (cursor);

		/* accumulate size */
		size += vortex_frame_get_content_size (frame);

		/* next position */
		axl_list_cursor_next (cursor);

	} /* end while */

	/* now we have the size to build */
	payload = axl_new (unsigned char, size + 1);
	size    = 0;

	/* reset to the first position of the cursor */
	axl_list_cursor_first (cursor);
	while (axl_list_cursor_has_item (cursor)) {

		/* get the size */
		frame = axl_list_cursor_get (cursor);

		/* write content */
		memcpy (payload + size, vortex_frame_get_payload (frame), vortex_frame_get_content_size (frame));
		
		/* update size */
		size += vortex_frame_get_content_size (frame);

		/* next position */
		axl_list_cursor_next (cursor);

	} /* end while */

	/* get the first frame */
	axl_list_cursor_first (cursor);
	frame = axl_list_cursor_get (cursor);

	/* build result */
	result = vortex_frame_create_full_ref (ctx,
		/* frame type */
		vortex_frame_get_type (frame),
		/* frame channel */
		vortex_frame_get_channel (frame),
		/* frame msgno */
		vortex_frame_get_msgno (frame),
		/* more flag */
		false,
		/* frame seqno (data from the first frame) */
		vortex_frame_get_seqno (frame),
		/* frame size */
		size,
		/* frame asno */
		vortex_frame_get_ansno (frame),
		vortex_frame_mime_status_is_available (frame) ? vortex_frame_get_content_type (frame) : NULL,
		vortex_frame_mime_status_is_available (frame) ? vortex_frame_get_transfer_encoding (frame) : NULL,
		(char *) payload);

	/* ensure we set the channel */
	vortex_frame_set_channel_ref (result, channel); 

	/* now we have the result, release all pending frames */
	axl_list_cursor_first (cursor);
	while (axl_list_cursor_has_item (cursor)) {
		/* remove item */
		axl_list_cursor_remove (cursor);
	} /* end while */
	axl_list_cursor_free (cursor);

	if (axl_list_length (channel->previous_frame) != 0) {
		vortex_log (VORTEX_LEVEL_CRITICAL, "internal vortex engine error..");
		vortex_frame_unref (result);
		return NULL;
	}
		
	/* return the frame */
	return result;
					       
	
}

/** 
 * @brief Allows to get actual complete flag status for the given channel.
 *
 * This function allows to get actual complete flag for the given
 * channel. This function is actually used by the vortex reader to be
 * able to join fragment frames. 
 *
 * @param channel the channel to operate on.
 * 
 * @return true if complete flag is defined or false if not.
 */
bool               vortex_channel_have_complete_flag           (VortexChannel * channel)
{
	/* check reference */
	if (channel == NULL)
		return false;

	return (channel->complete_flag); 
}

/** 
 * @brief Returns the channel number for selected channel.
 * 
 * @param channel the channel to get the number
 * 
 * @return the channel number or -1 if fail
 */
int             vortex_channel_get_number (VortexChannel * channel)
{
	/* check reference */
	if (channel == NULL)
		return -1;

	return channel->channel_num;
}

/** 
 * @internal
 * @brief Update channel status for sending messages.
 *
 * This function is for internal vortex library support. It update
 * actual channel status for messages to be sent.  Function also
 * allows to control what values are updated. This is necessary to
 * update only seqno value for a channel and not the msgno while
 * sending RPY, ANS and NUL frame types messages.
 *
 * @param channel    The channel to update internal status for next message to be sent
 * @param frame_size The frame size of the frame sent.
 * @param update     What values to update
 */
void vortex_channel_update_status (VortexChannel * channel, int  frame_size, int msg_no, WhatUpdate update)
{
	/* get a reference */
#if defined(ENABLE_VORTEX_LOG)
	VortexCtx     * ctx     = vortex_channel_get_ctx (channel);
#endif

	/* check reference */
	if (channel == NULL)
		return;

	/* update msgno */
	if ((update & UPDATE_MSG_NO) == UPDATE_MSG_NO) {
		channel->last_message_sent +=  1;
	}

	/* update rpy no */
	if ((update & UPDATE_RPY_NO) == UPDATE_RPY_NO) {
		channel->last_reply_sent   += 1;
	}

	/* update seqno */
	if ((update & UPDATE_SEQ_NO) == UPDATE_SEQ_NO) {
		channel->last_seq_no        = ((channel->last_seq_no + frame_size) % (MAX_SEQ_NO));
	}

	/* update ansno */
	if ((update & UPDATE_ANS_NO) == UPDATE_ANS_NO) {
		channel->last_ansno_sent   += 1;
	}

	/* update last reply written */
	if ((update & UPDATE_RPY_NO_WRITTEN) == UPDATE_RPY_NO_WRITTEN) {
		channel->last_reply_written += 1;
	}

	vortex_log (VORTEX_LEVEL_DEBUG, 
	       "updating channel sending status to: msgno=%d, rpyno=%d, seqno=%d, ansno=%d..",
	       channel->last_message_sent, channel->last_reply_sent, channel->last_seq_no, channel->last_ansno_sent);

	return;
}

/** 
 * @internal
 * @brief Updates channel status for received messages.
 *
 *
 * This function is for internal vortex library support. It update
 * actual channel status for messages to be received. This is actually
 * used by vortex_reader thread to check incoming messages.
 *
 * 
 * @param channel The channel to update internal status for next message to be received 
 * @param frame_size the frame size of the frame received
 * @param update what parts of the channel status to update.
 */
void vortex_channel_update_status_received (VortexChannel * channel, int  frame_size,
					    WhatUpdate update)
{

#if defined(ENABLE_VORTEX_LOG)
	VortexCtx     * ctx     = vortex_channel_get_ctx (channel);
#endif

	/* check reference */
	if (channel == NULL)
		return;
	
	/* update expected msgno */
	if ((update & UPDATE_MSG_NO) == UPDATE_MSG_NO) {
		channel->last_message_received += 1;
	}

	/* update expected rpy no */
	if ((update & UPDATE_RPY_NO) == UPDATE_RPY_NO) {
		channel->last_reply_expected   += 1;
	}

	/* update expected seqno */
	if ((update & UPDATE_SEQ_NO) == UPDATE_SEQ_NO) {
		channel->last_seq_no_expected   = ((channel->last_seq_no_expected + frame_size) % (MAX_SEQ_NO));
	}
	
	/* update expected ansno */
	if ((update & UPDATE_ANS_NO) == UPDATE_ANS_NO) {
		channel->last_ansno_expected    += 1;
	}

	vortex_log (VORTEX_LEVEL_DEBUG, 
	       "updating channel receiving status to: msgno=%d, rpyno=%d, seqno=%d, ansno=%d..",
	       channel->last_message_received, 
	       channel->last_reply_expected, 
	       channel->last_seq_no_expected, 
	       channel->last_ansno_expected);

	return;
}


/** 
 * @internal
 * @brief Common function support other function to send message.
 * 
 * @param channel the channel where the message will be sent.
 * @param message the message to be sent.
 * @param message_size the message size.
 * @param msg_no message number reference used for this message sending attempt.
 * @param wait_reply optional wait reply to implement Wait Reply method.
 * 
 * @return true if channel was sent or false if not.
 */
bool        __vortex_channel_send_msg_common (VortexChannel   * channel,
					      const void      * message,
					      size_t            message_size,
					      int             * msg_no,
					      WaitReplyData   * wait_reply)
{
	VortexSequencerData * data;
	int                   mime_header_size;
	VortexCtx           * ctx     = vortex_channel_get_ctx (channel);

	v_return_val_if_fail (channel,                           false);
	v_return_val_if_fail (message,                           false);
	v_return_val_if_fail (channel->is_opened,                false);

	if (channel->channel_num != 0) {
		v_return_val_if_fail (! channel->being_closed,   false);
	} /* end if */

	/* check if the connection is ok */
	if (! vortex_connection_is_ok (channel->connection, false)) {
		vortex_log (VORTEX_LEVEL_WARNING, "received a MSG request to be sent over a non connected session, dropming message");
		return false;
	}

	/* lock send mutex */
	vortex_mutex_lock (&channel->send_mutex);

	/* flag the channel to be awaiting to process message */
	channel->being_sending = true;

	/* get current mime header configuration */
	mime_header_size       = __vortex_channel_get_mime_headers_size (channel);

	/* prepare data to be sent */
	data                   = axl_new (VortexSequencerData, 1);
	data->channel          = channel;
	data->type             = VORTEX_FRAME_TYPE_MSG;
	data->channel_num      = vortex_channel_get_number (channel);
	data->msg_no           = vortex_channel_get_next_msg_no (channel);
	data->first_seq_no     = vortex_channel_get_next_seq_no (channel);
	
	/* update message size */
	data->message_size     = message_size + mime_header_size;
		
	vortex_log (VORTEX_LEVEL_DEBUG, 
		    "new message to sent, type=%d channel=%d msgno=%d size (%d) = msg size (%d) + mime size (%d)",
		    data->type, data->channel_num, data->msg_no,
		    data->message_size, message_size, mime_header_size);

	/* copy mime headers according to channel configuration, that
	 * comes from profile configuration. */
	data->message          = axl_new (char , data->message_size + 1);

	/* according to mime headers size */
	if (mime_header_size > 0)
		__vortex_channel_get_mime_headers (channel, data->message);
	
	/* copy message content */
	memcpy (data->message + mime_header_size, message, message_size);

	/* return back message no used  */
	if (msg_no != NULL) 
		(* msg_no) = vortex_channel_get_next_msg_no (channel);

	/* is there a wait reply? */
	if (wait_reply != NULL) {
		
		/* install queue  */
		wait_reply->msg_no_reply = vortex_channel_get_next_msg_no (channel);
		
		/* enqueue reply data on this */
		vortex_mutex_lock   (&channel->receive_mutex);
		/* update the ref count to avoid double deallocs */
		vortex_channel_wait_reply_ref (wait_reply);

		/* push data and unlock */
		vortex_queue_push   (channel->waiting_msgno, wait_reply);
		vortex_mutex_unlock (&channel->receive_mutex);
	}

	/* update channel status */
	vortex_channel_update_status (channel, data->message_size, 0, UPDATE_SEQ_NO | UPDATE_MSG_NO);

	vortex_sequencer_queue_data (ctx, data);

	/* unlock send mutex */
	vortex_mutex_unlock (&channel->send_mutex);

	/* flag the channel to be ready (no message being_sending) */
	channel->being_sending = false;

	return true;
}

/** 
 * @brief Allows to send a message.
 *
 * Sends the <i>message</i> over the selected <i>channel</i>. It will
 * create the needed frames to be sent to remote peer connected by
 * session where <i>channel</i> is created.
 *
 * On <i>msg_no</i> will be returned the message number used for this
 * delivery. This can be useful for waiting response for this message
 * on this channel. 
 *
 * A usual question you may have is: could I remove the message chunk
 * I've used to call this function. The answer is yes. This function
 * makes a local copy you can directly free used message as follows:
 *
 * \code 
 *
 *      // we create some dynamically allocated message
 *      char * message = axl_strdup_printf ("%d - %s ..\n", 10, 
 *                                          "some kind of very important message");
 *
 *      if (!vortex_channel_send_msg (channel, message, strlen (message), NULL)) {
 *             // some kind of error process and free your resources
 *             axl_free (message);
 *
 *      }
 *
 *      // you should free dynamically allocated resources no longer
 *      // used just after running vortex_channel_send_msg.
 *
 *      axl_free (message);
 *
 * \endcode
 * 
 * @param channel where message will be sent
 * @param message the message to send
 * @param message_size the message size (user space calculated, that is actual <i>message</i> size)
 * @param msg_no returns the message number used for this deliver. If NULL the message number will not be returned.
 * 
 * @return true if message could be sent, false if fail
 */
bool            vortex_channel_send_msg   (VortexChannel    * channel,
					   const void       * message,
					   size_t             message_size,
					   int              * msg_no)
{
	return __vortex_channel_send_msg_common (channel, message, message_size, msg_no, NULL);
}

/** 
 * @brief Allows to send message using a printf-like format.
 *
 * This function works and does the same like \ref
 * vortex_channel_send_msg but allowing you the define a message in a
 * printf like format.
 *
 * A simple example can be:
 * \code
 *    vortex_channel_send_msgv (channel, &msg_no,
 *                              "this a value %s", 
 *                              "an string");
 * \endcode
 *
 * It uses stdargs to build up the message to send and uses strlen to
 * figure out the message size to send.  See also \ref
 * vortex_channel_send_msg.
 *
 * 
 * @param channel the channel used to send the message
 * @param msg_no the msgno assigned to message sent
 * @param format the message format (printf-style) 
 * 
 * @return the same as \ref vortex_channel_send_msg.
 */
bool            vortex_channel_send_msgv       (VortexChannel * channel,
						int           * msg_no,
						const char    * format,
						...)
{
	char       * msg;
	bool         result;
	va_list      args;

	/* initialize the args value */
	va_start (args, format);

	/* build the message */
	msg = axl_strdup_printfv (format, args);

	/* end args values */
	va_end (args);

	/* execute send_msg */
	result = vortex_channel_send_msg (channel, msg, strlen (msg), msg_no);
	
	/* free value and return */
	axl_free (msg);
	return result;
	
}

/** 
 * @brief Allows to send a message and start a \ref vortex_manual_wait_reply "wait reply".
 *
 * This is especial case of \ref vortex_channel_send_msg. This function is
 * mainly used to be able to make a synchronous reply waiting for the
 * message this function is going to sent. 
 *
 * Because in some cases the thread sending the message can run
 * faster, or simply the thread planner have chosen to give greeter
 * priority, than the thread actually does the wait, this function
 * allows to avoid this race condition.
 *
 * Application designer which are planning to call \ref vortex_channel_wait_reply
 * should use this function to send message.
 *
 * You can also read more about Wait Reply Method \ref vortex_manual_wait_reply "here".
 * 
 * @param channel the channel where message will be sent.
 * @param message the message to be sent.
 * @param message_size the message size to be sent.
 * @param msg_no a required integer reference to store the message number used
 * @param wait_reply a Wait Reply object created using \ref vortex_channel_create_wait_reply 
 * 
 * @return the same as \ref vortex_channel_send_msg
 */
bool               vortex_channel_send_msg_and_wait               (VortexChannel    * channel,
								   const void       * message,
								   size_t             message_size,
								   int              * msg_no,
								   WaitReplyData    * wait_reply)
{
	return __vortex_channel_send_msg_common (channel, message, message_size, msg_no, wait_reply);
}

/** 
 * @brief Printf-like version for the \ref
 * vortex_channel_send_msg_and_wait function.
 *
 * This function works the same way \ref
 * vortex_channel_send_msg_and_wait does but supporting printf-like
 * message definition.
 * 
 * @param channel the channel where message will be sent.
 * @param msg_no a required integer reference to store message number used.
 * @param wait_reply a wait reply object created with \ref vortex_channel_create_wait_reply
 * @param format a printf-like string format defining the message to be sent.
 * 
 * @return 
 */
bool               vortex_channel_send_msg_and_waitv              (VortexChannel   * channel,
								   int             * msg_no,
								   WaitReplyData   * wait_reply,
								   const char      * format,
								   ...)
{
	char       * msg;
	bool         result;
	va_list      args;

	/* initialize the args value */
	va_start (args, format);

	/* build the message */
	msg = axl_strdup_printfv (format, args);

	/* end args values */
	va_end (args);

	/* execute send_msg */
	result = vortex_channel_send_msg_and_wait (channel, msg, strlen (msg), msg_no, wait_reply);
	
	/* free value and return */
	axl_free (msg);
	return result;
}

/** 
 * @internal Function used to dealloc stored sequencer data in the
 * pending reply hash.
 * 
 * @param data The data to be deallocated.
 */
void __vortex_channel_free_sequencer_data (VortexSequencerData * data)
{
	if (data == NULL)
		return;
	axl_free (data->message);
	axl_free (data);
	return;
}


/** 
 * @internal
 * @brief Common function to perform message replies.
 * 
 * @param channel the channel where the reply will be sent.
 * @param type the reply type for the message.
 * @param message the message included on the reply.
 * @param message_size the message reply size
 * @param msg_no_rpy the message number this function is going to reply.
 * 
 * @return true if reply was sent, false if not.
 */
bool      __vortex_channel_common_rpy (VortexChannel    * channel,
				       VortexFrameType    type,
				       const void       * message,
				       size_t             message_size,
				       int                msg_no_rpy)
{
	VortexSequencerData * data;
	int                   mime_header_size;
	VortexCtx           * ctx     = vortex_channel_get_ctx (channel);

	v_return_val_if_fail (channel,            false);

	/* check message content only if the frame type is not NUL */
	if (type != VORTEX_FRAME_TYPE_NUL) {
		v_return_val_if_fail (message,    false);
	} /* end if */
	v_return_val_if_fail (channel->is_opened, false);
	
	/* lock send mutex */
	vortex_mutex_lock (&channel->send_mutex);
	channel->being_sending = true;
	
	/* check if received a reply for a message that has being
	 * already replied */
	if (msg_no_rpy < vortex_channel_get_next_reply_no (channel) && 
	    (type == VORTEX_FRAME_TYPE_RPY || type == VORTEX_FRAME_TYPE_ERR)) {
		vortex_log (VORTEX_LEVEL_CRITICAL, "received a reply message to be sent with msgno=%d, but that message was already replied. Next expected message waiting for reply is msgno=%d. You BEEP app is replying twice to the same message. Dropping this reply..",
			    msg_no_rpy, vortex_channel_get_next_reply_no (channel));
		channel->being_sending = false;	
		vortex_mutex_unlock (&channel->send_mutex);
		return false;

	} /* end if */

	/* check if the connection is ok */
	if (! vortex_connection_is_ok (channel->connection, false)) {
		vortex_log (VORTEX_LEVEL_WARNING, "received a reply request to be sent over a non connected session, dropming message");

		/* flag the channels non being sending */
		channel->being_sending = false;	
		vortex_mutex_unlock (&channel->send_mutex);
		return false;
	}

	/* create data to be sent by the sequencer */
	data                  = axl_new (VortexSequencerData, 1);
	data->channel         = channel;
	data->type            = type;
	data->channel_num     = vortex_channel_get_number (channel);
	data->msg_no          = msg_no_rpy;
	data->first_seq_no    = vortex_channel_get_next_seq_no (channel);

	/* get current mime header configuration */
	mime_header_size      = __vortex_channel_get_mime_headers_size (channel);
	data->message_size    = message_size + mime_header_size;
	data->ansno           = channel->last_ansno_sent;

	vortex_log (VORTEX_LEVEL_DEBUG, "new reply message to sent size (%d) = msg size (%d) + mime size (%d)",
	       data->message_size, message_size, mime_header_size);
	
	/* copy the message to be send using memcpy */
	if (message != NULL || data->message_size > 0) {
		/* copy mime header configuration if defined */
		data->message = axl_new (char , data->message_size + 1);
		if (mime_header_size > 0)
			__vortex_channel_get_mime_headers (channel, data->message);

		/* copy application level message */
		memcpy (data->message + mime_header_size, message, message_size);
	}

	/* check we are going to reply the message that is waiting to
	 * be replied */
	if (vortex_channel_get_next_reply_no (channel) != msg_no_rpy) {

		/* check if the connection is ok */
		if (! vortex_connection_is_ok (channel->connection, false)) {
			vortex_log (VORTEX_LEVEL_WARNING, 
				    "while waiting for a previous reply to be sent, detected reply request to be sent over a non connected session, dropming message");
			
			/* flag the channels non being sending */
			channel->being_sending = false;	
			vortex_mutex_unlock (&channel->send_mutex);
			return false;
		}

		vortex_log (VORTEX_LEVEL_WARNING, 
			    "Received a rpy request for message %d while already waiting to reply to %d, storing and unlocking caller for later deliver",
			    msg_no_rpy,
			    vortex_channel_get_next_reply_no (channel));
		
		/* store */
		axl_hash_insert_full (channel->stored_replies, 
				      /* store the reply (key and destroy function) */
				      INT_TO_PTR (msg_no_rpy), 
				      NULL, 
				      /* data value (and the destroy function) */
				      data, (axlPointer) __vortex_channel_free_sequencer_data);

		/* unlock */
		vortex_mutex_unlock (&channel->send_mutex);
		return true;
	}

	/* do sending reply operation */
 send_reply:
	vortex_log (VORTEX_LEVEL_DEBUG, "sending reply for message %d (size: %d, sequencer queue status: %d, channel queue status: %d)\n", 
 		    msg_no_rpy, message_size, vortex_async_queue_items (ctx->sequencer_queue), axl_list_length (channel->pending_messages));

	switch (type) {
	case VORTEX_FRAME_TYPE_NUL:
		/* because tne NUL reply ends a series of ANS reply,
		 * we update the seqno and the next expected reply
		 * number. */
		vortex_channel_update_status (channel, data->message_size, 0, UPDATE_SEQ_NO | UPDATE_RPY_NO);

		/* reset last ansno message sent. */
		channel->last_ansno_sent = 0;
		
		break;
	case VORTEX_FRAME_TYPE_ANS:
		/* due to the ANS reply, we only update the seqno number.  */
		vortex_channel_update_status (channel, data->message_size, 0, UPDATE_SEQ_NO | UPDATE_ANS_NO);
		break;
	case VORTEX_FRAME_TYPE_RPY:
	case VORTEX_FRAME_TYPE_ERR:
		/* update channel status. */
		vortex_channel_update_status (channel, data->message_size, 0, UPDATE_SEQ_NO | UPDATE_RPY_NO);
		break;
	default:
		/* nothing to do */
		vortex_log (VORTEX_LEVEL_CRITICAL, "Reaching a switch case at common_rpy which should not be possible");
		break;
	}

	/* remove from the pending stored hash */
	axl_hash_delete (channel->stored_replies, INT_TO_PTR (msg_no_rpy));

	/* send data to sequencer */
	vortex_sequencer_queue_data (ctx, data);

	/* update next msg_no_rpy */
	msg_no_rpy = vortex_channel_get_next_reply_no (channel);

	/* now check if the pending stored replies have the next reply
	 * to be sent */
	data = axl_hash_get (channel->stored_replies, INT_TO_PTR (msg_no_rpy));
	if (data != NULL) {
		/* update the type */
		type = data->type;

		/* and update new first seq no to use while building
		 * the frame */
		data->first_seq_no    = vortex_channel_get_next_seq_no (channel); 

		vortex_log (VORTEX_LEVEL_WARNING, "found pending reply=%d to be sent on channel=%d", 
			    msg_no_rpy, channel->channel_num);
		goto send_reply;
	} /* end if */

	/* unlock send mutex */
	vortex_mutex_unlock   (&channel->send_mutex);
	channel->being_sending = false;	

	return true;
}

/** 
 * @brief printf-like version for \ref vortex_channel_send_rpy function.
 *
 * This function is a wrapper to \ref vortex_channel_send_rpy but allowing
 * you to use a prinf-style message format definition. See also
 * vortex_channel_send_rpy.
 * 
 * @param channel the channel where the message will be sent.
 * @param msg_no_rpy the message number to reply over this channel.
 * @param format a printf-like string format to build the message reply to be sent.
 * 
 * @return true if reply was sent, false if not.
 */
bool            vortex_channel_send_rpyv       (VortexChannel * channel,
						int             msg_no_rpy,
						const   char  * format,
						...)
{
	char       * msg;
	bool         result;
	va_list      args;

	/* initialize the args value */
	va_start (args, format);

	/* build the message */
	msg = axl_strdup_printfv (format, args);

	/* end args values */
	va_end (args);

	/* execute send_msg */
	result = vortex_channel_send_rpy (channel, msg, strlen (msg), msg_no_rpy);
	
	/* free value and return */
	axl_free (msg);
	return result;
}

/** 
 * @brief Allows to send a message reply using RPY type.
 *
 * Sends a RPY message to received message MSG identified by
 * <i>msg_no_rpy</i>. This functions allows you to reply a to a received MSG
 * in a way you only need to provide the <i>msg_no_rpy</i>.  According to
 * RFC3080: the BEEP Core, a message reply can not be done if there
 * are other message awaiting to be replied.
 *
 * This means calling to \ref vortex_channel_send_rpy trying to send a
 * reply having previous message been waiting for a reply yields to
 * block caller until previous replies are sent.
 *
 * These may seems to be an annoying behavior but have some advantages
 * and can be also avoided easily.  First of all, think about the
 * following scenario: a vortex client (or beep enabled client) sends
 * over the same channel 3 request using 3 MSG frame type without
 * waiting to be replied.
 * 
 * Beep RPC 3080 section "2.6.1 Withing a Single Channel" says
 * listener role side have to reply to these 3 MSG in the order they
 * have come in, if they have entered through the same channel. This ensure client
 * application that its request are sent in order to remote server but also
 * its reply are received in the same order. 
 *
 * This feature is a really strong one because application designer
 * don't need to pay attention how message replies are received no
 * matter how long each one have taken to be processed on the server
 * side.
 *
 * As a side effect, it maybe happen you don't want to get hang until
 * reply is received just because it doesn't make sense for the
 * application to receive all replies in the same order msg were sent.
 * 
 * For this case client application must send all msg over different
 * channel so each MSG/RPY pair is independent from each other. This
 * can also be found on RFC 3080 section "2.6.2 Between Different
 * Channels"
 *
 * As a consequence:
 * <ul>
 *
 *  <li>Message sent over the same channel will receive its replies in
 *  the same order the message were sent. This have a <i>"serial behavior"</i></li>
 *  
 *  <li>Message sent over different channels will receive replies as
 *  fast as the remote BEEP node replies to them. This have a <i>"parallel behavior"</i></li>
 *
 * </ul>
 * 
 * 
 * @param channel the channel where  the message will be sent
 * @param message the message to sent
 * @param message_size the message size
 * @param msg_no_rpy the message number this function is going to reply to.
 * 
 * @return true if message could be sent, false if fail
 */
bool            vortex_channel_send_rpy        (VortexChannel    * channel,  
						const void       * message,
						size_t             message_size,
						int                msg_no_rpy)
{
	return __vortex_channel_common_rpy (channel, VORTEX_FRAME_TYPE_RPY,
					    message, message_size, msg_no_rpy);
}

/** 
 * @brief Allows to perform an ANS/NUL reply to a given MSG frame received.
 *
 * While replying to a message received, a MSG frame type, the server
 * side may chose to reply to it using a RPY frame type. But, it may
 * chose to use a series of ANS frame type ended by a NUL frame
 * type. This message exchange type is termed as one-to-many.
 *
 * This allows to issue a MSG request and to receive several replies
 * to the same message because the server side have to perform several
 * tasks to complete the MSG request received. Instead of keep on
 * waiting to all tasks to be completed, the server side could send an
 * ANS reply for each partial task completed. Once all tasks are
 * completed, the server side will send the last reply frame type
 * using a NUL frame type.
 *
 * Previous description allows client application to get actual
 * responses from the server side as long as each piece of the global
 * task gets completed.
 *
 * We have described that a series of ANS frame types must be used
 * ended by a NUL frame type. This description is handled using
 * both function \ref vortex_channel_send_ans_rpy and \ref
 * vortex_channel_send_ans_rpyv, to generate ANS message reply and
 * using \ref vortex_channel_finalize_ans_rpy to generate the last
 * one, payload-empty message, using NUL frame type.
 * 
 * Let's see a piece of code:
 * \code
 *    while (some yet not ready condition) {
 *       // perform some task which is returned by a function
 *       response = function_which_returns_the_task (&response_size);
 *
 *       // send the reply
 *       if (!vortex_channel_send_ans_rpy (channel, response,
 *                                        response_size,
 *                                        // the MSG num we are replying to
 *                                        msg_no_rpy) {
 *           // well, we have found an error while sending a message
 *           manage_my_specific_error ();
 *       }
 *
 *       // update the condition to break the loop
 *       update_my_internal_loop_conditions ();
 *    }
 *  
 *    // we have finished, signal peer client side that no more ANS
 *    // will be received.
 *    if (!vortex_channel_finalize_ans_rpy (channel, msg_no_rpy) {
 *         // well, we have found an error while sending a message
 *         manage_my_especific_error ();
 *    }
 * \endcode
 *
 * This message style exchange is especially suited for those
 * situations where the client peer wants to get access to some huge
 * or unlimited content, such as streaming, making a request such as:
 * "get that huge file" to be replied by the server side with a series
 * of ANS replies ended by a NUL message.
 * 
 * Vortex Library is prepared to send messages no matter how huge they
 * are, but you have to consider the size of that set of messages in
 * memory. Oversizing you memory resources will cause your app to
 * start trashing or, in some OS, to just fail with unpredictable
 * behaviors.
 * 
 * As a conclusion:
 *
 *   - You can always use one-to-one message exchange using MSG/RPY
 *   but, if the message content replied is considerable, i.e. 4
 *   megabytes, you should consider starting to use MSG/ANS message
 *   exchange style.
 *
 *   - Using a MSG/RPY message exchange having considerable sized
 *   message replies, will cause the client peer side to get no
 *   response until all frames from the remote side have been received
 *   and joined into one single message. This could not be a problem,
 *   but, in the context of graphical user interfaces, the overall
 *   effect is that your applications is hanged, without knowing if
 *   the server side is death or is still processing current reply
 *   issued.
 * 
 *   - Using the ANS/NUL reply semantic is an elegant way to reply
 *   several messages to the same message because allows you to
 *   support that process on top of the Vortex Library API rather than
 *   splitting your message into several pieces to be sent to the
 *   remote side. 
 *
 *   - Once started an one-to-many reply using ANS, it must be ended
 *   using the NUL frame type (\ref vortex_channel_finalize_ans_rpy).
 * 
 * 
 * @param channel      The channel where the reply will be performed.
 * @param message      The message replied.
 * @param message_size The message size replied.
 * @param msg_no_rpy   Message number to reply.
 * 
 * @return true if the given reply was sent, otherwise false is
 * returned.
 */
bool               vortex_channel_send_ans_rpy                    (VortexChannel    * channel,
								   const void       * message,
								   size_t             message_size,
								   int                msg_no_rpy)
{
	return __vortex_channel_common_rpy (channel, VORTEX_FRAME_TYPE_ANS,
					    message, message_size,
					    msg_no_rpy);
}

/** 
 * @brief Allows to send ANS message reply using stdargs argument.
 * Check documentation for \ref vortex_channel_send_ans_rpy
 * function. This function only differ in the sense that message to be
 * sent is built from the StdArg received. Here is an example:
 * 
 * \code
 *     // sending a simple message using StdArgs 
 *     if (!vortex_channel_send_ans_rpyv (channel, msg_no_rpy, 
 *                                        "My message contains: %s, with the %d value",
 *                                        "This string value", 8)) {
 *            // manage the error found
 *     }
 * \endcode
 * 
 *   
 * 
 * @param channel      The channel where the reply will be performed.
 * @param msg_no_rpy   Message number to reply.
 * @param format       StdArg (printf-like) message format.
 * 
 * @return true if the message was sent properly.
 */
bool               vortex_channel_send_ans_rpyv                   (VortexChannel * channel,
								   int             msg_no_rpy,
								   const char    * format,
								   ...)
{
	char       * msg;
	bool         result;
	va_list      args;

	/* initialize the args value */
	va_start (args, format);

	/* build the message */
	msg = axl_strdup_printfv (format, args);

	/* end args values */
	va_end (args);

	/* execute send_msg */
	result = vortex_channel_send_ans_rpy (channel, msg, strlen (msg), msg_no_rpy);
	
	/* free value and return */
	axl_free (msg);
	return result;
}

/** 
 * @brief Allows to finalize the series of ANS reply already sent with a NUL reply.
 *
 * Inside the one-to-many exchange, it is defined that several ANS
 * reply messages are ended by a NUL reply. This function allows you
 * to sent that one NUL ending reply.
 *
 * @param channel The channel where previous ANS reply message was sent.
 *
 * @param msg_no_rpy The message we where replying to and the one to
 * be notified that one-to-many reply have ended.
 * 
 * @return true if the message was sent properly.
 */
bool               vortex_channel_finalize_ans_rpy                (VortexChannel * channel,
								   int             msg_no_rpy)
{
	return __vortex_channel_common_rpy (channel, VORTEX_FRAME_TYPE_NUL,
					    NULL, 0, msg_no_rpy);
}

/** 
 * @brief Allows to reply a message using ERR message type.
 *
 * Sends a error reply to message received identified by
 * <i>msg_no_rpy</i>. This function does the same as \ref vortex_channel_send_rpy
 * but sending a ERR frame, so take a look to its documentation.
 *
 * NOTE \ref vortex_channel_send_err is provided to enable sending BEEP ERR
 * frames which may or may not include XML content, just like RPY or
 * MSG type.  This depends on the profile you are implementing.
 * 
 * Because of this, \ref vortex_channel_send_err can't receive the
 * error code and the message because it has to provide a more general
 * API, for example, to send binary error messages.
 * 
 * In other words, if you want to use the error reply format used for
 * BEEP channel 0 (and some other standard profiles) you will have to
 * use \ref vortex_frame_get_error_message and then provide the
 * message created to \ref vortex_channel_send_err.
 *
 * @param channel the channel where error reply is going to be sent
 * @param message the message
 * @param message_size the message size
 * @param msg_no_rpy the message number to reply
 * 
 * @return true if message was sent or false if fails
 */
bool       vortex_channel_send_err        (VortexChannel    * channel,  
					   const void       * message,
					   size_t             message_size,
					   int                msg_no_rpy)
{
	return __vortex_channel_common_rpy (channel, VORTEX_FRAME_TYPE_ERR,
					    message, message_size, msg_no_rpy);
}

/** 
 * @brief Printf-like version for \ref vortex_channel_send_err.
 *
 * Allows to send a ERR message type using a printf-like message format.
 * 
 * @param channel the channel where message reply will be sent.
 * @param msg_no_err the message number to reply to.
 * @param format a printf-like string format.
 * 
 * @return true if message was sent of false if fails
 */
bool            vortex_channel_send_errv       (VortexChannel * channel,
						int             msg_no_err,
						const   char  * format,
						...)
{
	char       * msg;
	bool         result;
	va_list      args;

	/* initialize the args value */
	va_start (args, format);

	/* build the message */
	msg = axl_strdup_printfv (format, args);

	/* end args values */
	va_end (args);

	/* execute send_msg */
	result = vortex_channel_send_err (channel, msg, strlen (msg), msg_no_err);
	
	/* free value and return */
	axl_free (msg);
	return result;
}

/** 
 *
 * @brief Returns next sequence number to be used. 
 *
 * Return the next sequence number to be used on this channel. This function must not be
 * confused with \ref vortex_channel_get_next_expected_seq_no. 
 *
 * This function is actually used to know what message seq number to
 * use on next frame to be sent. If you what to get next frame seq no
 * to expected to be received on this channel you must use \ref vortex_channel_get_next_expected_seq_no.
 * 
 * @param channel the channel to operate on.
 * 
 * @return the next seq_number to use, or -1 if fail
 */
unsigned int      vortex_channel_get_next_seq_no (VortexChannel * channel)
{
	/* check reference */
	if (channel == NULL)
		return -1;
	return channel->last_seq_no;
}

/** 
 * @brief Returns actual channel window size.
 *
 * Maximum window size accepted by remote peer. This is the number of
 * bytes that the remote peer is willing to accept until a new SEQ
 * frame is set.
 * 
 * @param channel the channel to operate on.
 * 
 * @return the window size or -1 if fails
 */
int             vortex_channel_get_window_size (VortexChannel * channel)
{
	/* check reference */
	if (channel == NULL)
		return -1;
	
	return channel->window_size;
}



/** 
 * @brief Allows the caller to change the channel window size.
 *
 * A larger window size than the default (4096) allows the remote peer
 * to send more data over this channel at once. This increases the
 * bandwidth that can be received, especially over high-latency sockets,
 * at the expense of increasing the latency for other channels.
 *
 * @param channel the channel to operate on.
 *
 * @param desired_size the desired window size; it is recomended to be
 * at least 4096.
 */
void            vortex_channel_set_window_size (VortexChannel * channel,
                                                                                               int desired_size)
{
       v_return_if_fail (channel);
       v_return_if_fail (desired_size > 0);
       
       channel->desired_window_size = desired_size;

       return;
}

/** 
 * @brief Returns current mime type used for messages exchange perform
 * on the given channel.
 * 
 * @param channel The channel to operate on.
 * 
 * @return Current mime type or NULL if it fails. Value returned must
 * not be deallocated.
 */
const char        * vortex_channel_get_mime_type                (VortexChannel * channel)
{
	/* check reference */
	if (channel == NULL)
		return NULL;

	return channel->mime_type;
}

/** 
 * @brief Returns current content transfer encoding used for messages exchange perform
 * on the given channel.
 * 
 * @param channel The channel to operate on.
 * 
 * @return Current content transfer encoding or NULL if it fails. Value returned must
 * not be deallocated.
 */
const char             * vortex_channel_get_transfer_encoding        (VortexChannel * channel)
{
	/* check reference */
	if (channel == NULL)
		return NULL;
	
	return channel->transfer_encoding;
}

/** 
 * @internal
 *
 * @brief Updates current notion about how much bytes is willing to
 * accept the remote peer on the given channel.
 * 
 * While processing SEQ frames, it is needed to update current notion
 * about how much bytes to be accepted by the remote side while
 * sending new frames. This is done because the flow control needed to
 * allow several channel to share the connection bandwidth.
 *
 * Once the vortex reader receives a SEQ frame to a remote peer, this
 * function updates current status for the selected channel.
 * 
 * @param channel The channel where the update operation will be performed.
 *
 * @param frame The SEQ frame received containing information about
 * the current buffer size to be accepted.
 */
void vortex_channel_update_remote_incoming_buffer (VortexChannel * channel, 
						   VortexFrame   * frame)
{
	int          new_size;
#if defined(ENABLE_VORTEX_LOG)
	VortexCtx  * ctx     = vortex_channel_get_ctx (channel);
#endif

	/* check reference */
	if (channel == NULL || frame == NULL)
		return;

	vortex_log (VORTEX_LEVEL_DEBUG, "Received SEQ frame update operation: channel=%d ackno=%d window=%d",
	       vortex_frame_get_channel (frame), vortex_frame_get_seqno (frame), vortex_frame_get_content_size (frame));

	/* The following two function gets the ackno value and the
	 * window value from the SEQ frame received:
	 *
	 * What we have to keep in mind that SEQ frames represent
	 * first, the channel they applies to, second the last octet
	 * count received from the remote side at the time that the
	 * SEQ frame was generated and finally, the window value,
	 * which is a value representing how many bytes is prepare to
	 * accept remote side on the given channel at the time the SEQ
	 * frame was generated.
	 *
	 * buffer: #####0000000000000000000000
	 *             ^                     ^
	 *             | <-- window size --> |
	 *     ack no -+                     +
	 *    
	 *  # -> octet received
	 *  0 -> free space for an incoming octet.
	 *
	 * So, the maximum seqno value to be accepted for the remote
	 * side is the sum of the ackno value plus the window size
	 * minus one octet. */
	new_size = vortex_frame_get_seqno (frame) + vortex_frame_get_content_size (frame) -1;

	if (new_size <= channel->max_seq_no_remote_accepted) {
		vortex_log (VORTEX_LEVEL_CRITICAL, 
		       "Received a SEQ frame specifying a new seq no maximum value (%d = %d + %d) that is smaller than the max seq no stored (%d), protocol violation",
		       new_size, vortex_frame_get_seqno (frame), 
		       vortex_frame_get_content_size (frame),
		       channel->max_seq_no_remote_accepted);
		__vortex_connection_set_not_connected (vortex_channel_get_connection (channel), 
						       "Received a SEQ frame specifying a new seq no maximum value that is smaller than the max seq no stored, protocol violation");
		return;
	}

	vortex_log (VORTEX_LEVEL_DEBUG, "received SEQ frame, updated maximum seq no allowed from %d to %d",
	       channel->max_seq_no_remote_accepted, new_size);
	channel->max_seq_no_remote_accepted = new_size % (MAX_SEQ_NO);
	return;
}

/** 
 * @internal
 *
 * @brief Allows to get current maximum seq number accepted on the
 * given channel by the remote side.
 *
 * While creating BEEP frames, vortex sequencer uses this information
 * to track which is the maximum number of octets that the channel is
 * willing to accept. This allows to not oversize that value, which
 * would mean a protocol violation.
 *
 * Value returned from this function is the maximum sequence number to
 * be accepted by the remote side including the max seq no value.
 * 
 * @param channel The channel where the value is checked.
 * 
 * @return Current max sequence number to be accepted.
 */
int  vortex_channel_get_max_seq_no_remote_accepted (VortexChannel * channel)
{
	/* check reference */
	if (channel == NULL)
		return -1;

	return channel->max_seq_no_remote_accepted;
}

/** 
 * @brief Gets the amount of data to be copied from the pending
 * message into the frame about being fragmented or built. The
 * function is used by the vortex sequencer to get the amount of data
 * to get from the message. 
 *
 * If the channel do not have any \ref VortexChannelFrameSize defined
 * at \ref vortex_channel_set_next_frame_size_handler the function
 * will use the default implementation provided.
 * 
 * @param channel The channel that is required to return next frame size.
 *
 * @param next_seq_no This value represent the next sequence number
 * for the first octect to be sent on the frame.
 *
 * @param message_size This value represent the size of the payload to
 * be sent.
 *
 * @param max_seq_no Is the maximum allowed seqno accepted by the
 * remote peer. Beyond this value, the remote peer will close the
 * connection.
 * 
 * @return The amount of payload to use into the next frame to be
 * built. The function will return -1 if the channel reference
 * received is NULL.
 */
int                vortex_channel_get_next_frame_size         (VortexChannel * channel,
							       int             next_seq_no,
							       int             message_size,
							       int             max_seq_no)
{
	/* get current context */
	VortexCtx   * ctx = vortex_channel_get_ctx (channel);
	int           size;

	v_return_val_if_fail (channel, -1);

	/* check channel implementation */
	if (channel->next_frame_size) {
		/* return value provided by the handler defined */
		return channel->next_frame_size (channel, next_seq_no, message_size, max_seq_no, channel->next_frame_size_data);
	} /* end if */

	/* check the connection holding the channel have an
	 * implementation configured */
	size = vortex_connection_get_next_frame_size (channel->connection, channel, next_seq_no, message_size, max_seq_no);
	if (size > 0) 
		return size;

	/* now check for globally configured segmentator */
	if (ctx->next_frame_size) {
		/* return value provided by the handler defined */
		return ctx->next_frame_size (channel, next_seq_no, message_size, max_seq_no, ctx->next_frame_size_data);
	} /* end if */

	/* use default implementation */
	if ((next_seq_no + message_size) > max_seq_no)
		return VORTEX_MIN (max_seq_no - next_seq_no + 1, VORTEX_MIN (channel->window_size, 4096));
	return VORTEX_MIN (message_size, VORTEX_MIN (channel->window_size, 4096));
}

/** 
 * @brief Allows to configure the \ref VortexChannelFrameSize handler
 * to be used by the sequencer to decide how many data is used into
 * each frame produced (outstanding frames).
 * 
 * @param channel The channel to be configured.
 *
 * @param next_frame_size The handler to be configured or NULL if the
 * default implementation is required.
 *
 * @param user_data User defined pointer to be passed to the \ref
 * VortexChannelFrameSize handler.
 * 
 * @return Returns previously configured handler or NULL if nothing
 * was set. The function does nothing and return NULL if channel
 * reference received is NULL.
 */
VortexChannelFrameSize  vortex_channel_set_next_frame_size_handler (VortexChannel          * channel,
								    VortexChannelFrameSize   next_frame_size,
								    axlPointer               user_data)
{
	VortexChannelFrameSize previous;
	v_return_val_if_fail (channel, NULL);

	/* get previous value */
	previous                      = channel->next_frame_size;

	/* configure new value (first data and then handler) */
	channel->next_frame_size_data = user_data;
	channel->next_frame_size      = next_frame_size;


	/* nullify data if a null handler is received */
	if (next_frame_size == NULL)
		channel->next_frame_size_data = NULL;

	/* return previous configuration */
	return previous;
}

/** 
 * @internal
 * 
 * @brief Allows to update current buffer status for the maximum seq
 * no value to be accepted.
 *
 * Maximum sequence number to be accepted is a mechanism to perform
 * flow control so remote side do not send more bytes that the
 * advertised by this function.
 *
 * When this function is executed, vortex reader process have already
 * checked that the frame received is inside the buffer allowed (that
 * is, inside the maximum sequence number allowed).
 * 
 * @param channel The channel to update current status
 *
 * @param frame The containing data to be accepted, with a particular
 * size and sequence number.
 *
 * @return The function returns true if the channel buffer have
 * changed, so a SEQ frame must be notified or false to not notify any
 * change.
 */
bool     vortex_channel_update_incoming_buffer (VortexChannel * channel, 
						VortexFrame   * frame,
						int           * ackno,
						int           * window)
{
	int         new_max_seq_no_accepted;
	int         consumed_seqno;
	int         window_size;
#if defined(ENABLE_VORTEX_LOG)
	VortexCtx * ctx     = vortex_channel_get_ctx (channel);
#endif

	if (channel == NULL || frame == NULL)
		return false;

	/* check if the channel is being close, is that is right, do
	 * not generate more SEQ frames for the given channel. This is
	 * to avoid generating a SEQ frame replaying to the <ok /> *
	 * message that is received once the channel is accepted to be
	 * closed.
	 *
	 * Howerver, if the channel is flaged to be "being closed", we
	 * must keep on replying SEQ frames until the <ok /> is
	 * recevied.  This only applies to channel 0 */
	if (channel->being_closed && channel->channel_num == 0) {
		vortex_log (VORTEX_LEVEL_DEBUG, "avoiding generate SEQ frame for a channel that is being closed");
		return false;
	}

	/* Next sentence allows to get the next sequence that should
	 * be used for the next frame received on the given
	 * channel. With this value, a check for the maximum sequence
	 * to be accepted have been met and to update current window
	 * size. */
	consumed_seqno = vortex_frame_get_seqno (frame) + vortex_frame_get_content_size (frame);

	/* record current window size so we use the same value here
	 * and below. */
	window_size             = channel->window_size;

	/* generate a new value for the seqno accepted */
	new_max_seq_no_accepted = (consumed_seqno + window_size - 1) % (MAX_SEQ_NO);

	/* particular case where NUL frame was received, consuming 2
	 * bytes on the current window size, causing the next frame to
	 * be sent without 2 bytes (in the case it is bigger than
	 * window size) */
	if (vortex_frame_get_type (frame) == VORTEX_FRAME_TYPE_NUL) {
		vortex_log (VORTEX_LEVEL_DEBUG, "SEQ FRAME: updating due to NUL frame found");
		goto send_seq_frame;
	}

	/* check we have filled half window size advertised */
/*	if ((new_max_seq_no_accepted - channel->max_seq_no_accepted) < (window_size / 2)) { */
 	if ((consumed_seqno - (channel->max_seq_no_accepted - window_size)) < (window_size / 2)) {
 		if (vortex_log_is_enabled (ctx)) {
 			vortex_log (VORTEX_LEVEL_DEBUG, "SEQ FRAME: not updated, already not consumed half of window advertised: %d < (%d / 2)",
 				    (consumed_seqno - (channel->max_seq_no_accepted - window_size)), window_size);
 			vortex_log (VORTEX_LEVEL_DEBUG, "           frame-content-size=%d, frame-payload-size=%d, ",
 				    vortex_frame_get_content_size (frame), vortex_frame_get_payload_size (frame));
 			vortex_log (VORTEX_LEVEL_DEBUG, "           window_size=%d, consumed_seqno=%d, new_max_seq_no_accepted=%d",
 				    window_size, consumed_seqno, new_max_seq_no_accepted);
 			vortex_log (VORTEX_LEVEL_DEBUG, "           max_seq_no_accepted=%d",
 				    channel->max_seq_no_accepted);
 		} /* end if */
		goto not_update;
	} /* end if */
	
	/* check if we have to update current maximum sequence number
	 * to be accepted, according to the new max seq no accepted
	 * value. That is, if the value calculated is smaller than the
	 * already advertised, we just do nothing, no update and no
	 * SEQ frame advertisement. This will allow to control flow
	 * data received is the already not advertised window is
	 * configured to be smaller. */
	if (new_max_seq_no_accepted > channel->max_seq_no_accepted) {

 	send_seq_frame:
 		/* if the client wants to change the channel window
 		 * size, do so now */
 		if (window_size != channel->desired_window_size) {
 			vortex_log (VORTEX_LEVEL_DEBUG, "SEQ FRAME: Changing window size from %d to %d",
 				    window_size,channel->desired_window_size);
 			window_size             = channel->desired_window_size;
 			channel->window_size    = window_size;
 			new_max_seq_no_accepted = (consumed_seqno + window_size - 1) % (MAX_SEQ_NO);
 		}

		vortex_log (VORTEX_LEVEL_DEBUG, 
 			    "SEQ FRAME: updating allowed max seq no to be received from %d to %d (delta: %d, ackno: %d, window_size: %d)",
  			    channel->max_seq_no_accepted, new_max_seq_no_accepted, 
 			    (new_max_seq_no_accepted - channel->max_seq_no_accepted), consumed_seqno, window_size);

		/* update new max seq no accepted value */
		channel->max_seq_no_accepted = new_max_seq_no_accepted;
		
		/* notify caller new ackno and window value */
		(* ackno  ) = consumed_seqno;
		(* window ) = window_size;
		return true;
	}
	vortex_log (VORTEX_LEVEL_DEBUG, "SEQ FRAME: not updating current max seq values: new max seq no: %d < max seq no: %d",
	       new_max_seq_no_accepted, channel->max_seq_no_accepted);
 not_update:
	(* ackno  )  = -1;
	(* window ) = -1;
	return false;
}

/** 
 * @internal
 *
 * @brief Allows to get maximum max sequence value to be accepted for
 * incoming data.
 * 
 * @param channel  The channel where this data has been requested.
 * 
 * @return The maximum sequence number.
 */
int  vortex_channel_get_max_seq_no_accepted (VortexChannel * channel)
{
	/* check reference */
	if (channel == NULL)
		return -1;

	return channel->max_seq_no_accepted;
}

/** 
 * @brief Allows to check if both references provided points to the
 * same channel (\ref VortexChannel).
 * 
 * @param channelA The first channel reference to check.
 * @param channelB The second channel reference to check.
 * 
 * @return  true if  both references  are  equal, false  if not.  Both
 * references must  be not NULL. In  the case some  reference is null,
 * the function will return false.
 */
bool               vortex_channel_are_equal                    (VortexChannel * channelA,
								VortexChannel * channelB)
{
	/* check references received before proceeding */
	if (channelA == NULL || channelB == NULL)
		return false;

	/* check connection holding channels */
	if (vortex_connection_get_id (channelA->connection) != vortex_connection_get_id (channelB->connection))
		return false;

	/* return if both channel numbers are equal */
	return (channelA->channel_num == channelB->channel_num);
}

/** 
 * @internal
 *
 * @brief Allows to queue an new message pending to be sequenced
 * because the channel is stalled.
 * 
 * @param channel The channel where the message is going to be queue.
 * @param message The message to be queued.
 */
void               vortex_channel_queue_pending_message         (VortexChannel * channel,
								 axlPointer      message)
{
#if defined(ENABLE_VORTEX_LOG)
	VortexCtx     * ctx     = vortex_channel_get_ctx (channel);
#endif

	/* check reference received */
	if (channel == NULL || message == NULL)
		return;

	vortex_log (VORTEX_LEVEL_DEBUG, "queueing a new message pending to be sent: %d messages already queued on channel=%d",
		    axl_list_length (channel->pending_messages),
		    channel->channel_num);

	/* just append the message */
	axl_list_append (channel->pending_messages, message);
	return;
}

/** 
 * @internal
 *
 * @brief Allows to get the next pending message already queued.
 * 
 * @param channel The channel to query for messages to be queued.
 * 
 * @return The next message queued on the channel or NULL if not
 * message queued is pending.
 */
axlPointer         vortex_channel_next_pending_message          (VortexChannel * channel)
{
#if defined(ENABLE_VORTEX_LOG)
	VortexCtx     * ctx   = vortex_channel_get_ctx (channel);
#endif

	/* check the reference received */
	if (channel == NULL)
		return NULL;
	
	vortex_log (VORTEX_LEVEL_DEBUG, "returning the first pending message: current length=%d for channel=%d",
		    axl_list_length (channel->pending_messages), channel->channel_num);
	
	/* no pending messages, return NULL */
	if (axl_list_length (channel->pending_messages) == 0)
		return NULL;

	/* check result */
	return axl_list_get_first (channel->pending_messages);
}

/** 
 * @internal
 * 
 * Internal function which allows to check if the provided channel
 * have pending messages to be sequenced.
 */
bool             vortex_channel_is_empty_pending_message (VortexChannel * channel)
{
	/* check reference */
	if (channel == NULL)
		return false;

	/* return if the pending messages are 0 */
	return  (axl_list_length (channel->pending_messages) == 0);
}

/** 
 * @internal
 *
 * @brief Allows to remove the next message queued on the pending
 * message queue.
 * 
 * @param channel The channel where the removal operation will take
 * place.
 */
void               vortex_channel_remove_pending_message        (VortexChannel * channel)
{
#if defined(ENABLE_VORTEX_LOG)
	VortexCtx     * ctx   = vortex_channel_get_ctx (channel);
#endif

	/* check reference */
	if (channel == NULL)
		return;

	vortex_log (VORTEX_LEVEL_DEBUG, "removing next pending message from channel=%d, length=%d", 
		    channel->channel_num, axl_list_length (channel->pending_messages));

	/* check that the pending messages queue is not empty */
	if (axl_list_length (channel->pending_messages) == 0)
		return;

	/* get the first pending message to be removed and check it is not NULL */
	axl_list_unlink_first (channel->pending_messages);
	return;
}

/** 
 * @internal
 *
 * @brief Returns the current status of pending frames to be sent for
 * the given channel.
 *
 * This function will return how many frames are pending to be sent
 * for the given channel.
 * 
 * @param channel the channel where messages pending will be counted.
 * 
 * @return pending message for the given channel
 */
int                vortex_channel_queue_length                    (VortexChannel * channel)
{
	/* check reference */
	if (channel == NULL)
		return 0;

	/* return current queue length */
	return vortex_queue_get_length (channel->queue_msgs) + vortex_queue_get_length (channel->queue_seqs);
}

/** 
 * @brief Allows to serialize all replies received on a particular
 * channel, by checking that previous replies were delivered, avoiding
 * thread race conditions.
 *
 * This function allows to *ensure* that all calls to the frame
 * received handler are done in a way they are ordered. By calling to
 * this function, the vortex engine will ensure that all frames
 * received will be serialized without taking into consideration the
 * thread planner.
 * 
 * Though all frames received are ordered, once they are processed by
 * the vortex reader, they are passed in to the second level handler
 * (for the frame received activation) by using threads.
 * 
 * Beyond this point, the thread planner could decide to stop a thread
 * that carries the RPY 1, and give priority to the thread carrying
 * the RPY 2, causing that the frame receiver to process the frame RPY
 * 2 before its preceding.
 * 
 * In most cases this is not a problem because all replies are matched
 * with its corresponding MSG, but under some circumstances where
 * replies have a relation, like chunks of a file transferred, it is
 * really required to process chunks received in an strict order.
 *
 * @param channel The channel to make frame received handler to behave
 * in a serialize fashion.
 *
 * @param serialize true to enable serialize, false if not.
 */
void               vortex_channel_set_serialize                   (VortexChannel * channel,
								   bool            serialize)
{
	/* check reference */
	if (channel == NULL)
		return;

	/* configure serialize */
	vortex_mutex_lock (&channel->serialize_msg_mutex);
	channel->serialize = serialize;
	if (serialize) {
		/* init hashes */
		channel->serialize_rpy = axl_hash_new (axl_hash_int, axl_hash_equal_int);
		channel->serialize_msg = axl_hash_new (axl_hash_int, axl_hash_equal_int);
		channel->serialize_ans = axl_hash_new (axl_hash_int, axl_hash_equal_int);
	} /* end if */

	vortex_mutex_unlock (&channel->serialize_msg_mutex);

	/* nothing more */
	return;
}

/** 
 * @brief Allows to store a pair key/value associated to the channel.
 * 
 * This allows to associate application data to the channel which can
 * be easily retrieved later using \ref vortex_channel_get_data.
 *
 * In the case a NULL reference is passed to the function as the value
 * parameter, the pair key/value is removed from the channel:
 *
 * \code
 *          vortex_channel_set_data (my_channel, "my_key", NULL);
 *          // previous will remove "my_key" pair from the channel
 * \endcode
 *
 * If it is required to associate a "destroy handler" to the key or
 * value stored, you can use \ref vortex_channel_set_data_full.
 *
 * @param channel Channel where data will be stored.
 *
 * @param key The key index to look up for the data stored. Though the
 * API expects an axlPointer, the hash storing values is configured to
 * receive string keys.
 *
 * @param value The data to be stored. NULL to remove previous data
 * stored under the provided key.
 *
 * See also:
 *
 * - \ref vortex_channel_set_data_full
 * - \ref vortex_channel_get_data
 * - \ref vortex_connection_set_data
 * - \ref vortex_connection_set_data_full
 *
 * <i><b>NOTE:</b> the function do not allows storing NULL value pointers. </i>
 */
void               vortex_channel_set_data                        (VortexChannel * channel,
								   axlPointer key,
								   axlPointer value)
{
	/* store the value without providing a pointer to destroy the
	 * key and the value */
	vortex_channel_set_data_full (channel, key, value, NULL, NULL);
	return;
}

/** 
 * @brief Allows to store a pair key/value associated to the channel,
 * with optional destroy handlers.
 * 
 * This allows to associate application data to the channel which can
 * be easily retrieved later using \ref vortex_channel_get_data. 
 * 
 * The function allows to configure the set of destroy handlers to be
 * called to dealloc key and value stored.
 *
 * In the case a NULL reference is passed to the function as the value
 * parameter, the pair key/value is removed from the channel, calling
 * to associated destroy functions.
 *
 * @param channel Channel where data will be stored.
 *
 * @param key The key index to look up for the data stored. Though the
 * API expects an axlPointer, the hash storing values is configured to
 * receive string keys.
 *
 * @param value The data to be stored. NULL to remove previous data
 * stored under the provided key.
 *
 * @param key_destroy The optional key destroy handler to be called to
 * release the particular key provided.
 *
 * @param value_destroy The optional value destroy handler to be
 * called to release the particular value provided.
 *
 * See also:
 * - \ref vortex_channel_set_data
 * - \ref vortex_channel_get_data
 * - \ref vortex_connection_set_data
 * - \ref vortex_connection_set_data_full
 *
 * <i><b>NOTE:</b> the function do not allows storing NULL value pointers. </i>
 */
void             vortex_channel_set_data_full (VortexChannel    * channel, 
					       axlPointer         key, 
					       axlPointer         value, 
					       axlDestroyFunc     key_destroy,
					       axlDestroyFunc     value_destroy)
{
	/* perform some environment checks */
	if (channel == NULL || key == NULL)
		return;

	/* check if the value is not null. It it is null, remove the
	 * value. */
	if (value == NULL) {
		vortex_hash_remove (channel->data, key);
		return;
	}

	/* store the data selected replacing previous one */
	vortex_hash_replace_full (channel->data, 
				  key, key_destroy, value, value_destroy);
	return;
}

/** 
 * @brief Allows to remove data associated to the channel via \ref
 * vortex_channel_set_data_full without calling to defined destroy
 * functions. If you want to remove items stored on the hash, calling
 * to defined destroy function, just call to \ref
 * vortex_channel_set_data_full with the appropiate key and NUL as
 * data.
 * 
 * @param channel The channel where the delete operation will take place.
 *
 * @param key The key for the data stored to be deleted.
 */
void               vortex_channel_delete_data                     (VortexChannel    * channel,
								   axlPointer         key)
{
	/* perform some environment checks */
	v_return_if_fail (channel);
	v_return_if_fail (key);

	/* delete associated data */
	vortex_hash_delete (channel->data, key);
	return;
}

/** 
 * @brief Returns the value indexed by the given key inside the given
 * channel.
 *
 * User defined pointers returned by this function were stored by
 * calls to \ref vortex_channel_set_data or \ref
 * vortex_channel_set_data_full.
 *
 * @param channel The channel where data will be looked up.
 *
 * @param key The index key to use. Though the API expects an
 * axlPointer, the hash storing values is configured to receive string
 * keys.
 *
 * @return the value or NULL if fails. 
 */
axlPointer         vortex_channel_get_data                        (VortexChannel * channel,
								   axlPointer key)
{
 	v_return_val_if_fail (channel,       NULL);
 	v_return_val_if_fail (key,           NULL);
 	v_return_val_if_fail (channel->data, NULL);


	/* get the data stored */
	return vortex_hash_lookup (channel->data, key);
	
}

/** 
 * @brief Allows to increase reference counting for the provided
 * channel. If the reference count for the channel provided reach 0,
 * the channel is deallocated.
 *
 * The function is thread-safe, internally protected with mutex.
 * 
 * @param channel The channel to increase its reference counting.
 * 
 * @return true if the channel reference counting was increased,
 * otherwise false is returned.
 */
bool               vortex_channel_ref                             (VortexChannel * channel)
{
	/* check channel received */
	v_return_val_if_fail (channel,                false);
	v_return_val_if_fail (channel->ref_count > 0, false);
	
	/* lock ref/unref operations over this connection */
	vortex_mutex_lock   (&channel->ref_mutex);
	
	/* increase the channel reference counting */
	channel->ref_count++;

	vortex_mutex_unlock (&channel->ref_mutex);

	/* reference increased */
	return true;
}

/** 
 * @brief Decrease in one unit the reference count for the channel
 * provided. In the channel reference count reach 0 value, the channel
 * is deallocated, automatically calling to \ref vortex_channel_free.
 *
 * The function is thread-safe, internally protected with mutex.
 * 
 * @param channel The channel to decrease its reference counting.
 */
void               vortex_channel_unref                           (VortexChannel * channel)
{
#if defined(ENABLE_VORTEX_LOG)
	VortexCtx     * ctx;
#endif

	/* check reference */
	v_return_if_fail (channel);
	v_return_if_fail (channel->ref_count > 0);

	/* lock the channel */
	vortex_mutex_lock (&channel->ref_mutex);

	/* decrease the channel */
	channel->ref_count--;

	ctx = vortex_channel_get_ctx (channel);

	vortex_log (VORTEX_LEVEL_DEBUG, "channel=%d unref called, ref count status after calling=%d", 
		    channel->channel_num, channel->ref_count);

	/* check reference counting */
	if (channel->ref_count == 0) {
		/* unlock the mutex and free the channel */
		vortex_mutex_unlock (&channel->ref_mutex);

		vortex_log (VORTEX_LEVEL_DEBUG, "  freeing channel=%d, ref count is 0", 
			    channel->channel_num);

		vortex_channel_free (channel);
		return;
	} /* end if */
		
	/* unlock and return, reference counting isn't exhausted. */
	vortex_mutex_unlock (&channel->ref_mutex);
	return;
}

/** 
 * @brief Allows to get current reference counting for the channel
 * provided.
 * 
 * @param channel The channel that is checked for its reference
 * counting.
 * 
 * @return Number of references for the channel, or -1 if it fails.
 */
int                vortex_channel_ref_count                       (VortexChannel * channel)
{
	int result;

	/* check reference */
	if (channel == NULL)
		return -1;

	/* lock the channel */
	vortex_mutex_lock (&channel->ref_mutex);

	/* get reference counting */
	result = channel->ref_count;

	/* unlock the channel */
	vortex_mutex_unlock (&channel->ref_mutex);

	return result;
}

/** 
 * @brief Returns actual reply number to be used.
 * 
 * This reply number must be used to now if a message reply can be
 * done. This represent the first message identifier to be replied.
 * 
 * @param channel the channel to query
 * 
 * @return the next reply number to use or -1 if fails
 */
int             vortex_channel_get_next_reply_no          (VortexChannel * channel)
{
	/* check reference */
	if (channel == NULL)
		return -1;
	
	return channel->last_reply_sent;
}

/**
 * @brief Returns actual reply number expected to be received from
 * remote peer.
 * 
 * @param channel the channel to query
 * 
 * @return the reply number expected or -1 if fails
 */
int             vortex_channel_get_next_expected_reply_no (VortexChannel * channel)
{
	/* check reference */
	if (channel == NULL)
		return -1;

	return channel->last_reply_expected;
}

/** 
 * @brief Returns actual seq no expected to be received on this channel. 
 *
 * This function is used to check incoming message seq no.
 * 
 * @param channel the channel to operate on.
 * 
 * @return -1 if fails or expected seq no for this channel.
 */
unsigned int    vortex_channel_get_next_expected_seq_no (VortexChannel * channel)
{
	/* check reference */
	if (channel == NULL)
		return -1;
	
	return channel->last_seq_no_expected;
}

/** 
 * @brief Gets next ansno number to be used for the next message to be sent.
 * 
 * @param channel The channel where the ANS message will be sent.
 * 
 * @return The ansno number.
 */
unsigned int        vortex_channel_get_next_ans_no              (VortexChannel * channel)
{
	/* check reference */
	if (channel == NULL)
		return -1;

	return channel->last_ansno_sent;
}

/** 
 * @brief Returns the next expected ansno number message to be received.
 * 
 * @param channel The channel where the ANS was received.
 * 
 * @return The ansno number.
 */
unsigned int        vortex_channel_get_next_expected_ans_no     (VortexChannel * channel)
{
	/* check reference */
	if (channel == NULL)
		return -1;

	return channel->last_ansno_expected;
}

typedef struct {
	VortexChannel              * channel;
	VortexOnClosedNotification   on_closed;
	VortexOnClosedNotificationFull on_closed_full;
	axlPointer                   user_data;
	bool                         threaded;
} VortexChannelCloseData;


/** 
 * @brief Validates err message received from remote peer.
 * 
 * @param frame The frame where the error message was received.
 * @param code  The error code inside the frame payload.
 * @param msg   The message code inside the frame payload.
 *
 * @return true if the a error message as found, false if not.
 */
bool     vortex_channel_validate_err (VortexFrame * frame, 
				      char  ** code, char  **msg)
{
	axlDtd    * channel_dtd;
	axlDoc    * doc;
	axlNode   * node;
	axlError  * error;
	VortexCtx * ctx = vortex_frame_get_ctx (frame);

	/* first clear received variables */
	if (code != NULL)
		(* code) = NULL;
	if (msg != NULL)
		(* msg)  = NULL;

	/* get channel management DTD */
	if ((channel_dtd = vortex_dtds_get_channel_dtd (ctx)) == NULL) {
		vortex_log (VORTEX_LEVEL_CRITICAL, "unable to load dtd file, cannot validate incoming message, returning error frame");
		return false;
	}

	/* parser xml document */
	doc = axl_doc_parse (vortex_frame_get_payload (frame), 
			     vortex_frame_get_payload_size (frame), &error);
	if (!doc) {
		vortex_log (VORTEX_LEVEL_CRITICAL, "unable to parse err reply, we have a buggy remote peer: %s",
		       axl_error_get (error));
		axl_error_free (error);
		return false;
	}

	/* validate de document */
	if (! axl_dtd_validate (doc, channel_dtd, &error)) {
		/* Validation failed */
		vortex_log (VORTEX_LEVEL_DEBUG, "reply received doesn't contain an error element: %s",
		       axl_error_get (error));
		
		/* free error reported */
		axl_error_free (error);

		/* free document read */
		axl_doc_free (doc);
		return false;
	}
	
	/* get a reference to document root (the error) */
	node  = axl_doc_get_root (doc);
	if (! NODE_CMP_NAME (node, "error")) {
		vortex_log (VORTEX_LEVEL_DEBUG, "not found error element on err reply, we have a buggy remote peer");
		
		/* free document load */
		axl_doc_free (doc);
		return false;
	}
	
	/* get error code returned */
	if (code != NULL)
		(* code ) = axl_node_get_attribute_value_copy (node, "code");

	/* get the message value returned */
	if (msg != NULL) 
		(* msg  ) = axl_node_get_content_copy (node, NULL);

	/* free not needed data */
	axl_doc_free (doc);
	return true;
}

/** 
 * @internal
 * @brief Validates rpy message received from remote peer for close
 * channel request sent.
 * 
 * @param channel 
 * @param frame 
 *
 * @return true if the validation was ok, false if not.
 */
bool __vortex_channel_close_validate_rpy (VortexChannel * channel, 
					  VortexFrame   * frame)
{
	
	axlDtd       * channel_dtd;
	axlDoc       * doc;
	axlNode      * node;
	axlError     * error;
	VortexCtx    * ctx   = vortex_channel_get_ctx (channel);

	/* perform fast validation, trying to match the default
	 * string */
	if (axl_cmp (vortex_frame_get_ok_message (), vortex_frame_get_payload (frame))) {
		vortex_log (VORTEX_LEVEL_DEBUG, "fast channel=%d close reply found, skiping xml and dtd validation", 
			    channel->channel_num);
		return true;
	}

	/* get channel management DTD */
	if ((channel_dtd = vortex_dtds_get_channel_dtd (ctx)) == NULL) {
		vortex_log (VORTEX_LEVEL_DEBUG, "unable to load dtd file, cannot validate incoming message, returning error frame");
		return false;
	}

	/* parser xml document */
	doc = axl_doc_parse (vortex_frame_get_payload (frame), 
			     vortex_frame_get_payload_size (frame), &error);
	if (!doc) {
		vortex_log (VORTEX_LEVEL_DEBUG, "unable to parse rpy reply, we have a buggy remote peer: %s",
		       axl_error_get (error));
		
		/* release memory used by the error object */
		axl_error_free (error);
		return false;
	}

	/* validate the document */
	if (! axl_dtd_validate (doc, channel_dtd, &error)) {
		/* Validation failed */
		vortex_log (VORTEX_LEVEL_DEBUG, "validation failed for rpy reply, we have a buggy remote peer: %s",
		       axl_error_get (error));

		/* free error reported */
		axl_error_free (error);
		
		/* free document reported */
		axl_doc_free (doc);
		return false; 
	}
	
	/* get a reference to document root (the error) */
	node  = axl_doc_get_root (doc);
	if (! NODE_CMP_NAME (node, "ok")) {
		/* free document reported */
		axl_doc_free (doc);

		vortex_log (VORTEX_LEVEL_DEBUG, "not found ok element on rpy reply, we have a buggy remote peer");
		return false;
	}
	
	/* free not needed data */
	axl_doc_free (doc);
	return true;
}

/**
 * @internal Blocks a channel until all replies to message sent are
 * received.
 *
 * This function is synchronized with vortex reader so that process
 * notifies when a reply have been received.
 *
 * This function uses a mutex (close_mutex) and a conditional var
 * (close_cond) that are signaled from vortex_channel_signal_on_close_blocked.
 * 
 * @param channel the channel to operate on.
 *
 * @return true if the waiting was ok in all cases or false if
 * something has failed because the connection is not ok.
 */
bool     __vortex_channel_block_until_replies_are_received (VortexChannel * channel)
{
	VortexConnection * connection;
#if defined(ENABLE_VORTEX_LOG)
	VortexCtx        * ctx    = vortex_channel_get_ctx (channel);
#endif
	bool               result = true;

	/* flag the channel to be in close situation */
	channel->being_closed = true;
	vortex_mutex_lock (&channel->close_mutex);

	connection = vortex_channel_get_connection (channel);
	/*
	 * BEEP RFC Section 2.3.1.3 paragraph 5:
	 * 
	 * No close request can be sent if all replies to the message
	 * sent are received. This requires to only wait for replies
	 * to be received but not processed. 
	 * 
	 * Previously, this while loop was calling to
	 * vortex_channel_is_ready which does the same like the
	 * following check but adding an additional condition: ensure
	 * that the last reply was received.
	 *
	 * However this creates an interation between close the
	 * channel from inside the frame received handler that is
	 * managing the last message.
	 */
	while (! (channel->last_message_sent == (channel->last_reply_expected - 1))) {
		
		/* flag that we are waiting for replies */
		channel->waiting_replies = true;
		
		/* drop a log */
		vortex_log (VORTEX_LEVEL_WARNING, 
		       "we still didn't received all replies for connection=%d channel=%d (MSG %d != RPY %d), %s",
		       vortex_connection_get_id (channel->connection),
		       channel->channel_num,
		       channel->last_message_sent, channel->last_reply_expected - 1,
		       vortex_connection_is_ok (connection, false) ? "block until all replies are received" :
		       "leaving remaining replies because connection is broken..");
		
		/* check the connection status to perform a signaled
		 * wait for replies not received */
		if (vortex_connection_is_ok (connection, false)) {

			/* wait every half second (result receive the wait status returned) */
			VORTEX_COND_TIMEDWAIT (result, 
					       &channel->close_cond, 
					       &channel->close_mutex,
					       500000);
			if (! result)
				break;
		} else {
			/* the connection is closed so, the replies
			 * will never arrive. Because this is a
			 * protocol violation, we can are going to
			 * drop a log to the console and let the close
			 * process to continue.  */
			vortex_log (VORTEX_LEVEL_CRITICAL, 
				    "protocol violation: expected to receive replies to message sent on a closed connection (or the remote peer suddently died)");

			/* from a loop perspective, simulate that all
			 * replies where received */
			result = false;
			break;
		}
	} /* end while */
	
	vortex_mutex_unlock (&channel->close_mutex);
	vortex_log (VORTEX_LEVEL_DEBUG, 
	       "we have received (or it should!) the last reply we can now close the channel %d (MSG %d = RPY %d)", 
	       channel->channel_num, channel->last_message_sent, channel->last_reply_expected - 1);

	/* return wait status */
	return result;
}



/** 
 * @brief Block the caller until all pending replies are sent for the
 * channel provided.
 *
 * @param channel The channel that could contain pending replies to be
 * sent.
 *
 * @param microseconds_to_wait Amount of time to implement the timeout
 * or -1 if the caller is requesting to lock using default timeout
 * provided by vortex_connection_get_timeout.
 *
 * @return true if all pending replies were sent (or no pending
 * replies were found). Otherwise false is returned due to connection
 * problems (\ref vortex_connection_is_ok) or because timeout has been
 * reached.
 */
bool     vortex_channel_block_until_replies_are_sent (VortexChannel * channel, 
						      long int        microseconds_to_wait)
{
	VortexCtx     * ctx     = vortex_channel_get_ctx (channel);
	bool            result  = true;

	/* normalize the value received */
	if (microseconds_to_wait < 0)
		microseconds_to_wait = vortex_connection_get_timeout (ctx);

	/* look the mutex */
	vortex_mutex_lock (&channel->pending_mutex);

	while (channel->last_message_received != (channel->last_reply_written)) {
		
		/* check timeout here */
		if (microseconds_to_wait < 0) {
			vortex_log (VORTEX_LEVEL_WARNING, 
				    "we still didn't sent replies for connection=%d channel=%d, (RPY %d != MSG %d), but timeout has been reached",
				    vortex_connection_get_id (channel->connection),
				    channel->channel_num,
				    (channel->last_reply_written), channel->last_message_received);
			result = false;
			break;
		} /* end if */

		/* drop a log */
		vortex_log (VORTEX_LEVEL_WARNING, 
			    "we still didn't sent replies for connection=%d channel=%d, block until all replies are sent (RPY %d != MSG %d), remaining timeout=%d",
			    vortex_connection_get_id (channel->connection),
			    channel->channel_num,
			    (channel->last_reply_written), channel->last_message_received, microseconds_to_wait);
		
		/* check the connection status to perform a signaled
		 * wait for replies not received */
		if (vortex_connection_is_ok (channel->connection, false)) {

			/* wait */
			VORTEX_COND_TIMEDWAIT (result, 
					       &channel->pending_cond, 
					       &channel->pending_mutex,
					       500000);

			/* remove the amount of time implemented */
			microseconds_to_wait -= 500000;
			result                = true;
		} else {
			/* the connection is closed so, the replies
			 * will never arrive. Because this is a
			 * protocol violation, we can are going to
			 * drop a log to the console and let the close
			 * process to continue.  */
			vortex_log (VORTEX_LEVEL_CRITICAL, 
				    "protocol violation: expected to receive replies to message sent on a closed connection (or the remote peer suddently died)");

			/* from a loop perspective, simulate that all
			 * replies where received */
			result = false;
			break;
		} /* end if */
	} /* end while */

	/* unlock */
	vortex_mutex_unlock (&channel->pending_mutex);
	
	vortex_log (VORTEX_LEVEL_DEBUG, 
	       "we have sent the last reply we can now close the channel %d (RPY %d = MSG %d)", 
	       channel->channel_num, 
	       (channel->last_reply_sent - 1), 
	       channel->last_message_received);

	return result;
}


/*
 * @internal Function which finally implements channel close functions.
 */
axlPointer __vortex_channel_close (VortexChannelCloseData * data)
{
	VortexChannel                  * channel        = NULL;
	VortexChannel                  * channel0       = NULL;
	VortexFrame                    * frame          = NULL;
	VortexConnection               * connection     = NULL;
	WaitReplyData                  * wait_reply     = NULL;
	VortexOnClosedNotification       on_closed      = NULL;
	VortexOnClosedNotificationFull   on_closed_full = NULL;
	axlPointer                       user_data      = NULL;
	
	/* true channel is closed, false not. */
	bool                             result         = false;
	bool                             threaded;
	char                           * code           = NULL;
	char                           * msg            = NULL;
	char                           * close_msg      = NULL;
	int                              msg_no         = -1;
	VortexCtx                      * ctx;
	
	
	/* do not operate if null is received */
	if (data == NULL)
		return NULL;

	/* get a reference to received data (comes from
	 * vortex_channel_close) */
	channel        = data->channel;
	ctx            = vortex_channel_get_ctx (channel);
	on_closed      = data->on_closed;
	on_closed_full = data->on_closed_full;
	user_data      = data->user_data;
	threaded       = data->threaded;

	/* free data */
	axl_free (data);

	/* According to section 2.3.1.3 "The close message" from RFC
	 * 3080.
	 * 
	 * We are going to close a channel, so the process is as
	 * follows:
	 *
	 * 1) Send a close channel msg indication to remote peer.
	 * But, if there are pending message replies to be received,
	 * wait for them until send close message.
	 *
	 * 2) Once close message have been sent, we have to avoid
	 * sending futher message and only be prepared to still
	 * receive more msg from remote peer. Of course we are allowed
	 * to (and we must) reply to this received MSG frame type.
	 *
	 * 3) Once received, we have to check there are not pending
	 * RPY to be sent and then close the channel.
	 *
	 * 4) At the same time we keep on waiting remote peer response
	 * to close indication.
	 *
	 * step 1) */
	connection = channel->connection;
	 
	/* check for a connection already closed */
	if (!vortex_connection_is_ok (connection, false)) {
		vortex_log (VORTEX_LEVEL_DEBUG, 
			    "Detected a session not connected during channel close, stoping BEEP close negotiation and going ahead to resource deallocation...");
		result = true;
		goto __vortex_channel_close_invoke_caller;
	}


	/* before sending close message we have to ensure all replies
	 * to message sent have been received if not, we can't send
	 * the close message (2.3.1.3 The Close Message BEEP Core) */
	vortex_log (VORTEX_LEVEL_DEBUG, "check replies to be received for a close request=%d (application issued)",
		    channel->channel_num);
	if (! __vortex_channel_block_until_replies_are_received (channel)) {
		vortex_log (VORTEX_LEVEL_DEBUG,
			    "an error was found while waiting for replies on a channel being closed, free vortex channel resources");
		result = true;
		goto __vortex_channel_close_invoke_caller;
	}

	/* get a reference to administrative channel */
	channel0 = vortex_connection_get_channel (connection, 0);

	/* check channel 0 being closed */
	if (channel->channel_num == 0) {
		/* flag the connection to be at close processing. We
		 * set this flag so vortex reader won't complain about
		 * not properly connection close.  To close the
		 * channel 0 means to close the beep session. */
		vortex_connection_set_data (connection, "being_closed", INT_TO_PTR (true));
	}

	/* send the close channel message and get the wait reply */
	wait_reply = vortex_channel_get_data (channel, VORTEX_CHANNEL_WAIT_REPLY);

	/* check before sending the close request if we have received
	 * a close in transit while we are requesting to close the
	 * same channel */
	if (vortex_async_queue_items (wait_reply->queue) == 0) {
		
		/* create the close message */
		close_msg  = vortex_frame_get_close_message (channel->channel_num, "200",
							     NULL, NULL);
		vortex_log (VORTEX_LEVEL_DEBUG, "sending close request for channel: %d", channel->channel_num);
		if (!vortex_channel_send_msg_and_wait (channel0, close_msg, strlen (close_msg), 
						       &msg_no, wait_reply)) {
			/* free close message */
			axl_free (close_msg);
			
			/* because be could close the channel unflag the
			 * "being_closed".  This should be only applied when
			 * closing the channel 0 and an error was get, but the
			 * same sentence cause the same effect on other
			 * channels. */
			vortex_connection_set_data (connection, "being_closed", NULL);
			vortex_log (VORTEX_LEVEL_DEBUG, "unable to send close channel message");
			result = false;
			vortex_channel_free_wait_reply (wait_reply);
			goto __vortex_channel_close_invoke_caller;
		}

		/* free close message */
		axl_free (close_msg);
	} else {
		/* good close in transit detected, do not send a close
		 * request for a channel that the remote peer have
		 * requested to close (and we accepted to do so). */
		vortex_log (VORTEX_LEVEL_DEBUG, "close in transit detected, skiping sending close message for channel=%d",
			    channel->channel_num);
	} /* end if */

	/* step 2)
	 *
	 * This step is actually been fulfilled due to being_closed
	 * attribute. This allows to keep on sending rpy to incoming
	 * msg but not to send more msg. 
	 */

	/* step 3)
	 *
	 * ensure we have sent all message replies to remote peer
	 * doing timeout wait with the default value. */
	vortex_channel_block_until_replies_are_sent (channel, -1);

	frame = vortex_channel_wait_reply (channel0, msg_no, wait_reply);
	switch (vortex_frame_get_type (frame)) {
	case VORTEX_FRAME_TYPE_ERR:
		/* remote peer do not agree to close the channel, get
		 * values returned */
		vortex_channel_validate_err (frame, &code, &msg);
		result = false;
		break;
	case VORTEX_FRAME_TYPE_RPY:
		/* remote peer agree to close the channel */
		result = __vortex_channel_close_validate_rpy (channel, frame);
		break;
	default:
		vortex_log (VORTEX_LEVEL_DEBUG, 
			    "unexpected reply found for a close channel=%d indication, remote peer have a buggy implementation",
			    channel->channel_num);
		/* __vortex_connection_set_not_connected (connection,
		   "unexpected reply found for a close channel
		   indication, remote peer have a buggy
		   implementation"); */
		result = true;
		goto __vortex_channel_close_invoke_caller;
	}

	/* set channel to be closed. It result = true channel will be
	 * marked as closed so vortex reader can check if a message
	 * can be actually delivered */
	channel->is_opened = !result;

 __vortex_channel_close_invoke_caller:
	if (threaded) {
		/* invoke handler */
		if(on_closed_full)
			on_closed_full(channel->connection, channel->channel_num, result, code, msg, user_data);
		else {
			if (on_closed)
				on_closed (channel->channel_num, result, code, msg);
		}
	}

	/* free not needed values */
	vortex_support_free (3, code, axl_free, msg, axl_free, frame, vortex_frame_unref);

	/*
	 * Channel close have failed, restore its values to keep
	 * working. 
	 */
	if (!result && (channel->channel_num == 0)) {
		/* unflag the connection to be at close processing. We
		 * unset this flag because the connection were not
		 * closed */
		vortex_connection_set_data (connection, "being_closed", NULL);
	}

	/* remove the channel from the connection */
	if (result) {
		vortex_log (VORTEX_LEVEL_DEBUG, 
		       "because channel=%d over connection id=%d have been accepted to be removed, remove it from its session",
		       channel->channel_num,
		       vortex_connection_get_id (connection));

		/* remove the channel from the connection */
		vortex_connection_remove_channel (connection, channel); 

		/* actually, vortex_connection_remove_channel already
		 * does a vortex_channel_free */
	}
	
	/* return the value */
	if (threaded) {
		/* before exit, reduce the reference counting updated
		 * by the calling thread */
		vortex_connection_unref (connection, "channel close threaded");

		return NULL; /* on threaded mode always returns
			      * NULL */
	}
	return result ? INT_TO_PTR (result) : NULL;
}

#define NOTIFY_CLOSE(channel, status, code, message) do{\
    if (on_closed_full != NULL) {\
        on_closed_full (vortex_channel_get_connection(channel), channel->channel_num, status, code, message, user_data);\
    }\
    if (on_closed != NULL) {\
        on_closed (channel->channel_num, status, code, message);\
    }\
}while(0)

/*
 * @internal Implementation that is used by \ref vortex_channel_close
 * and \ref vortex_channel_close_full.
 */
bool     __vortex_channel_close_full (VortexChannel * channel, 
				      VortexOnClosedNotification on_closed, 
				      VortexOnClosedNotificationFull on_closed_full, 
				      axlPointer user_data)
{
	WaitReplyData          * wait_reply;
	VortexCtx              * ctx   = vortex_channel_get_ctx (channel);
	VortexChannelCloseData * data;
	
	v_return_val_if_fail (channel,                   false);
	v_return_val_if_fail (channel->channel_num >= 0, false);

	vortex_log (VORTEX_LEVEL_DEBUG, "vortex_channel_close of channel #%d",channel->channel_num);

	/* check if the caller is trying to close the channel that was
	 * notified to be closed */
	if (vortex_channel_get_data (channel, VORTEX_CHANNEL_CLOSE_HANDLER_CALLED)) {
		vortex_log (VORTEX_LEVEL_WARNING, 
			    "BEEP application is calling to close a channel that is notified to be closed. You are calling to close a channel from the close handler.");
		return true;
	}

	/* set this channel to be in closed channel process and check
	 * that only one thread call to close the channel */
	vortex_mutex_lock (&channel->close_mutex);
	if (channel->being_closed) {

		/* seems the channel is being closed already */
		vortex_mutex_unlock (&channel->close_mutex);

		/* notify close */
		NOTIFY_CLOSE(channel, false, "554", "Channel in process on being closed before calling to this function.");

		vortex_log (VORTEX_LEVEL_WARNING, "Channel in process on being closed before calling to this function.");
		return true;
	}

	/* flag this thread to be the only one closing the channel */
	channel->being_closed = true;
	vortex_mutex_unlock (&channel->close_mutex);
	
	/* create and register the way reply here to ensure that we
	 * support close in transit */
	wait_reply = vortex_channel_create_wait_reply ();
	vortex_channel_set_data (channel, 
				 /* the key */
				 VORTEX_CHANNEL_WAIT_REPLY, 
				 /* the value */
				 wait_reply);

	/* creates data to be passed in to __vortex_channel_close  */
	data            = axl_new (VortexChannelCloseData, 1);
	data->channel   = channel;
	data->on_closed = on_closed;
	data->on_closed_full = on_closed_full;
	data->user_data = user_data;
	data->threaded  = (on_closed != NULL) || (on_closed_full != NULL);

	/* launch threaded mode */
	if (data->threaded) {
		/* if we are in threaded mode, update the reference
		 * counting to the connection to avoid lossing the
		 * reference during the close activation and an
		 * asynchrous broken connection received */
		if (! vortex_connection_ref (channel->connection, "channel close threaded")) {
			vortex_log (VORTEX_LEVEL_DEBUG, "failed to update reference counting to the connection during channel close, going ahead anyway...");
		} /* end if */

		vortex_thread_pool_new_task (ctx, (VortexThreadFunc) __vortex_channel_close, data);
		return true;
	}

	/* launch blocking mode */
	return (__vortex_channel_close (data) != NULL);
}

/** 
 * @brief Close the given channel, allowing to provide a user defined
 * pointer to be passed to the callback.
 *
 * Free all resources allocated by the channel and tries to closes it
 * if is opened.  Once you have called this function, the channel will
 * no longer be available until it is created again and the function
 * will notify vortex session (the connection associated with this
 * channel) to remove this channel reference from it.
 *
 * In some cases the channel close request could be denied by the
 * remote peer. This may happen if other side have something remaining
 * to be sent while you are trying to close. This means the function
 * will block the caller until the channel close process reply is
 * receive.
 *
 * It is possible to avoid getting blocked on the channel close
 * process by setting the <i>on_close_notification</i> which is called
 * at the end of the process, notifying if channel was closed or not.
 * 
 * In both models, making the blocking close operation or the
 * non-blocking one, using the <i>on_close_notification</i>, if
 * channel is closed, channel resources will be deallocated so channel
 * pointer provided will no longer be pointing to a valid channel.
 * 
 * Another thing about channel close process to keep in mind it that
 * close handler defined at \ref vortex_channel_new or at \ref
 * vortex_profiles_register is NOT executed when \ref
 * vortex_channel_close is called.  Those handlers are actually
 * executed when your channel receive a close indication. This also
 * means this function sends a close indication to remote peer so, if
 * remote peer is also a vortex enabled one and have a close handler
 * defined, then they will be executed.
 * 
 * There are an exception to keep in mind. Channel 0 can not be closed
 * using this function. This channel is hard-wired to the actual
 * session. Channel 0 is closed on session close (\ref
 * vortex_connection_close). If you still want to close channel 0 call
 * \ref vortex_connection_close instead.
 * 
 * During the <i>on_closed</i> handler execution, if defined, you can
 * still get access to the channel being closed. This is provided
 * because you may be interesting on getting the channel status, etc.
 * After <i>on_closed</i> is executed and finished, the channel
 * resources are deallocated.
 * 
 * @param channel The channel to close and dealloc its associated
 * resources.
 *
 * @param on_closed A optional handler notification to get a
 * notification about the final process status. If provided the
 * handler, the function changes its behaviour to a non blocking
 * process, returning the control to the caller as soon as possible.
 *
 * @param user_data An optional user defined pointer that is passed to
 * the <b>on_closed</b> handler provided. This value is ignored if
 * previous handler is not provided.
 * 
 * @return true if the channel was closed, otherwise false is
 * returned. In threaded mode, activated by defining the
 * <i>on_closed</i> handler, the function always return true. Actual
 * close process result is notified on <i>on_closed</i>
 */
bool     vortex_channel_close_full (VortexChannel * channel, 
			       VortexOnClosedNotificationFull on_closed, axlPointer user_data)
{
	return __vortex_channel_close_full (channel, NULL, on_closed, user_data);
}

/** 
 * @brief Close the given channel.
 *
 * See \ref vortex_channel_close_full for more details. This function
 * works the same as the previous one, but without providing a user
 * defined pointer that is passed to the optional handler provided.
 *
 * @param channel the channel to free
 * @param on_closed a optional handler notification
 * 
 * @return true if the channel was closed or false if not. On
 * threaded mode, activated by defining the <i>on_closed</i> handler,
 * always return true. Actual close process result is notified on
 * <i>on_closed</i>
 */
bool     vortex_channel_close (VortexChannel * channel, 
			       VortexOnClosedNotification on_closed)
{
	return __vortex_channel_close_full (channel, on_closed, NULL, NULL);
}



/** 
 * @brief Returns the current profile the channel is running.
 * 
 * You must not free returned value. You can also check \ref
 * vortex_channel_is_running_profile if you are looking to check for
 * some profile to be run under the given channel.
 * 
 * 
 * @param channel the channel to operate on
 * 
 * @return the profile the channel have or NULL if fails
 */
const char    * vortex_channel_get_profile (VortexChannel * channel)
{
	/* check references received */
	if (channel == NULL)
		return NULL;

	return channel->profile;
}

/** 
 * @brief Allows to check if the given channel is running a particular profile.
 *
 * \code
 * // check if my channel is running XML-RPC profile
 * if (vortex_channel_is_running_profile (myChannel, VORTEX_XML_RPC_PROFILE)) {
 *     // sure, it is running the XML-RPC profile
 * }
 * \endcode
 * 
 * @param channel The channel to check for running some particular profile.
 * @param profile The profile to be checked.
 * 
 * @return true if the channel is running the given profile, otherwise false is returned.
 */
bool               vortex_channel_is_running_profile           (VortexChannel * channel,
								const char    * profile)
{
	/* check references received */
	if (channel == NULL || profile == NULL)
		return false;

	return (axl_cmp (profile, vortex_channel_get_profile (channel)));
}

/** 
 * @brief Returns actual channel session (or Vortex Connection). 
 * 
 * @param channel the channel to operate on.
 * 
 * @return the connection where channel was created or NULL if
 * fails.
 */
VortexConnection * vortex_channel_get_connection               (VortexChannel * channel)
{
	/* check references received */
	if (channel == NULL)
		return NULL;

	return channel->connection;
}

/** 
 * @brief Return whenever the received handler have been defined for this
 * channel.
 * 
 * @param channel the channel to check
 * 
 * @return If received handler have been defined returns true if
 * not returns false.
 */
bool            vortex_channel_is_defined_received_handler (VortexChannel * channel)
{
	/* check references received */
	if (channel == NULL)
		return false;

	return (channel->received != NULL);
}


typedef struct _ReceivedInvokeData {
	VortexChannel * channel;
	VortexFrame   * frame;
	int             channel_num;
}ReceivedInvokeData;

/** 
 * @internal Macro that allows to check next message to be delivered
 * using the frame in the context, storing the frame in the provided
 * hash if it is not the next to be delivered.
 */
#define CHECK_AND_STORE_FRAME(mutex, last_msgno_delivered, hash) do{                    \
    /* lock the section with the mutex provided */                                      \
    vortex_mutex_lock (&mutex);                                                         \
                                                                                        \
    /* check store stauts */                                                            \
    status = false;                                                                     \
    switch (vortex_frame_get_type (frame)) {                                            \
    case VORTEX_FRAME_TYPE_RPY:                                                         \
    case VORTEX_FRAME_TYPE_ERR:                                                         \
    case VORTEX_FRAME_TYPE_MSG:                                                         \
	/* status */                                                                    \
	status = last_msgno_delivered != vortex_frame_get_msgno (frame);                \
        index = vortex_frame_get_msgno (frame);                                         \
        break;                                                                          \
    case VORTEX_FRAME_TYPE_ANS:                                                         \
        status = last_msgno_delivered != vortex_frame_get_ansno (frame);                \
        index = vortex_frame_get_ansno (frame);                                         \
        break;                                                                          \
    case VORTEX_FRAME_TYPE_NUL:                                                         \
        /* always store */                                                              \
        status = vortex_frame_get_seqno (frame) != channel->last_ans_seqno_delivered;   \
        if (status) {                                                                   \
              vortex_log (VORTEX_LEVEL_DEBUG,                                           \
			  "storing NUL frame, seqno (%d) != delivered (%d)",            \
			  vortex_frame_get_seqno (frame),                               \
			  channel->last_ans_seqno_delivered);                           \
        }                                                                               \
        index  = -1;                                                                    \
        break;                                                                          \
    default:                                                                            \
        /* default case, do not store anything */                                       \
        status = false;                                                                 \
        index = -1;                                                                     \
    } /* end switch */                                                                  \
                                                                                        \
    /* now according to the status, store or keep woring */                             \
    if (status) {                                                                       \
        /* received frame is not the one expected, store and return */                  \
        axl_hash_insert_full (hash, INT_TO_PTR (index),                                 \
                              NULL, frame,                                              \
                              (axlDestroyFunc) vortex_frame_unref);                     \
        vortex_mutex_unlock (&mutex);                                                   \
        vortex_channel_unref (channel);                                                 \
        vortex_connection_unref (connection, "second level handler (frame received)");  \
                                                                                        \
        /* free received data */                                                        \
        axl_free (data);                                                                \
        vortex_log (VORTEX_LEVEL_WARNING, "store frame because previous one is expected"); \
        return NULL;                                                                    \
    }                                                                                   \
    vortex_mutex_unlock (&mutex);                                                       \
}while(0);


#define CHECK_PENDING_FRAME(mutex, hash, last_msgno_delivered) do{                                                   \
   /* lock */                                                                                                        \
   vortex_mutex_lock (&mutex);                                                                                       \
                                                                                                                     \
   /* check that the reply delivered is not in the hash */                                                           \
   is_ans_frame = (vortex_frame_get_type (frame) == VORTEX_FRAME_TYPE_ANS);                                          \
   /* update last ans seqno delivered */                                                                             \
   if (is_ans_frame) {                                                                                               \
         channel->last_ans_seqno_delivered = vortex_frame_get_seqno (frame) + vortex_frame_get_content_size (frame); \
   }                                                                                                                 \
                                                                                                                     \
   /* remove according to the frame type */                                                                          \
   status = false;                                                                                                   \
   if (vortex_frame_get_type (frame) == VORTEX_FRAME_TYPE_NUL) {                                                     \
         status = axl_hash_remove (hash, INT_TO_PTR (-1));                                                           \
   } else {                                                                                                          \
         status = axl_hash_remove (hash, INT_TO_PTR (last_msgno_delivered));                                         \
   }                                                                                                                 \
                                                                                                                     \
   /* update the last reply received and check if the next */                                                        \
   /* reply is found in the hash  */                                                                                 \
   last_msgno_delivered++;                                                                                           \
                                                                                                                     \
   /* check, unref and nullify the frame */                                                                          \
   if (! status) {                                                                                                   \
        vortex_frame_unref (frame);                                                                                  \
   }                                                                                                                 \
   frame = NULL;                                                                                                     \
                                                                                                                     \
   /* check if the next reply is stored in the hash */                                                               \
   frame = axl_hash_get (hash, INT_TO_PTR(last_msgno_delivered));                                                    \
                                                                                                                     \
   /* check if the frame is null and the previous was an ans frame, to check */                                      \
   /* for the null frame */                                                                                          \
   if (frame == NULL && is_ans_frame) {                                                                              \
	 frame = axl_hash_get (hash, INT_TO_PTR (-1));                                                               \
         if (frame == NULL || (channel->last_ans_seqno_delivered != vortex_frame_get_seqno (frame))) {               \
		 frame = NULL;                                                                                       \
	 } /* end if */                                                                                              \
   } /* end if */                                                                                                    \
                                                                                                                     \
   /* if the frame previously handled was a */                                                                       \
   /* unlock */                                                                                                      \
   vortex_mutex_unlock   (&mutex);                                                                                   \
                                                                                                                     \
   if (frame != NULL) {                                                                                              \
       vortex_log (VORTEX_LEVEL_WARNING, "restored frame that can now be delivered");                    \
       /* deliver the frame as received */                                                                           \
       goto deliver_frame;                                                                                           \
   } /* end if */                                                                                                    \
}while(0);

/** 
 * @internal
 *
 * @brief Support function for vortex_channel_invoke_received_handler.
 *
 * This is the function that finally perform the frame deliverance
 * inside the user space application.
 * 
 * @param data Data to deliver the frame received onto the channel.
 * 
 * @return 
 */
axlPointer __vortex_channel_invoke_received_handler (ReceivedInvokeData * data)
{
	VortexChannel    * channel      = data->channel;
	VortexConnection * connection   = vortex_channel_get_connection (channel);
	VortexFrame      * frame        = data->frame;
	char             * raw_frame    = NULL;
	bool               is_connected;
	
	/* variables used by the CHECK_AND_STORE_FRAME macro */
	bool               status;
	int                index;
	bool               is_ans_frame;
	bool               is_nul_frame;
	VortexFrameType    type;

	/* get a reference to channel number so we can check after
	 * frame received handler if the channel have been closed.
	 * Once the frame received have finished this will help us to
	 * know if application space have issued a close channel. */
	int               channel_num   = data->channel_num;
	VortexCtx       * ctx           = vortex_channel_get_ctx (channel);
	
 	if (vortex_log_is_enabled (ctx)) {
 		/* get type */
 		type = vortex_frame_get_type (frame);
 		vortex_log (VORTEX_LEVEL_DEBUG, "STARTED the frame received handler invocation (new task): %s%s%s%s%s %d %d %s %d %d",
 			    (type == VORTEX_FRAME_TYPE_MSG) ? "MSG" : "",
 			    (type == VORTEX_FRAME_TYPE_RPY) ? "RPY" : "",
 			    (type == VORTEX_FRAME_TYPE_ERR) ? "ERR" : "",
 			    (type == VORTEX_FRAME_TYPE_ANS) ? "ANS" : "",
 			    (type == VORTEX_FRAME_TYPE_NUL) ? "NUL" : "",
 			    /* channel */
 			    vortex_frame_get_channel (frame),
 			    /* msg no */
 			    vortex_frame_get_msgno   (frame),
 			    /* more indicator */
 			    vortex_frame_get_more_flag (frame) == 1 ? "*" : ".",
 			    /* seq no */
 			    vortex_frame_get_seqno   (frame),
 			    /* message size */
 			    vortex_frame_get_content_size (frame));
 	}

	/* show received frame */
	if (vortex_log2_is_enabled (ctx)) {
		raw_frame = vortex_frame_get_raw_frame (frame);
		vortex_log (VORTEX_LEVEL_DEBUG, 
		       "second level frame receive invocation for connection=%d channel=%d channel_received=%d (seqno: %d size: %d)\n%s",
		       vortex_connection_get_id (connection),
		       channel_num, channel->channel_num, 
		       vortex_frame_get_seqno (frame),
		       vortex_frame_get_content_size (frame),
		       raw_frame);
		axl_free (raw_frame);
	}

	/* record actual connection state */
	is_connected = vortex_connection_is_ok (connection, false);
	if (! is_connected) {
		vortex_log (VORTEX_LEVEL_CRITICAL, "invoking frame receive on a non-connected session");
		goto free_resources;
	}

	/* check to enforce serialize */
	if (channel->serialize) {
		if (vortex_frame_get_type (frame) == VORTEX_FRAME_TYPE_RPY ||
		    vortex_frame_get_type (frame) == VORTEX_FRAME_TYPE_ERR) {
			/* check that the frame to deliver is the one next
			 * expected */
			CHECK_AND_STORE_FRAME(channel->serialize_rpy_mutex, 
					      channel->last_reply_delivered,
					      channel->serialize_rpy);
		} else if (vortex_frame_get_type (frame) == VORTEX_FRAME_TYPE_MSG) {
			/* check that the frame to deliver is the one
			 * expected */
			CHECK_AND_STORE_FRAME(channel->serialize_msg_mutex,
					      channel->last_msg_delivered,
					      channel->serialize_msg);

		} else if (vortex_frame_get_type (frame) == VORTEX_FRAME_TYPE_ANS ||
			   vortex_frame_get_type (frame) == VORTEX_FRAME_TYPE_NUL) {
			/* check that the frame to deliver is the one
			 * expected */
			CHECK_AND_STORE_FRAME(channel->serialize_ans_mutex,
					      channel->last_ans_delivered,
					      channel->serialize_ans);
		} /* end if */
	} /* end if */

 deliver_frame:

	/* invoke handler */
	if (channel->received) {
		channel->received (channel, channel->connection, frame, 
				   channel->received_user_data);
 		if (vortex_log_is_enabled (ctx)) {
 			/* get type */
 			type = vortex_frame_get_type (frame);
 			vortex_log (VORTEX_LEVEL_DEBUG, "frame received invocation for second level FINISHED (new task): %s%s%s%s%s %d %d %s %d %d",
 				    (type == VORTEX_FRAME_TYPE_MSG) ? "MSG" : "",
 				    (type == VORTEX_FRAME_TYPE_RPY) ? "RPY" : "",
 				    (type == VORTEX_FRAME_TYPE_ERR) ? "ERR" : "",
 				    (type == VORTEX_FRAME_TYPE_ANS) ? "ANS" : "",
 				    (type == VORTEX_FRAME_TYPE_NUL) ? "NUL" : "",
 				    /* channel */
 				    vortex_frame_get_channel (frame),
 				    /* msg no */
 				    vortex_frame_get_msgno   (frame),
 				    /* more indicator */
 				    vortex_frame_get_more_flag (frame) == 1 ? "*" : ".",
 				    /* seq no */
 				    vortex_frame_get_seqno   (frame),
 				    /* message size */
 				    vortex_frame_get_content_size (frame));
 		} /* end if */
	}else {
		vortex_log (VORTEX_LEVEL_CRITICAL, "invoking frame received on channel %d with not handler defined",
		       vortex_channel_get_number (channel));
	}

	/* check serialize to broadcast other waiting threads */
	if (channel->serialize) {

		if (vortex_frame_get_type (frame) == VORTEX_FRAME_TYPE_RPY ||
		    vortex_frame_get_type (frame) == VORTEX_FRAME_TYPE_ERR) {

			/* check frames stored pending to be delivered */
			CHECK_PENDING_FRAME(channel->serialize_rpy_mutex,
					    channel->serialize_rpy,
					    channel->last_reply_delivered);

		} else if (vortex_frame_get_type (frame) == VORTEX_FRAME_TYPE_MSG) {

			/* check frames stored pending to be delivered */
			CHECK_PENDING_FRAME(channel->serialize_msg_mutex,
					    channel->serialize_msg,
					    channel->last_msg_delivered);

		} else if (vortex_frame_get_type (frame) == VORTEX_FRAME_TYPE_ANS ||
			   vortex_frame_get_type (frame) == VORTEX_FRAME_TYPE_NUL) {

			/* record nul type frame status */
			is_nul_frame = (vortex_frame_get_type (frame) == VORTEX_FRAME_TYPE_NUL);

			/* check frames stored pending to be delivered */
			CHECK_PENDING_FRAME(channel->serialize_ans_mutex,
					    channel->serialize_ans,
					    channel->last_ans_delivered);

			/* reset ANS/NUL delivery status */
			if (is_nul_frame)
				channel->last_ans_delivered = 0;
		} /* end if */
	}

	/* before frame receive handler we have to check if client
	 * have closed its connection */
	is_connected = vortex_connection_is_ok (connection, false);
	if (! is_connected ) {
		/* the connection have been closed inside frame receive */
		goto free_resources;
	}

	/* The function __vortex_channel_0_frame_received_close_msg
	 * can be blocked awaiting to receive all replies
	 * expected. The following signal tries to wake up a possible
	 * thread blocked until last_reply_expected change. */
	if (vortex_connection_channel_exists (connection, channel_num))
		vortex_channel_signal_on_close_blocked (channel);
	else
		vortex_log (VORTEX_LEVEL_DEBUG, "channel=%d was closed during frame receive execution, unable to signal on close",
		       channel_num);

	/* free data and frame */
 free_resources:

	/* log a message */
	vortex_log (VORTEX_LEVEL_DEBUG, 
	       "invocation frame received handler for channel %d finished (second level: channel), ref count: channel=%d connection=%d", 
		    channel_num, channel->ref_count, vortex_connection_ref_count (connection));
	
	/* update channel reference */
	vortex_channel_unref (channel);

	/* decrease connection reference counting */
	vortex_connection_unref (connection, "second level handler (frame received)");

	vortex_frame_unref (frame);
	axl_free           (data);

	return NULL;
}



/** 
 * @internal
 * @brief Invokes received handler on this channel. 
 * 
 * This handler is mainly used by vortex reader process at function
 * vortex_reader_process_socket.
 *
 * This enable vortex reader to notify that a frame have been received
 * on a particular channel. This function also check if there are
 * waiting thread blocked at vortex_channel_wait_reply. This means
 * there are threads waiting to a RPY frame type or ERR frame type to
 * be received.
 *
 * If no wait reply is found for a particular RPY or ERR, then the
 * default channel frame received handler is invoked.
 *
 * Frame comes to this function in a ordered manner. Vortex reader
 * thread filter all message received on registered connection to
 * detect channel synchronization failures. So, MSG received are
 * already ordered when they comes to this function and RPY, ERR
 * frames are also ordered.
 *
 * @param channel  The channel where frame received invocation will happen
 * @param frame    The frame actually producing the invocation
 * 
 * @return true if frame was delivered or false if not.
 */
bool          vortex_channel_invoke_received_handler (VortexConnection * connection, VortexChannel * channel, VortexFrame * frame)
{
	ReceivedInvokeData * data;
	WaitReplyData      * wait_reply;
	VortexCtx          * ctx     = vortex_channel_get_ctx (channel);
	
	/* check data to avoid self-encrintation ;-) */
	if (channel == NULL || frame == NULL)
		return false;

	/* query if we have a waiting queue */
	switch (vortex_frame_get_type (frame)) {
	case VORTEX_FRAME_TYPE_RPY:
	case VORTEX_FRAME_TYPE_ERR:
		/* lock the channel during channel operation */
		vortex_mutex_lock (&channel->receive_mutex);

		/* we have a reply message: rpy or err. this means we
		   can check if there are waiting thread */
		if ((wait_reply = vortex_queue_peek (channel->waiting_msgno)) == NULL) {
			vortex_mutex_unlock (&channel->receive_mutex);
			break;
		}
		/* it seems we have waiting threads, so check if
		   received frame have a msgno for the next waiting
		   queue */
		if (vortex_frame_get_msgno (frame) != wait_reply->msg_no_reply) {
			vortex_mutex_unlock (&channel->receive_mutex);
			break;
		}

		/* ok, we have found there are a thread waiting, so
		 * push data */
		vortex_log (VORTEX_LEVEL_DEBUG, "found a frame that were waited by a thread: reply no='%d' over channel=%d",
		       vortex_frame_get_msgno (frame),
		       channel->channel_num);

		/* remove waiting reply data */
		vortex_queue_pop (channel->waiting_msgno);

		/* queue frame received */
		QUEUE_PUSH (wait_reply->queue, frame);

		/* decrease wait reply reference counting */
		vortex_channel_free_wait_reply (wait_reply);

		/* unlock the channel */
		vortex_mutex_unlock (&channel->receive_mutex);

		return true;
	default:
		/* do nothing */
		break;
	}
	vortex_log (VORTEX_LEVEL_DEBUG, "no waiting queue for this message: %d", 
	       vortex_frame_get_msgno (frame));
	
	/* no waiting queue, invoke frame received handler so create
	 * the thread on this newly created thread, the frame will be
	 * deleted */
	if (channel->received == NULL) {
		vortex_log (VORTEX_LEVEL_DEBUG, "it seems you didn't define a second level received handler");
		return false;
	}

	/* prepare data to be passed in to thread */
	data              = axl_new (ReceivedInvokeData, 1);
	data->channel     = channel;
	data->frame       = frame;
	data->channel_num = channel->channel_num;

	/* increase connection reference counting to avoid reference
	 * loosing if connection is closed */
	if (! vortex_connection_ref (connection, "second level invocation (frame received)")) {
		/* unable to increate reference for the connection */
		vortex_log (VORTEX_LEVEL_CRITICAL, "unable to increase connection reference, avoiding delivering data (dropping frame)..");

		/* deallocate resources */
		vortex_frame_unref (frame);
		axl_free (data);
		return false;
	}

	/* also update channel references */
	vortex_channel_ref (channel);

	/* create the thread to invoke frame received handler */
	vortex_log (VORTEX_LEVEL_DEBUG, "about to invoke the frame received under a newly created handler");
	vortex_thread_pool_new_task (ctx, (VortexThreadFunc)__vortex_channel_invoke_received_handler, data);
	return true;

}

/** 
 * @brief Returns if the given channel have defined its close handler.
 * 
 * @param channel the channel to operate on
 * 
 * @return true if close handler is defined or false if not.
 */
bool            vortex_channel_is_defined_close_handler (VortexChannel * channel)
{
	/* check references received */
	if (channel == NULL)
		return false;
	
	return (channel->close != NULL);
}

/** 
 * @internal
 * @brief Invoke the close handler for the given channel if were defined. 
 * 
 * @param channel the channel to operate on
 * 
 * @return what have returned close handler invocation
 */
bool                vortex_channel_invoke_close_handler     (VortexChannel  * channel)
{
	/* check references received */
	if (channel == NULL || channel->close == NULL)
		return false;

	return channel->close (channel->channel_num, channel->connection, 
			       channel->close_user_data);
}

/** 
 * @internal
 * @brief Internal Vortex Library function to validate received frames on channel 0.
 * 
 * @param channel0 the channel 0 itself
 * @param frame the frame received
 * 
 * @return true if validated was ok or false if it fails.
 */
bool     __vortex_channel_0_frame_received_validate (VortexChannel * channel0, VortexFrame * frame)
{
	axlDtd      * channel_dtd;
	axlDoc      * doc;
	axlError    * error;
	char        * error_msg;
	VortexCtx   * ctx     = vortex_channel_get_ctx (channel0);

	/* get channel management DTD */
	if ((channel_dtd = vortex_dtds_get_channel_dtd (ctx)) == NULL) {
		vortex_log (VORTEX_LEVEL_DEBUG, "unable to load dtd file, cannot validate incoming message, returning error frame");
		error_msg = vortex_frame_get_error_message ("421", 
							    "service not available, unable to load dtd file, cannot validate incoming message",
							    NULL);
		vortex_channel_send_err (channel0, 
					 error_msg,
					 strlen (error_msg),
					 vortex_frame_get_msgno (frame));
		axl_free (error_msg);
		return false;
	}

	/* parser xml document */
	doc = axl_doc_parse (vortex_frame_get_payload (frame), 
			     vortex_frame_get_payload_size (frame), &error);
	if (!doc) {
		/* report an error to the remote peer */
		error_msg = vortex_frame_get_error_message ("500", "general syntax error: xml parse error", NULL);
		vortex_channel_send_err (channel0, error_msg, strlen (error_msg), vortex_frame_get_msgno (frame));
		axl_free (error_msg);

		/* flag an error */
		vortex_log (VORTEX_LEVEL_CRITICAL, 
		       "received message on channel 0 with xml parse error, closing connection: %s",
		       axl_error_get (error));
		axl_error_free (error);

		/* drop the connection */
		__vortex_connection_set_not_connected (vortex_channel_get_connection (channel0),
						       "general syntax error: xml parse error");
		return false;		 
	}
	
	/* validate document */
	if (! axl_dtd_validate (doc, channel_dtd, &error)) {
		/* Validation failed */
		error_msg = vortex_frame_get_error_message ("501", "syntax error in parameters: non-valid XML", NULL);
		vortex_channel_send_err (channel0, error_msg, strlen (error_msg), vortex_frame_get_msgno (frame));
		axl_free (error_msg);

		/* set the channel to be not connected */
		__vortex_connection_set_not_connected (vortex_channel_get_connection (channel0),
						       "501 syntax error in parameters: non-valid XML");

		/* report a log and free the error reported */
		vortex_log (VORTEX_LEVEL_CRITICAL, 
		       "received message on channel 0 with syntax error in parameters: non-valid XML: %s",
		       axl_error_get (error));
		axl_error_free (error);

		/* release the document read */
		axl_doc_free (doc);
		return false;		 
	}
	
	/* release the document read */
	axl_doc_free (doc);
	return true;
}

enum {START_MSG, CLOSE_MSG, ERROR_MSG, OK_MSG};


/** 
 * @internal
 * @brief Identifies the frame type received over channel 0.
 * 
 * @param channel0 the channel 0 itself
 * @param frame the frame received
 * 
 * @return the frame type received.
 */
int  __vortex_channel_0_frame_received_identify_type (VortexChannel * channel0, VortexFrame * frame)
{
	axlDoc     * doc;
	char       * error_msg;
	axlNode    * node;
	axlError   * error;
	int          result = -1;
	char       * payload;

	/* try to perform simple check, if not, do an xml parse
	 * operation, taking the checking to the xml level */
	payload = (char*) vortex_frame_get_payload (frame);
	if (axl_memcmp (payload, "<start", 6))
		return START_MSG;
	if (axl_memcmp (payload, "<close", 6))
		return CLOSE_MSG;
	if (axl_memcmp (payload, "<error", 6))
		return ERROR_MSG;
	if (axl_memcmp (payload, "<ok", 3))
		return OK_MSG;

	/* seems fast parser doesn't work, try to parse the content
	 * into the xml level */

	/* parser xml document */
	doc = axl_doc_parse (payload, vortex_frame_get_payload_size (frame), &error);
	if (!doc) {
		/* report a error to the remote peer */
		error_msg = vortex_frame_get_error_message ("500", "general syntax error: xml parse error", NULL);
		vortex_channel_send_err (channel0, error_msg, strlen (error_msg),
					 vortex_frame_get_msgno (frame));
		axl_free (error_msg);

		/* free the error reported */
		axl_error_free (error);
		return false;		 
	}

	
	/* Get the root element ( ? element) */
	node = axl_doc_get_root (doc);
	if (NODE_CMP_NAME (node, "start"))
		result = START_MSG;
	if (NODE_CMP_NAME (node, "close"))
		result = CLOSE_MSG;
	if (NODE_CMP_NAME (node, "error"))
		result = ERROR_MSG;
	if (NODE_CMP_NAME (node, "ok"))
		result = OK_MSG;

	/* return value found */
	axl_doc_free (doc);
	return result;
}


/** 
 * @internal
 * @brief Parses and validates the start message received.
 * 
 * @param frame The frame received
 * @param channel_num 
 * @param profile 
 * 
 * @return 
 */
bool     __vortex_channel_0_frame_received_get_start_param (VortexFrame    * frame,
							    int            * channel_num,
							    char          ** profile,
							    char          ** profile_content,
							    char          ** serverName,
							    VortexEncoding * encoding)
{
	axlDoc        * doc;
	axlNode       * start;
	axlNode       * profile_node;
	const char    * channel;
	const char    * enc;
#if defined(ENABLE_VORTEX_LOG)
	VortexCtx     * ctx     = vortex_frame_get_ctx (frame);
#endif

	/* parser xml document */
	doc = axl_doc_parse (vortex_frame_get_payload (frame), 
			     vortex_frame_get_payload_size (frame), NULL);
	if (!doc) {
		return false;
	}

	/* Get the root element (start element): this is always the
	 * start element. */
	start           = axl_doc_get_root (doc);

	/* get channel number from the start element property: the
	 * function return a reference so it is not necessary to
	 * deallocate it */
	channel         = axl_node_get_attribute_value (start, "number");
	(* channel_num) = atoi (channel);

	/* get serverName value from the start element property */
	(* serverName)  = axl_node_get_attribute_value_copy (start, "serverName");

	/* Get the position of the first child the start element has
	 * and check if it is the profile element. This is done
	 * because libxml handle the as a node all characters found
	 * between the <start> and the next <profile> element.
	 * Because the message could be built using either
	 * <start><profile.. or <start>\x0D\x0Da<profile>, this must
	 * be checked. */
	profile_node    = axl_node_get_child_nth (start, 0);
	
	/* get profiles */
	(* profile)     = axl_node_get_attribute_value_copy (profile_node, "uri");

	vortex_log (VORTEX_LEVEL_DEBUG, "profile received %s", (* profile));

	/* get current encoding */
	enc        = axl_node_get_attribute_value (start, "encoding");
	if (enc == NULL) 
		(* encoding) = EncodingNone;
	else if (axl_cmp (enc, "none"))
		(* encoding) = EncodingNone;
	else if (axl_cmp (enc, "base64"))
		(* encoding) = EncodingBase64;
	else
		(* encoding) = EncodingUnknown;

	/* get profile content, the piggyback, setting as default a
	 * NULL value and updating it to the value found if defined */
	(* profile_content ) = axl_node_get_content_copy (profile_node, NULL);
	
	/* free the axl document */
	axl_doc_free (doc);
	return true;

}

/** 
 * @internal
 * @brief Support function for \ref __vortex_channel_0_frame_received_start_msg. 
 * 
 * It allows to invoke current start channel handler providing
 * additional data needed for the process, that is, the serverName, the
 * profile content and its encoding.
 * 
 * @param profile         The profile notified for its creation
 * @param channel_num     The channel number requested to be created.
 * @param connection      The connection where the channel will be created.
 * @param serverName      The optional serverName start message attribute content
 * @param profile_content The optional profile content.
 * @param encoding        The optional profile content encoding.
 * 
 * @return true if the channel is accepted to be created, or false if channel couldn't be created.
 */
bool     __vortex_channel_0_frame_received_start_msg_support_invoke (char             * profile, 
								     int                channel_num, 
								     VortexConnection * connection,
								     char             * serverName,
								     char             * profile_content,
								     char            ** profile_content_reply,
								     VortexEncoding     encoding)
{
	bool         result;

	/* invoke the start channel handler */
	result = vortex_profiles_invoke_start (profile, channel_num, connection,
					       serverName, profile_content, 
					       profile_content_reply, encoding);

	/* deallocate unused memory (do not remove profile, it is used on
	 * the next steps) */
	axl_free (profile_content);

	return result;
	
}

/** 
 * @internal Implementation for the vortex_channel_notify_start that
 * do not use a hash value to know the msg_no value.
 */
bool vortex_channel_notify_start_internal (const char       * serverName, 
					   VortexConnection * conn,
					   VortexChannel    * channel0,
					   VortexChannel    * new_channel, 
					   const char       * profile_content_reply,  
					   int                msg_no,
					   bool               action)
{
	char        * start_rpy;
	const char  * profile;
	bool          result = true;
#if defined(ENABLE_VORTEX_LOG)
	VortexCtx   * ctx     = vortex_channel_get_ctx (channel0);
#endif

	/* according to the action received, close the channel */
	if (! action) {
		/* build and report error message */
		if (profile_content_reply == NULL) {
			/* channel not accepted, send error reply */
			start_rpy = vortex_frame_get_error_message ("421", 
								    "service not available: channel can not be created because application level have denied it, unable to start channel requested", 
								    NULL);
		}else {
			/* use as error message the one provided by
			 * the profile implementation */
			start_rpy = axl_strdup (profile_content_reply);
		} /* end if */

		/* send the error message */
		if (! vortex_channel_send_err (channel0, start_rpy, strlen (start_rpy), msg_no)) {
			vortex_log (VORTEX_LEVEL_CRITICAL, "failed to send channel 0 reply (deferred notify start)");
			result = false;
		}  /* end if */
			

		/* deallocate the channel created */
		vortex_connection_remove_channel (conn, new_channel);

		/* free references */
		axl_free (start_rpy);

		return result;
	} /* end if */

	/* configure the server name value (if it is already
	 * configured due to previous channel configuration, the
	 * function does nothing) */
	if (serverName)
		vortex_connection_set_server_name (conn, serverName);

	/* check references received */
	profile   = vortex_channel_get_profile (new_channel);

	vortex_log (VORTEX_LEVEL_DEBUG, "channel %d with profile %s successfully created", 
		    vortex_channel_get_number (new_channel), profile);

	/* flag the channel to be connected */
	new_channel->is_opened = true;

	/* build start reply here using the optional content profile
	 * reply */
	start_rpy = vortex_frame_get_start_rpy_message (profile, profile_content_reply);

	/* send rpy and then create the channel */
	if (!vortex_channel_send_rpy (channel0, 
				      start_rpy, strlen (start_rpy), msg_no)) {
		vortex_log (VORTEX_LEVEL_DEBUG, "an error have happen while sending start channel reply, closing channel");
		result = false;
	} 


	/* free start reply */
	axl_free (start_rpy);

	return result;
}

/** 
 * @brief Allows to perform a notification for a channel start that
 * was deferred.
 *
 * @param new_channel The channel reference which is about to be
 * accepted.
 *
 * @param profile_content_reply The optional profile content reply to
 * send to the requester.
 *
 * @param action This is the status value, according to this the
 * channel is accepted or not.
 *
 * @return true if the notification was done (no matter the value
 * provided for action).
 */
bool vortex_channel_notify_start (VortexChannel    * new_channel,
				  const char       * profile_content_reply,
				  bool               action)
{
	VortexConnection * conn;
	VortexChannel    * channel0;
	int                msg_no;
	const char       * serverName;

	/* check references received */
	if (new_channel == NULL)
		return false;

	/* get a reference to the connection */
	conn       = vortex_channel_get_connection (new_channel);
	
	/* check connection status */
	v_return_val_if_fail (vortex_connection_is_ok (conn, false), false);

	channel0   = vortex_connection_get_channel (conn, 0);
	msg_no     = PTR_TO_INT (vortex_channel_get_data (new_channel, "_vo:ch:msg_no"));
	serverName = vortex_channel_get_data (new_channel, "_vo:ch:srvnm");

	/* call to notify start status */
	return vortex_channel_notify_start_internal (serverName,
						     conn,
						     channel0,
						     new_channel,
						     profile_content_reply,
						     msg_no,
						     action);
}

/** 
 * @brief Allows to configure the channel provided to defer its reply
 * for an incoming start channel request received. This function is
 * mainly used from inside the start handler (\ref
 * VortexOnStartChannel or \ref VortexOnStartChannelExtended), to notify
 * the vortex engine to not perform any reply at this moment.
 *
 * Later the application must call to \ref vortex_channel_notify_start
 * to notify to the remote BEEP peer if the channel is accepted or
 * not.
 *
 */
void               vortex_channel_defer_start                    (VortexChannel    * channel)
{
	/* check references received */
	if (channel == NULL)
		return;

	/* get the result just after calling to set */
	vortex_channel_set_data (channel, "_vo:ch:defer", INT_TO_PTR (true));
	
	return;
}

/** 
 * @internal
 *
 * @brief Dispatch an start message received on the given channel
 * invoking start handlers defined.
 * 
 * @param channel0 The channel where the start message was received.
 * @param frame    The frame containing the frame received.
 */
void __vortex_channel_0_frame_received_start_msg (VortexChannel * channel0, VortexFrame * frame)
{
	int                channel_num           = -1;
	char             * profile               = NULL;
	char             * profile_content       = NULL;
	char             * profile_content_reply = NULL;
	char             * serverName            = NULL;
	char             * error_msg             = NULL;
	char             * aux                   = NULL;
	char             * start_rpy             = NULL;
	VortexEncoding     encoding;       
	VortexChannel    * new_channel           = NULL;
	VortexConnection * connection            = channel0->connection;
	bool               status;
	VortexCtx        * ctx                   = vortex_channel_get_ctx (channel0);
	
	/* get and check channel num and profile */
	if (!__vortex_channel_0_frame_received_get_start_param (frame, &channel_num, 
								&profile, &profile_content, 
								&serverName, &encoding)) {
		error_msg = vortex_frame_get_error_message ("500", "unable to get channel start param", NULL);
		vortex_channel_send_err (channel0, error_msg, strlen (error_msg), vortex_frame_get_msgno (frame));
		axl_free (error_msg);
		return;
	}

	vortex_log (VORTEX_LEVEL_DEBUG, 
	       "start message received: channel='%d' profile='%s' serverName='%s' profile_content='%s' encoding='%s'", 
	       channel_num, profile,
	       (serverName != NULL) ? serverName : "",
	       (profile_content != NULL) ? profile_content : "",
	       (encoding == EncodingNone) ? "none" : "base64");

	/* check if channel exists */
	if (vortex_connection_channel_exists (connection, channel_num)) {
		/* send an error reply */
		error_msg = vortex_frame_get_error_message ("554", "transaction failed: channel requested already exists", NULL);
		vortex_channel_send_err (channel0, error_msg, strlen (error_msg), vortex_frame_get_msgno (frame));

		/* deallocate unused memory */
		vortex_support_free (4, error_msg,    axl_free, 
				     profile,         axl_free, 
				     serverName,      axl_free, 
				     profile_content, axl_free);
		return;
	}

	/* check if the connection have masked the profile recevied */
	if (vortex_connection_is_profile_filtered (connection, channel_num, profile, profile_content, serverName, &aux)) {
		/* send an error reply */
		if (aux == NULL)
			error_msg = vortex_frame_get_error_message ("554", "transaction failed: requested profile is not available on the connection", NULL);
		else
			error_msg = vortex_frame_get_error_message ("554", aux, NULL);
		vortex_channel_send_err (channel0, error_msg, strlen (error_msg), vortex_frame_get_msgno (frame));

		/* deallocate unused memory */
		vortex_support_free (5, 
				     error_msg,       axl_free, 
				     profile,         axl_free, 
				     serverName,      axl_free, 
				     profile_content, axl_free,
				     aux,             axl_free);
		return;
	} /* end if */

	/* check if profile already exists */
	if (!vortex_profiles_is_registered (ctx, profile)) {
		vortex_log (VORTEX_LEVEL_WARNING, "received request for an unsupported profile=%s, denying request",  profile);

		/* send an error reply */
		error_msg = vortex_frame_get_error_message ("554", "transaction failed: channel profile requested not supported", NULL);
		vortex_channel_send_err (channel0, error_msg, strlen (error_msg), vortex_frame_get_msgno (frame));

		/* deallocate unused memory */
		vortex_support_free (4, error_msg,    axl_free, 
				     profile,         axl_free, 
				     serverName,      axl_free, 
				     profile_content, axl_free);
		return;
	}

	/* ask if we have an start handler defined */
	if (!vortex_profiles_is_defined_start (ctx, profile)) {
		/* send an error reply */
		error_msg = vortex_frame_get_error_message ("421", "service not available: channel can not be created, no start handler defined", NULL);
		vortex_channel_send_err (channel0, error_msg, strlen (error_msg), vortex_frame_get_msgno (frame));

		/* deallocate unused memory */
		vortex_support_free (4, error_msg,    axl_free, 
				     profile,         axl_free, 
				     serverName,      axl_free, 
				     profile_content, axl_free);
		return;
	}

	/* create the channel reference and set it to the connection
	 * to allow the start handler implementation to handle channel
	 * reference. If start handler fails, the channel is
	 * deallocated. Flag the channel to be not connected.*/
	new_channel = vortex_channel_empty_new (channel_num, profile, connection);
	vortex_connection_add_channel (connection, new_channel);
	new_channel->is_opened = false;
	

	/* ask if channel can be created, (before this function it is
	 * not needed to call any deallocation code for profile,
	 * profile_content and serverName. This is already done by the
	 * following function. profile variable is still needed to be
	 * deallocated. */
	status = __vortex_channel_0_frame_received_start_msg_support_invoke (profile, channel_num, connection,
									     serverName, profile_content, 
									     &profile_content_reply, encoding);
	/* if the channel start is deferred, flag the msgno and do not
	 * reply */
	if (PTR_TO_INT (vortex_channel_get_data (new_channel, "_vo:ch:defer"))) {
		/* configure msg_no to reply and the serverName value,
		 * this will be used by vortex_channel_notify_start */
		vortex_channel_set_data      (new_channel, "_vo:ch:msg_no", INT_TO_PTR (vortex_frame_get_msgno (frame)));

		/* configure the serverName */
		if (serverName)
			vortex_channel_set_data_full (new_channel, "_vo:ch:srvnm", axl_strdup (serverName), NULL, axl_free);
		goto free_channel_start_data;
	}
	
	/* after checking status replied, check if the channel start
	 * reply has been deferred */

	/* if the application level denies the channel creation reply error */
	if (! status) {

		/* build and report error message */
		if (profile_content_reply == NULL) {
			/* no profile content created, create one */
			error_msg = vortex_frame_get_error_message ("421", 
								    "service not available: channel can not be created because application level have denied it, unable to start channel requested", 
								    NULL);
		}else {
			/* use as error message the one provided by
			 * the profile implementation */
			error_msg = profile_content_reply;
		} /* end if */

		/* send the error message */
		vortex_channel_send_err (channel0, error_msg, strlen (error_msg), vortex_frame_get_msgno (frame));

		/* deallocate memory requested */
		vortex_support_free (3, serverName, axl_free, error_msg, axl_free, profile, axl_free);

		/* deallocate the channel created */
		vortex_connection_remove_channel (connection, new_channel);

		return;
	}
	
	/* call to notify that the channel must be started */
	vortex_channel_notify_start_internal (serverName, 
					      connection, 
					      channel0, 
					      new_channel, 
					      profile_content_reply,  
					      vortex_frame_get_msgno (frame), true);

	/* deallocate memory deallocated */
 free_channel_start_data:
	vortex_support_free (4, serverName, axl_free, start_rpy, axl_free, profile, axl_free, profile_content_reply, axl_free);
	return;
}


/** 
 * @internal
 * 
 * @param frame 
 * @param channel_num 
 * @param code 
 * 
 * @return 
 */
bool     __vortex_channel_0_frame_received_get_close_param (VortexFrame * frame,
							    int  * channel_num,
							    char  ** code)
{
	axlDoc      * doc;
	axlNode     * close;
	const char  * channel;

	/* parser xml document */
	doc = axl_doc_parse (vortex_frame_get_payload (frame), 
			     vortex_frame_get_payload_size (frame), NULL);
	if (!doc) 
		return false;

	/* Get the root element (close element) */
	close           = axl_doc_get_root (doc);
	channel         = axl_node_get_attribute_value (close, "number");
	(* channel_num) = atoi (channel);

	(* code )       = axl_node_get_attribute_value_copy (close, "code");
	
	/* release the document */
	axl_doc_free (doc);

	return true;
}


/** 
 * @internal
 * 
 * Looking for the code which actually process close messages received
 * on the channel 0. You have just hit it!
 *
 * This function process the close messages received for the channel
 * 0.
 * 
 * @param channel0 The channel where the message was received.
 * @param frame    The frame actually received.
 */
void __vortex_channel_0_frame_received_close_msg (VortexChannel * channel0, 
						  VortexFrame   * frame)
{
	
	int                channel_num     = -1;
	char             * code            = NULL;
	VortexChannel    * channel;
	VortexConnection * connection      = vortex_channel_get_connection (channel0);
	char             * error_msg       = NULL;
	bool               close_value     = false;
	WaitReplyData    * wait_reply;
	VortexFrame      * ok_frame;
	VortexCtx        * ctx             = vortex_channel_get_ctx (channel0);
	
	/* get and check channel num and profile */
	if (!__vortex_channel_0_frame_received_get_close_param (frame, &channel_num, &code)) {
		/* build error reply */
		error_msg = vortex_frame_get_error_message ("500", "unable to get channel close param", NULL);
		vortex_channel_send_err (channel0, error_msg, strlen (error_msg), vortex_frame_get_msgno (frame));
		axl_free (error_msg);
		return;
	}
	vortex_log (VORTEX_LEVEL_DEBUG, "close message received: channel='%d' code='%s'",  channel_num, code);

	/* code actually not used */
	axl_free (code);

	/* check if channel exists */
	if (!vortex_connection_channel_exists (connection, channel_num)) {
		vortex_log (VORTEX_LEVEL_WARNING, "received a request to close a channel=%d which doesn't exist",
			    channel_num);
		error_msg = vortex_frame_get_error_message ("554", "transaction failed: channel requested to close doesn't exists", NULL);
		vortex_channel_send_err (channel0, error_msg, strlen (error_msg), vortex_frame_get_msgno (frame));
		axl_free (error_msg);
		return;
	}
	
	/* get channel to remove */
	channel = vortex_connection_get_channel (connection, channel_num);

	/* check if the channel is in process of being closed */
	vortex_mutex_lock (&channel->close_mutex);
	if (channel->being_closed) {
		/* seems the channel is being closed already  */
		vortex_mutex_unlock (&channel->close_mutex);
		
		/* reply that we accept to close the channel */
		vortex_log (VORTEX_LEVEL_WARNING, 
			    "received a close channel=%d (conn=%d) request while waiting for an outstading close channel request (cross in transit close), accepting..",
			    channel->channel_num,
			    vortex_connection_get_id (channel->connection));

		vortex_channel_send_rpy (channel0, 
					 vortex_frame_get_ok_message (), 
					 strlen (vortex_frame_get_ok_message ()),
					 vortex_frame_get_msgno (frame));
		
		/* get the reference for the wait reply */
		wait_reply = vortex_channel_get_data (channel, VORTEX_CHANNEL_WAIT_REPLY);
		if (wait_reply != NULL) {
			vortex_log (VORTEX_LEVEL_DEBUG, "creating a fake ok reply (translated the incomming close request into close accept)");
			ok_frame = vortex_frame_create (ctx, 
							VORTEX_FRAME_TYPE_RPY, 
							0, 
							wait_reply->msg_no_reply,
							false,
							0, 
							strlen (vortex_frame_get_ok_message ()), 
							0, 
							vortex_frame_get_ok_message ());

			vortex_log (VORTEX_LEVEL_DEBUG, "pushing fake frame");
			
			/* queue frame received */
			QUEUE_PUSH (wait_reply->queue, ok_frame);

			return;
		} /* end if */

		vortex_log (VORTEX_LEVEL_CRITICAL, "unable to find wait reply structure to queue a fake reply, channel=%d, conn=%d",
			    channel->channel_num, vortex_connection_get_id (channel->connection));
		
		return;
	}

	/* check if the channel has a close notify */
	if (channel->close_notify != NULL) {
		vortex_log (VORTEX_LEVEL_DEBUG, "found close notify handler, notifying this and disabling close handler");

		/* call to notify the close request on the channel */
		channel->close_notify (channel, vortex_frame_get_msgno (frame), channel->close_notify_user_data);

		/* close unlock the mutex */
		vortex_mutex_unlock (&channel->close_mutex);

		return;
		
	} /* end if */

	/* ask second level handler */
	if (vortex_channel_is_defined_close_handler (channel)) {
		/* flag the channel as notified to be closed so the
		 * close handler can call to vortex_channel_close */
		vortex_channel_set_data (channel, VORTEX_CHANNEL_CLOSE_HANDLER_CALLED, INT_TO_PTR(1));

		/* call to get the application level accept */
		close_value = vortex_channel_invoke_close_handler (channel);
		
		vortex_channel_set_data (channel, VORTEX_CHANNEL_CLOSE_HANDLER_CALLED, 0);
		vortex_log (VORTEX_LEVEL_DEBUG, "close second level handler response=%d", close_value);
		goto end;
	}

	/* ask first level handler */
	if (vortex_profiles_is_defined_close (ctx, channel->profile)) {
		close_value = vortex_profiles_invoke_close (channel->profile, 
							    channel->channel_num, 
							    channel->connection);
		vortex_log (VORTEX_LEVEL_DEBUG, "close first level handler response=%d", close_value);
		goto end;
	}

	/* if channel0 have no defined its close handler we assume
	 * close = true */
	if (channel->channel_num == 0) 
		close_value = true;

	vortex_log (VORTEX_LEVEL_WARNING, "no close handler defined for any level, assuming=%d", close_value);
 end:
	/* now we have executed close handler for all levels and we
	 * know what to do we need to follow the next indications:
	 * 
	 * 1) If close_value is false, simple reply an error message
	 * to close avoiding to get blocked until all replies have 
	 * been received.
	 * 
	 * 2) If close_value is true, we have to wait to all replies
	 * to be received and tag the channel as being closed because
	 * the application have accepted its closing. This is equal to
	 * call vortex_channel_close. The channel gets into a state
	 * where no new MSG can be sent and only replies will be
	 
	 * check 1)
	 * ~~~~~~~~
	 * flag the channel to be usable again */
	channel->being_closed = close_value;
	if (close_value == false) {
		/* unlock the mutex */
		vortex_mutex_unlock (&channel->close_mutex);

		error_msg = vortex_frame_get_error_message ("550", "still working", NULL);
		vortex_channel_send_err (channel0, error_msg, strlen (error_msg), vortex_frame_get_msgno (frame));
		axl_free (error_msg);
		return;
	}

	/* unlock and proceed to close the channel */
	vortex_mutex_unlock (&channel->close_mutex);

	/* call to notify channel to be closed */
	vortex_channel_notify_close (channel, vortex_frame_get_msgno (frame), true);

	return;
}

/** 
 * @brief Allows to notify the channel close after receiving the close
 * notification request. This function is used along with the handler
 * \ref VortexOnNotifyCloseChannel.
 *
 * The idea behind is that you configure the \ref
 * VortexOnNotifyCloseChannel handler to get notification about the
 * channel being requested to be closed, but no action is taken on
 * that handler. Just notify.
 *
 * Later, a call to this function must be performed to complete
 * (close) or deny the request received. 
 *
 * This function provides a way to decouple close notification from
 * its handling allowing to better integrate inside other environments
 * that process all information from inside an main loop.
 * 
 * @param channel The channel that is notified to take action about a
 * close message received.
 *
 * @param msg_no The msg number that was received identifiying the
 * close request received.
 *
 * @param close The value to notify as answer to the request to close
 * the channel.
 */
void vortex_channel_notify_close (VortexChannel * channel, int  msg_no, bool     close)
{
	VortexChannel    * channel0;
	VortexConnection * connection;
	char             * error_msg;
	bool               ok_on_wait      = false;
#if defined(ENABLE_VORTEX_LOG)
	VortexCtx        * ctx     = vortex_channel_get_ctx (channel);
#endif

	/* check channel reference */
	if (channel == NULL)
		return;
	
	/* get the connection */
	connection = vortex_channel_get_connection (channel);
	channel0   = vortex_connection_get_channel (connection, 0);

	/* check that the user doesn't want to close the channel, and
	 * reply as is */
	if (! close ) {

		/* create the error msg */
		error_msg = vortex_frame_get_error_message ("550", "still working", NULL);
		vortex_channel_send_err (channel0, error_msg, strlen (error_msg), msg_no);
		axl_free (error_msg);
		
		return;
	} /* end if */

	/* check 2)
	 * ~~~~~~~~
	 * Ok, close we have to make sure have already receive all
	 * replies for all message, that is, to be equal channel MSG
	 * sent to RPY or ERR received */
	vortex_log (VORTEX_LEVEL_DEBUG, "check replies to be received for an incoming close request=%d (peer issued)",
		    channel->channel_num);
	ok_on_wait = __vortex_channel_block_until_replies_are_received (channel);
	
	/* So, anyone have something to say about closing this
	 * channel?, no?, then close the channel.  We remove the
	 * channel before sending replies so we avoid channel closing
	 * collision. */
	if (channel->channel_num == 0) {

		/* flag the connection to be at close processing. We
		 * set this flag so vortex reader won't complain about
		 * not properly connection close. */
		vortex_connection_set_data (connection, "being_closed", INT_TO_PTR (true));

		if (ok_on_wait) {
			/* install a waiter into the channel 0 for the message
			 * to be sent */
			vortex_channel_install_waiter  (channel0, msg_no);
			
			/* send ok message */
			vortex_channel_send_rpyv       (channel0, msg_no, 
							vortex_frame_get_ok_message ());
			
			/* block until the message is sent */
			vortex_channel_wait_until_sent (channel0, msg_no);

			vortex_log (VORTEX_LEVEL_DEBUG, "message <ok />, sent, closing the connection");
		}else {
			/* log that the connection is broken */
			vortex_log (VORTEX_LEVEL_WARNING," not setn <ok /> message because the connection is broken");
		}
		
		/* flag the connection to be closed and non usable */ 
		__vortex_connection_set_not_connected (connection, "channel closed properly");

		vortex_log (VORTEX_LEVEL_DEBUG, "connection closed..");
		
	}else {
		vortex_log (VORTEX_LEVEL_DEBUG, "channel %d with profile %s successfully closed", 
		       channel->channel_num, channel->profile);

		/* send the positive reply (ok message) */
		if (ok_on_wait) {
			vortex_log (VORTEX_LEVEL_DEBUG, "sending reply to close channel=%d", channel->channel_num);
			vortex_channel_send_rpyv (channel0, msg_no,
						  vortex_frame_get_ok_message ());
			vortex_log (VORTEX_LEVEL_DEBUG, "reply sent to close=%d", channel->channel_num);
		} else {
			/* log that the connection is broken */
			vortex_log (VORTEX_LEVEL_WARNING," not setn <ok /> message because the connection is broken");
		}

		/* before closing the channel ensure there are not
		 * message being_sending */
		if (channel->being_sending) {
			vortex_mutex_lock   (&channel->send_mutex);
			vortex_mutex_unlock (&channel->send_mutex);
		}

		/* remove channel from connection */
		vortex_connection_remove_channel (connection, channel);
	}

	return;
}

/** 
 * @brief Allows to get best message size for a message transfer. 
 *
 * The information returned by this function could be used to know the
 * amount of information that should be included into the next message
 * to be used (\ref vortex_channel_send_msg, \ref
 * vortex_channel_send_rpy, etc).
 *
 * This function provides the effective message size to be used for a
 * bulk transfer, avoiding BTF issue (see \ref
 * http://www.aspl.es/vortex/btf.html).
 *
 * In general, if your intention is to produce a large set of messages
 * for a long transfer (because you are sending a file) you should
 * call this function to messages that do not produce bad interations
 * inside the Vortex engine.
 * 
 * @param channel The channel where the transfer will be performed.
 *
 * @param mime_message Allows to inform the function that the content
 * to be sent is a MIME message. 
 * 
 * @return The amount of bytes that should be included on each message
 * to be transfered. NOTE: this function is just an indication. Your
 * message will arrive, no matter its size (with common sense
 * expections) and the size you use to transfer its fragments. Using
 * the information from this function will make your transfer to get
 * best throughput. If a NULL reference is received, 0 is returned.
 */
int                vortex_channel_get_effective_transfer_size    (VortexChannel    * channel,
								  bool               mime_message)
{
	v_return_val_if_fail (channel, 0);

	/* in the case a MIME message is found, let the caller to use
	 * all available window */
	if (mime_message)
		return 4096;
	/* if not, use only availabe window without two bytes (CR+LF)
	 * required to implement the empty MIME header */
	return 4094;
}

/** 
 * @internal
 * 
 * @param channel0 
 * @param frame 
 */
void __vortex_channel_0_frame_received_ok_msg (VortexChannel * channel0, VortexFrame * frame)
{
	
	return;
}

/** 
 * @internal
 * @brief Frame received for channel 0 inside Vortex Library.
 *
 * Internal vortex library function. This function handle all incoming
 * frames that are received on channel 0 for a given connection.
 * 
 * Channel 0 is a really important channel for each session on BEEP
 * standard. Over this channel is negotiated new channel creation and
 * profiles used for these new channels.
 *
 * If you are looking for the handler which process: start and close
 * message defined on RFC 3080 this is the place.
 *
 * @param channel0    The channel 0 where a message was received.
 * @param connection  The connection where the channel 0 belongs to.
 * @param frame       The frame actually received
 * @param user_data   User defined data
 */
void vortex_channel_0_frame_received (VortexChannel    * channel0,
				      VortexConnection * connection,
				      VortexFrame      * frame,
				      axlPointer         user_data)
{
#if defined(ENABLE_VORTEX_LOG)
	VortexCtx   * ctx     = vortex_channel_get_ctx (channel0);
#endif

	/* check we are handling the channel 0 */
	if (vortex_channel_get_number (channel0) != 0) {
		vortex_log (VORTEX_LEVEL_DEBUG, "invoked channel 0 frame received over a different channel (%d)\n",
		       vortex_channel_get_number (channel0));
		__vortex_connection_set_not_connected (connection, 
						       "invoked channel 0 frame received over a different channel (%d)\n");
		return;
	}

	vortex_log (VORTEX_LEVEL_DEBUG, "frame received over channel 0 (size: %d):\n%s\n",
	       vortex_frame_get_content_size (frame),
	       vortex_frame_get_payload (frame));

	/* validate message */
	/* if (!__vortex_channel_0_frame_received_validate (channel0,
	   frame)) return; */
	
	/* dispatch the frame received over the channel 0 to the
	 * appropriate place */
	switch (__vortex_channel_0_frame_received_identify_type (channel0, frame)) {
	case START_MSG:
		/* received a start message */
		__vortex_channel_0_frame_received_start_msg (channel0, frame);
		break;
	case CLOSE_MSG:
		/* received a close message */
		__vortex_channel_0_frame_received_close_msg (channel0, frame);
		break;
	case OK_MSG:
		vortex_log (VORTEX_LEVEL_DEBUG, "received <ok /> reply");
		/* do nothing, Vortex Library uses wait reply method
		 * to get remote ok message.  */
		break;
	case ERROR_MSG:
		/* should not happen */
		break;
	}
	
	return;
}



/** 
 * @brief Return channel opened state. 
 * 
 * This is actually used by vortex_reader thread to detect if an
 * incoming message received can be delivered. Vortex reader use this
 * function to detect if a channel is being closed but waiting for
 * some received msg to be replied after sending close message.
 *
 * Before issuing a close message to close a channel
 * (\ref vortex_channel_close) the channel must fulfill some steps. So,
 * until reply to a close message is receive a channel is considered
 * to be valid to keep on sending replies to received messages.  
 * 
 * Note that this situation can take some time because remote peer, at
 * the moment of receiving a close message, could have tons of message
 * waiting to be sent.
 * 
 * Vortex library consumer can use this function to avoid sending
 * message over a channel that can be on an ongoing closing process.
 * 
 * @param channel the channel to operate on.
 * 
 * @return true if a channel is opened or false if not.
 */
bool            vortex_channel_is_opened       (VortexChannel * channel)
{
	/* a null reference is considered as a closed channel */
	if (channel == NULL)
		return false;

	/* return current channel status */
	return channel->is_opened && vortex_connection_is_ok (channel->connection, false);
}

/** 
 * @brief Return if a channel is on the process of being closed. 
 * 
 * If a channel is being closed means no more message MSG can be sent
 * over it. But, actually more message can still be received (and
 * these message must be replied). On this context a channel being
 * closed can be used to send rpy and err message.
 *
 * This function can be used by vortex library consumer to know if a
 * channel can be used to send more replies to received messages.
 * 
 * @param channel the channel to operate on
 * 
 * @return if channel is being closed. Keep in mind the function will
 * return false if a NULL reference is received. 
 */
bool            vortex_channel_is_being_closed (VortexChannel * channel)
{
	if (channel == NULL)
		return false;

	return channel->being_closed;
}

/** 
 * @brief free channel allocated resources.
 *
 * Free channel resources. Vortex Library consumers must not call this
 * function directly.
 *
 * In order to close and free the channel properly
 * \ref vortex_channel_close must be used.
 * 
 * @param channel the channel to free.
 */
void vortex_channel_free (VortexChannel * channel)
{
	VortexWriterData * writer_data;
	WaitReplyData    * wait_reply;
#if defined(ENABLE_VORTEX_LOG)
	VortexCtx        * ctx     = vortex_channel_get_ctx (channel);
#endif

	/* get channel profile */
	if (channel == NULL)
		return;
	
	vortex_log (VORTEX_LEVEL_DEBUG, "freeing resources for channel %d", channel->channel_num);


	/* check if this channel belongs to a pool to remove it */
	if (channel->pool) {
		/* it seems this channel belongs to a pool remove it
		 * from the pool */
		vortex_channel_pool_deattach (channel->pool, channel);
	}

	/* ensure no wait reply is running remaining to unlock the
	 * channel receive_mutex. Lock on it and unlock it
	 * immediately. */
	vortex_mutex_lock    (&channel->receive_mutex);
	vortex_mutex_unlock  (&channel->receive_mutex);
	vortex_log (VORTEX_LEVEL_DEBUG, "freeing receive_mutex");
	vortex_mutex_destroy (&channel->receive_mutex);

	vortex_mutex_lock    (&channel->send_mutex);
	vortex_mutex_unlock  (&channel->send_mutex);
	vortex_log (VORTEX_LEVEL_DEBUG, "freeing send_mutex channel=%d", channel->channel_num);
	vortex_mutex_destroy (&channel->send_mutex);

	vortex_log (VORTEX_LEVEL_DEBUG, "freeing close_mutex");
	vortex_mutex_destroy (&channel->close_mutex);

	vortex_log (VORTEX_LEVEL_DEBUG, "freeing pending_mutex");
	vortex_mutex_destroy (&channel->pending_mutex);

	vortex_log (VORTEX_LEVEL_DEBUG, "freeing serialize_mutex");
	vortex_mutex_destroy (&channel->serialize_msg_mutex);
	vortex_mutex_destroy (&channel->serialize_rpy_mutex);
	vortex_mutex_destroy (&channel->serialize_ans_mutex);
	axl_hash_free (channel->serialize_rpy);
	axl_hash_free (channel->serialize_msg);
	axl_hash_free (channel->serialize_ans);
	axl_hash_free (channel->stored_replies);

	vortex_log (VORTEX_LEVEL_DEBUG, "freeing ref_mutex");
	vortex_mutex_destroy (&channel->ref_mutex);

	vortex_log (VORTEX_LEVEL_DEBUG, "freeing close_cond");
	vortex_cond_destroy  (&channel->close_cond);

	vortex_log (VORTEX_LEVEL_DEBUG, "freeing pending_cond");
	vortex_cond_destroy  (&channel->pending_cond);

	axl_free              (channel->profile);
	channel->profile = NULL;

	/* destroy pending piggyback */
	if (vortex_channel_have_piggyback (channel))
		vortex_frame_unref (vortex_channel_get_piggyback (channel));
	
	/* destroy data hash */
	vortex_hash_destroy (channel->data);
	channel->data = NULL;

	/* freeing queue messages */
	vortex_log (VORTEX_LEVEL_DEBUG, "freeing messages queue");
	while (!vortex_queue_is_empty (channel->queue_msgs)) {
		writer_data = vortex_queue_pop (channel->queue_msgs);
		vortex_writer_data_free (writer_data);
	}
	vortex_queue_free   (channel->queue_msgs);
	channel->queue_msgs = NULL;

	/* freeing SEQ frames queue */
	vortex_log (VORTEX_LEVEL_DEBUG, "freeing SEQ frames queue");
	while (!vortex_queue_is_empty (channel->queue_seqs)) {
		writer_data = vortex_queue_pop (channel->queue_seqs);
		vortex_writer_data_free (writer_data);
	}
	vortex_queue_free   (channel->queue_seqs);
	channel->queue_seqs = NULL;

	/* freeing wait replies */
	vortex_log (VORTEX_LEVEL_DEBUG, "freeing waiting reply queue");
	while (!vortex_queue_is_empty (channel->waiting_msgno)) {
		wait_reply = vortex_queue_pop (channel->waiting_msgno);
		vortex_channel_free_wait_reply (wait_reply);
	}
	vortex_queue_free   (channel->waiting_msgno);
	channel->waiting_msgno = NULL;

	/* free pending messages to be completed */
	vortex_log (VORTEX_LEVEL_DEBUG, "freeing previous frames");

	/* dealloc previous frame */
	if (channel->previous_frame)
		axl_list_free (channel->previous_frame);
	channel->previous_frame = NULL;

	/* free pending messages */
	axl_list_free (channel->pending_messages);

	vortex_log (VORTEX_LEVEL_DEBUG, "freeing channel node");

	/* free channel */
	axl_free (channel);

	return;
}


/** 
 * @brief Allows to increase wait reply ref count.
 * 
 * @param wait_reply The wait reply to increase its ref count
 */
void vortex_channel_wait_reply_ref (WaitReplyData * wait_reply)
{
	/* lock */
	vortex_mutex_lock (&wait_reply->mutex);

	/* oper */
	wait_reply->refcount++;

	/* unlock */
	vortex_mutex_unlock (&wait_reply->mutex);
	
	return;
}

/** 
 * @brief Terminates allocated memory by wait reply data.
 *
 * See \ref vortex_manual_wait_reply "this section" to know more about this function
 * and how it is used inside the Wait Reply process.
 * 
 * @param wait_reply Wait reply to free. The reference created by using \ref vortex_channel_create_wait_reply.
 */
void     vortex_channel_free_wait_reply (WaitReplyData * wait_reply)
{
	VortexFrame * frame;

	/* get channel profile */
	if (wait_reply == NULL)
		return;

	/* lock */
	vortex_mutex_lock (&wait_reply->mutex);

	/* decrease ref count */
	wait_reply->refcount--;
	if (wait_reply->refcount != 0) {
		/* unlock and return */
		vortex_mutex_unlock (&wait_reply->mutex);
		return;
	} /* end if */

	/* perform dealloc operations if refcount reaches 0 */

	/* free the queue */
	while (vortex_async_queue_items (wait_reply->queue) > 0) {
		/* get the frame and unref */
		frame = vortex_async_queue_pop (wait_reply->queue);
		vortex_frame_unref (frame);
	}
	vortex_async_queue_unref (wait_reply->queue);

	/* unlock and return */
	vortex_mutex_unlock (&wait_reply->mutex);
	vortex_mutex_destroy (&wait_reply->mutex);
	axl_free (wait_reply);

	return;
}

/** 
 * @brief Returns the actual state for a given channel about pending
 * replies to be received (not pending replies to be sent).
 * 
 * Due to RFC3080/RFC3081 design any message sent must be replied in
 * the same order the message was issued. This means that if you use
 * the same channel to send two messages the reply for the second one
 * will not be received until the previous one is. This is true even the
 * second message have a task-impact lower than previous on the remote
 * peer.
 *
 * Of course this is a bottleneck problem because you can increase
 * greatly the message/reply processing relation using channels that
 * are ready in the sense they are not waiting for message replies so
 * your message will not be blocked due to previous ones.
 *
 * This function returns if a channel is ready in the previous sense:
 * <i>"No previous message is waiting to be replied over this channel so
 * my message will only wait as long as the remote peer process my
 * message and reply it"</i>.
 *
 * In other words: <i>"The function returns true if the channel
 * received all replies for all message sent".</i>
 *
 * Keep in mind a channel is always ready to accept new message to be
 * sent messages. In fact, eventually any message sent over a channel
 * will have its reply but this "ready sense" is a matter of
 * performance not availability.
 *
 * <i><b>NOTE 1:</b>The function do not check if the channel have
 * pending replies to be sent. This is the opossite situation where
 * the remote peer sent messages that this channel didn't reply.
 *
 * This function only cares about channel's pending replies to be
 * received, not those to be sent.
 * </i>
 *
 * <i><b>NOTE 2:</b>As a general rule any MSG received, by both peers,
 * must be replied by a RPY or ERR or ANS/NUL series.</i>
 * 
 * @param channel the channel to check for its readyness state.
 * 
 * @return true if ready, otherwise false is returned.
 */
bool               vortex_channel_is_ready                       (VortexChannel * channel)
{
#if defined(ENABLE_VORTEX_LOG)
	VortexCtx * ctx     = vortex_channel_get_ctx (channel);
#endif

	if (channel == NULL)
		return false;

	vortex_log (VORTEX_LEVEL_DEBUG, "checking channel to be ready (%d == %d) && reply_processed=%d",
	       channel->last_message_sent, 
	       (channel->last_reply_expected - 1), 
	       channel->reply_processed);
	
	return ((channel->last_message_sent == (channel->last_reply_expected - 1)) && channel->reply_processed);
}

/** 
 * @brief Support function that could be used as a frame received
 * function which queue all frames received inside a \ref VortexAsyncQueue.
 *
 * See \ref vortex_channel_get_reply for more explanation about using
 * this function.
 * 
 *
 *
 * @param channel    The channel where the frame was received.
 * @param connection The connection where the channel lives.
 * @param frame      The frame received.
 * @param user_data  A queue where the frame received will be pushed.
 */
void               vortex_channel_queue_reply                    (VortexChannel    * channel,
								  VortexConnection * connection,
								  VortexFrame      * frame,
								  axlPointer         user_data)
{
	VortexAsyncQueue * queue      = user_data;
	VortexFrame      * frame_copy = NULL;
#if defined(ENABLE_VORTEX_LOG)
	VortexCtx        * ctx        = vortex_channel_get_ctx (channel);
#endif
	
	if (queue == NULL) {
		vortex_log (VORTEX_LEVEL_CRITICAL, 
		       "NULL reference where a queue was expected, unable to queue frame received");
		return;
	}
	vortex_log (VORTEX_LEVEL_DEBUG, "received frame, queueing frame over channel=%d at connection=%d",
	       channel->channel_num, vortex_connection_get_id (connection));

	/* because this frame handler is run under the semantic of the
	 * second level handler, once finished this function, the
	 * frame to push will be deallocate. We have to perform a
	 * copy. */
	frame_copy = vortex_frame_copy (frame);
	
	QUEUE_PUSH (queue, frame_copy);
	return;
}

/** 
 * @brief Allows to get the next frame received on the given channel
 * due to channel start reply piggybacking or due to a frame received
 * while using the function \ref vortex_channel_queue_reply as a \ref VortexOnFrameReceived "frame received handler".
 *
 * This function is used with \ref vortex_channel_queue_reply to
 * receive the message replies (in fact, all messages received). It
 * also support returning the data received due to channel start
 * piggybacking through \ref vortex_channel_get_piggyback.
 *
 * If you would like to know more about the piggyback concept, check
 * the following section to \ref vortex_manual_piggyback_support "know more about how piggyback could improve your protocol startup".
 *
 * The idea is to use \ref vortex_channel_queue_reply as a frame
 * receive handler which queues all frames received on a queue. Then,
 * every time \ref vortex_channel_get_reply is called, a frame from
 * the queue is returned.
 * 
 * This allows to support, using the same code, to receive a message
 * reply either as a piggyback or as a normal reply (and any other
 * frame received).
 *
 * Let's suppose you are implementing a profile which could send an
 * initial start message with the piggyback, so it is saved one round
 * trip: <i>"with one message, we send the start channel request, including
 * the first message exchanged."</i>.
 *  
 * At the remote peer, the channel creation request, with a piggyback
 * content, could be replied, using two, standard allowed, methods:
 *  
 * - A reply to the &lt;start> message accepting the channel to be
 * created and then reply to the implicit content received, in a
 * separated message.
 * 
 * - Or reply to the &lt;start> message accepting the channel to be
 * created, including inside that reply, the reply to the first
 * piggyback message received.
 *
 * This leads to a source code problem at the client side that perform
 * a blocking wait: <i>"It is needed a way to get, not only the
 * piggyback, but also the same reply not using the piggyback
 * method."</i>
 *
 * Because piggyback is closely related to the channel creation, and
 * to avoid writing two different piece of code to manage both cases,
 * you have two methods:
 * 
 * - 1. Set an \ref VortexOnChannelCreated "on created handler" at the
 * \ref vortex_channel_new or \ref vortex_channel_new_full and you'll
 * receive, as the first frame, the piggyback. For this case, the
 * first frame received over the channel will either the piggyback or,
 * if not defined, the first frame received.
 * 
 * - 2. If you need to make synchronous, the code you are doing, set
 * the \ref vortex_channel_queue_reply as the frame received handler
 * and then, once the channel is created call to \ref
 * vortex_channel_get_reply. This method works the same as <b>1.</b>
 * 
 * <i><b>NOTE: </b> The frame returned by this function have to be
 * deallocated using \ref vortex_frame_unref when no longer
 * needed. This is an special case because all frames delivered with
 * first and second invocation handlers are automatically managed,
 * that is, deallocated when the frame handler scope has gone.</i>
 *
 * Because examples are more clear, here is one:
 *
 * \code
 * // create a channel making all frames received, including the
 * // piggyback to be received in a blocking way:
 * VortexChannel      * channel; 
 * VortexAsyncQueue   * queue;
 * VortexFrame        * frame;
 *
 * // create the queue to be used to perform the async wait
 * queue   = vortex_async_queue_new ();
 *
 * // create the channel
 * channel = vortex_channel_new (// the connection where the channel
 *                               // will be created
 *                               connection, 
 *                               // let vortex library to manage
 *                               // the next channel number available
 *                               0, 
 *                               // use default channel close handling
 *                               NULL, NULL,
 *                               // set the frame receive handling
 *                               vortex_channel_queue_reply, queue,
 *                               // no channel creation notification
 *                               NULL, NULL);
 * 
 * // now wait and process all replies and messages received
 * while (true) {
 *     // get the next message, blocking at this call.
 *     frame = vortex_channel_get_reply (channel, queue);
 *
 *     if (frame == NULL) {
 *          // timeout found, do some error reporting
 *          // and default action on timeout received.
 *          //
 *          // for our example, the default action is:
 *          //     keep on reading!.
 *          continue;
 *     }
 *     printf ("Frame received, content: %s\n",
 *             vortex_frame_get_payload (frame));
 *    
 *     // deallocate the frame received
 *     vortex_frame_unref (frame);
 * }
 * \endcode
 *
 * @param channel The channel where is expected to receive a frame
 * using the frame received handler or the initial piggyback.  @param
 * queue The queue where the frame is expected to be received. The
 * queue received, no matter the result will be deallocated.
 * 
 * @return The frame received or NULL if the timeout is expired. 
 */
VortexFrame      * vortex_channel_get_reply                      (VortexChannel    * channel, 
								  VortexAsyncQueue * queue)
{
	VortexFrame * frame;
	VortexCtx   * ctx     = vortex_channel_get_ctx (channel);

	/* check for piggyback reply */
	frame = vortex_channel_get_piggyback (channel);
	if (frame) {
		vortex_log (VORTEX_LEVEL_DEBUG, "getting piggyback from remote peer at get_reply");
		return frame;
	}

	vortex_log (VORTEX_LEVEL_DEBUG, "getting next frame from remote peer at get_reply");

	/* get reply from queue */
	frame = vortex_async_queue_timedpop (queue, vortex_connection_get_timeout (ctx));
	if (frame == NULL) {
		/* drop a log in case of a timeout is reached. */
		vortex_log (VORTEX_LEVEL_CRITICAL, 
		       "timeout was expired while waiting at get-reply");
	}
	
	/* return whatever is returned */
	return frame;
}

/** 
 * @brief Allows to get initial piggyback received on the channel
 * start reply.
 *
 * Once the functions returns the piggyback, next calls will return
 * NULL.
 *
 * @param channel The channel where the piggyback was received.
 * 
 * @return Piggyback received or NULL if not defined.
 */
VortexFrame      * vortex_channel_get_piggyback                  (VortexChannel    * channel)
{
	
	VortexFrame * frame;

	/* get channel profile */
	if (channel == NULL)
		return NULL;

	/* lookup and clear */
	frame = vortex_hash_lookup_and_clear (channel->data, "piggyback");

	return frame;
}

/** 
 * @brief Allows to check if the given channel have piggyback waiting to be processed.
 * 
 * If the function returns true then \ref vortex_channel_get_piggyback
 * should be used to get current piggyback received.
 * 
 * @param channel The channel to query.
 * 
 * @return true if the channel has piggyback, otherwise false is returned.
 */
bool               vortex_channel_have_piggyback                 (VortexChannel   * channel)
{
	/* get channel profile */
	if (channel == NULL)
		return false;

	return (vortex_channel_get_data (channel, "piggyback") != NULL);
}

/** 
 * @internal
 * @brief Allows to set current piggyback for the given channel. 
 *
 * This function should not be used by Vortex Library API
 * consumers. It is used by Vortex Library to support general
 * piggybacking.
 * 
 * @param channel The channel where the piggyback will be set.
 * @param frame   The frame to be set as piggyback.
 */
void               vortex_channel_set_piggyback                  (VortexChannel    * channel,
								  const char       * profile_content)
{
	VortexFrame * piggyback_frame;
	VortexCtx   * ctx     = vortex_channel_get_ctx (channel);

	/* get channel profile */
	if (channel == NULL || profile_content == NULL)
		return;

	/* build the frame to convert the piggyback received, that is
	 * the profile element content, into a frame as it were the
	 * first frame received on this channel. */
	piggyback_frame = vortex_frame_create (ctx,
					       VORTEX_FRAME_TYPE_RPY,
					       0,     /* channel number */
					       0,     /* the msgno */
					       false, /* more flag */
					       0,     /* seqno number */
					       strlen (profile_content),
					       0,
					       profile_content);

	/* store the frame */
	vortex_channel_set_data (channel, "piggyback", piggyback_frame);
	
	return;
}

/** 
 * @internal
 * 
 * This function is for internal Vortex library purposes. Actually
 * this function is used by the vortex_channel_pool to check if a
 * message is ready from the point of view that all replies to message
 * sent have been received discarding if that replies have been
 * actually processed.
 *
 * This function should not be useful for vortex library consumer but
 * can be used to check if a channel have received (but may not
 * processed) all replies.
 * 
 * @param channel the channel to operate on
 * 
 * @return 
 */
bool               vortex_channel_is_up_to_date (VortexChannel * channel)
{
	/* get channel profile */
	if (channel == NULL)
		return false;
	
	return (channel->last_message_sent == (channel->last_reply_expected - 1));
}

/** 
 * @brief Creates a new wait reply to be used to wait for a specific
 * reply.
 *
 * @return a new Wait Reply object. 
 */
WaitReplyData * vortex_channel_create_wait_reply ()
{
	WaitReplyData * data;	
	data           = axl_new (WaitReplyData, 1);
	data->queue    = vortex_async_queue_new ();
	data->refcount = 1;
	vortex_mutex_create (&data->mutex);

	return data;
}

/** 
 * @brief Allows caller to wait for a particular reply to be received. 
 * 
 * Check \ref vortex_manual_wait_reply "this section" to know more about how using
 * this function inside the Wait Reply Method.
 *
 * Keep in mind that this function could return a NULL frame while
 * performing a wait. This timeout is configured through the following
 * functions:
 *
 *   - \ref vortex_connection_timeout
 *   - \ref vortex_connection_get_timeout
 *
 * 
 * @param channel The channel where \ref vortex_manual_wait_reply "Wait Reply" is done. This value is
 * not optional and should be a valid channel. This function will
 * check if the reference provided is not NULL.
 *
 * @param msg_no The message number we are waiting to be replied. This
 * value is not optional and should be greater than 0. (Deprecated
 * value, no longer used, keept for historic and backward compact).
 *
 * @param wait_reply The wait reply object used. A valid wait reply
 * object created by \ref vortex_channel_create_wait_reply. This
 * function will check if the provided value is not NULL. 
 * 
 * @return The frame reply or NULL if a timeout occurs, having all
 * parameters well specified, or there was a parameter error detected. 
 * 
 * <i><b>NOTE:</b> In the case the function returns a valid frame, your
 * application must terminate it by using \ref vortex_frame_unref.  The reference to
 * the wait_reply object is also terminated (with \ref
 * vortex_channel_free_wait_reply) in the case a valid frame is
 * returned.</i>
 *
 * <i><b>NOTE 2:</b> You cannot use the same \ref WaitReplyData for
 * several wait operations. You must create a new one \ref
 * WaitReplyData object for each call done to this function (\ref vortex_channel_wait_reply).</i>
 */
VortexFrame   * vortex_channel_wait_reply              (VortexChannel * channel, int msg_no, 
							WaitReplyData * wait_reply)
{

	VortexFrame * frame;
	VortexCtx   * ctx     = vortex_channel_get_ctx (channel);

	/* get channel profile */
	if (channel == NULL || wait_reply == NULL)
		return NULL;
	
	/* wait for the message to be replied */
	vortex_log (VORTEX_LEVEL_DEBUG, "getting reply at wait reply from the queue");
	frame = vortex_async_queue_timedpop (wait_reply->queue, vortex_connection_get_timeout (ctx));


	/* because we have accept to deliver the frame to a waiting
	 * thread, we understand this frame have being delivered */
	channel->reply_processed = true;

	if (frame == NULL) {
		vortex_log (
			    VORTEX_LEVEL_CRITICAL, 
			    "received a timeout while waiting performing a 'wait reply'");
	}else {
		vortex_log (VORTEX_LEVEL_DEBUG, "received reply, freeing wait reply object");

		/* free data no longer needed */
		vortex_channel_free_wait_reply (wait_reply);
	}

	/* return whatever we have get */
	return frame;
}

/** 
 * @internal
 *
 * This function actually is used by the vortex reader process. It
 * allows vortex reader process to create a critical section while
 * update channel status.
 *
 * <b>NOTE: </b> this function is deprecated and should not be used anymore.
 * 
 * @param channel the channel to operate.
 */
void            vortex_channel_lock_to_update_received   (VortexChannel * channel)
{
	return;
}

/** 
 * @internal
 * 
 * This function actually is used by the vortex reader process. It
 * allows vortex reader process to create a critical section while
 * update channel status. 
 *
 * <b>NOTE: </b> this function is deprecated and should not be used
 * anymore.
 * 
 * @param channel the channel to operate on.
 */
void            vortex_channel_unlock_to_update_received (VortexChannel * channel)
{
	return;
}

/** 
 * @internal
 *
 * @brief Allows external vortex subsystems to lock the receive mutex.
 * 
 * @param channel The channel to operate.
 */
void               vortex_channel_lock_to_receive                (VortexChannel * channel)
{
	/* get channel profile */
	if (channel == NULL)
		return;

	vortex_mutex_lock (&channel->receive_mutex);

	return;
}

/** 
 * @internal
 *
 * @brief Allows external vortex subsystems to unlock the receive
 * mutex.
 * 
 * @param channel The channel to operate.
 */
void               vortex_channel_unlock_to_receive              (VortexChannel * channel)
{
	if (channel == NULL)
		return;
	
	vortex_mutex_unlock (&channel->receive_mutex);

	return;
}

/** 
 * @internal
 *
 * The function __vortex_channel_0_frame_received_close_msg can be
 * blocked awaiting to receive all replies expected. The following
 * signal tries to wake up a possible thread blocked until
 * last_reply_expected change.
 * 
 * @param channel 
 */
void           	   vortex_channel_signal_on_close_blocked        (VortexChannel    * channel)
{

#if defined(ENABLE_VORTEX_LOG)
	VortexCtx   * ctx     = vortex_channel_get_ctx (channel);
#endif

	/* get channel profile */
	if (channel == NULL)
		return;

	vortex_mutex_lock   (&channel->close_mutex);

	/* flag channel to be up to date on reply process */
	channel->reply_processed = true;

	/* check if channel is being closed */
	if (!channel->being_closed) {
		vortex_mutex_unlock (&channel->close_mutex);
		return;
	}
	
	/* do not signal if not close waiting */
	if (!channel->waiting_replies) {
		vortex_mutex_unlock (&channel->close_mutex);
		return;
	}

	/* do a close waiting signal */
	vortex_log (VORTEX_LEVEL_DEBUG, "signaling blocked thread on close msg process");

	vortex_cond_signal  (&channel->close_cond);
	vortex_mutex_unlock (&channel->close_mutex);

	return;
}

/** 
 * @internal
 *
 * Function used to unblock threads on close process that are waiting
 * all replies to message received are sent. Until done this, the
 * channel can not wait for a ok message.
 * 
 * @param channel 
 */
void   vortex_channel_signal_reply_sent_on_close_blocked (VortexChannel * channel)
{
	/* get channel profile */
	if (channel == NULL)
		return;

	/* check if channel is being closed */
	if (!channel->being_closed) {
		return;
	}

	vortex_mutex_lock   (&channel->pending_mutex); 
	vortex_cond_signal  (&channel->pending_cond);
	vortex_mutex_unlock (&channel->pending_mutex); 
	return;
}

/** 
 * @brief Allows to flag that the channel have processed its last
 * reply.
 *
 * Under normal situations, this function is used by the vortex reader
 * to notify the reply processed status for a provided channel. This
 * allows to control race conditions and to keep \ref
 * vortex_channel_is_ready working.
 *
 * The reply processed flag is only activated once the frame was
 * delivered and the frame received handler for the provided channel
 * has finished. This ensures that calls to \ref
 * vortex_channel_is_ready returns proper status for the provided
 * channel and all internal functions for a channel (especially the
 * close operation) keeps working and synchronized.
 *
 * However, it could happen, especially for request-response pattern
 * implemented on top of Vortex Library, like XML-RPC, that a race
 * condition could happen while calling to invoke several services
 * using the same channel. Because the thread that executes the
 * invocation code could be faster than the internal thread, inside
 * the vortex library, that flags the channel to have its reply
 * processed, makes the next invocation to fail because it is detected
 * that other ongoing operation is being taking place.
 *
 * To avoid such situations RPC implementations on top of Vortex
 * Library could use this function to force that a channel have
 * processed its reply. An example could be found inside the
 * implementation of \ref vortex_xml_rpc_invoke.
 *
 * @param channel The channel what is about to be flagged as ready (or
 * not, according to the flag value).
 *
 * @param flag The status to flag.
 */
void vortex_channel_flag_reply_processed (VortexChannel * channel, bool  flag)
{
#if defined(ENABLE_VORTEX_LOG)
	VortexCtx    * ctx     = vortex_channel_get_ctx (channel);
#endif

	/* get channel profile */
	if (channel == NULL)
		return;

	vortex_mutex_lock   (&channel->close_mutex);
	vortex_log (VORTEX_LEVEL_DEBUG, "setting pending reply processed=%d for channel=%d",
	       flag, channel->channel_num);
	channel->reply_processed = flag;
	vortex_mutex_unlock (&channel->close_mutex);

	return;
}


/** 
 * @internal
 *
 * This function allows to close channel processing while this message
 * refers to channel0 to properly synchronize the ok message response
 * and connection closing. 
 *
 * Used in conjunction with \ref vortex_channel_wait_until_sent enable to
 * get blocked until a reply have been sent.
 *
 * This function is for vortex internal purposes so it should not be
 * useful for vortex consumers.
 * 
 * @param channel 
 * @param rpy 
 */
void               vortex_channel_install_waiter                 (VortexChannel * channel,
								  int             msg_no)
{
	char * rpy;
	
	VortexAsyncQueue    * queue;

	/* get channel profile */
	if (channel == NULL)
		return;

	/* create the wait rpy */
	rpy   = axl_strdup_printf ("vortex:wait:rpy:%d", msg_no);

	/* create the queue */
	queue = vortex_async_queue_new ();

	/* install on the data associated to the channel */
	vortex_channel_set_data_full (channel, rpy, queue, axl_free, NULL);

	return;
}

/** 
 * @internal
 *
 * Block the caller until the message have been sent. This function is
 * for internal vortex purposes so it should not be useful to vortex
 * consumers.
 * 
 * @param channel 
 * @param rpy 
 */
void               vortex_channel_wait_until_sent                (VortexChannel * channel,
								  int             msg_no)
{
	VortexAsyncQueue * queue;
	char             * rpy;

	/* get channel profile */
	if (channel == NULL)
		return;

	/* get the queue */
	rpy   = axl_strdup_printf ("vortex:wait:rpy:%d", msg_no);
	queue = vortex_channel_get_data (channel, rpy);
	axl_free (rpy);

	/* check queue reference received */
	if (queue == NULL)
		return;

	/* wait until message is sent */
	vortex_async_queue_pop   (queue);
	vortex_async_queue_unref (queue);
	
	return;
}

/** 
 * @internal
 * 
 * @param channel 
 * @param rpy 
 */
void             vortex_channel_signal_rpy_sent                 (VortexChannel * channel,
								 int             msg_no)
{
	char             * rpy;
	VortexAsyncQueue * queue;
#if defined(ENABLE_VORTEX_LOG)
	VortexCtx        * ctx     = vortex_channel_get_ctx (channel);
#endif

	/* get channel profile */
	if (channel == NULL)
		return;

	/* get the queue */
	rpy   = axl_strdup_printf ("vortex:wait:rpy:%d", msg_no);
	queue = vortex_channel_get_data (channel, rpy);
	axl_free (rpy);

	/* check the queue */
	if (queue == NULL)
		return;

	/* push some data, to wakeup up the waiting thread */
	QUEUE_PUSH (queue, INT_TO_PTR (true));

	return;
}

/** 
 * @internal
 *
 * This function is for internal vortex purposes. This function
 * actually allows vortex channel pool module to notify this channel
 * that belongs to a pool so in case of a channel close (received or
 * invoked) this channel will be able to notify channel pool to remove
 * the reference to the channel.
 * 
 * @param channel the channel to notify it belongs to a pool.
 * @param pool the pool the channel belongs to.
 */
void               vortex_channel_set_pool                       (VortexChannel * channel,
								  VortexChannelPool * pool)
{
	/* get channel profile */
	if (channel == NULL)
		return;

	channel->pool = pool;

	return;
}


/** 
 * @brief Allows to get channel pool associated to the channel provided. 
 * 
 * @param channel The channel that is required to return its
 * associated channel pool (if is defined).
 * 
 * @return Reference to the channel pool or NULL if it fails or is not
 * defined.
 */
VortexChannelPool * vortex_channel_get_pool                       (VortexChannel * channel)
{
	/* get channel profile */
	if (channel == NULL)
		return NULL;
	
	/* return the pool associated */
	return channel->pool;
}

/** 
 * @brief Allows to get the context under which the channel was
 * created (\ref VortexCtx).
 * 
 * @param channel The channel that is required to return the context
 * associated.
 * 
 * @return A reference to the context or NULL if it fails. The
 * function only returns NULL if the reference received is null or the
 * connection holding the channel is not defined.
 */
VortexCtx         * vortex_channel_get_ctx                        (VortexChannel * channel)
{
	VortexCtx * ctx = NULL;

	if (channel == NULL) {
		vortex_log (VORTEX_LEVEL_DEBUG, "returning null context because null channel received");
		return NULL;
	}
	
	/* call to get the context associated to the cnonection */
	ctx = channel->ctx;

	return ctx;
} 

/** 
 * @internal Allows to start the vortex channel module state at the
 * provided context.
 * 
 * @param ctx The context to initialize (only the vortex channel
 * module).
 */
void                vortex_channel_init                           (VortexCtx * ctx)
{
	v_return_if_fail (ctx);

	vortex_mutex_create (&ctx->channel_start_reply_cache_mutex);
	ctx->channel_start_reply_cache = axl_hash_new (axl_hash_string, axl_hash_equal_string);
	
	return;
}

/** 
 * @brief Terminates the vortex channel module state (cleanup) on the
 * provided vortex context (\ref VortexCtx).
 * 
 * @param ctx The vortex context to cleanup.
 */
void                vortex_channel_cleanup                        (VortexCtx * ctx)
{
	v_return_if_fail (ctx);

	vortex_mutex_destroy (&ctx->channel_start_reply_cache_mutex);
	axl_hash_free (ctx->channel_start_reply_cache);
	ctx->channel_start_reply_cache = NULL;

	return;
}
								   

/* @} */
