/** 
 *  PyVortex: Vortex Library Python bindings
 *  Copyright (C) 2025 Advanced Software Production Line, S.L.
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
#ifndef __PY_VORTEX_CTX_H__
#define __PY_VORTEX_CTX_H__

/** 
 * @brief Object definition that represents the Python encapsulation
 * for VortexCtx API type.
 */
typedef struct _PyVortexCtx PyVortexCtx;

/* include base header */
#include <py_vortex.h>

VortexCtx * py_vortex_ctx_get    (PyObject * py_vortex_ctx);

#define START_HANDLER(handler) py_vortex_ctx_record_start_handler (ctx, handler)

#define CLOSE_HANDLER(handler) py_vortex_ctx_record_close_handler (ctx, handler)

typedef void (*PyVortexTooLongNotifier) (const char * msg, axlPointer user_data);

void        py_vortex_ctx_record_start_handler (VortexCtx * ctx, PyObject * handler);

void        py_vortex_ctx_record_close_handler (VortexCtx * ctx, PyObject * handler);

void        py_vortex_ctx_start_handler_watcher (VortexCtx * ctx, int watching_period,
						 PyVortexTooLongNotifier notifier, axlPointer notifier_data);

axl_bool    py_vortex_ctx_log_too_long_notifications (VortexCtx * ctx, int watching_period,
						      const char * file);

PyObject  * py_vortex_ctx_create (VortexCtx * ctx);

void        py_vortex_ctx_register (VortexCtx  * ctx,
				    PyObject   * data,
				    const char * key,
				    ...);

PyObject  * py_vortex_ctx_register_get (VortexCtx  * ctx,
					const char * key,
					...);

axl_bool    py_vortex_ctx_check  (PyObject  * obj);

void        init_vortex_ctx      (PyObject * module);

/** internal declaration **/
PyObject  * py_vortex_ctx_exit   (PyVortexCtx* self);

#endif
