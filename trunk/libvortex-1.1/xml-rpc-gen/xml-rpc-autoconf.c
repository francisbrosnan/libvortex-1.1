/* Hey emacs, show me this like 'c': -*- c -*-
 *
 * xml-rpc-gen: a protocol compiler for the XDL definition language
 * Copyright (C) 2013 Advanced Software Production Line, S.L.
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

#include <xml-rpc-autoconf.h>

/** 
 * @internal
 *
 * Creates the autogen.sh file with the provided component name.
 * 
 * @param comp_name The component name to make the autogen.sh file to
 * be particular.
 */
void xml_rpc_autoconf_autogen_sh_create (axlDoc   * doc, 
					 char     * out_dir, 
					 char     * comp_name,
					 axl_bool   is_server)
{


	/* open the autogen.sh file */
	xml_rpc_support_open_file ("%s/autogen.sh", out_dir);

	/* write initial lines that are particular for the component
	 * name */
	xml_rpc_support_write ("#!/bin/sh\n\n");
	xml_rpc_support_write ("PACKAGE=\"%s%s XML-RPC component\"\n", 
			       is_server ? "" : "C stub for ",
			       comp_name);

	/* and now the rest of the file */
	xml_rpc_support_write ("(automake --version) < /dev/null > /dev/null 2>&1 || {\n");

	/* push the indent file */
	xml_rpc_support_push_indent ();
	
	xml_rpc_support_multiple_write (
		"echo;\n",
		"echo \"You must have automake installed to compile $PACKAGE\";\n",
		"echo;\n",
		"exit;\n",
		NULL);
	
	/* pop the indent file */
	xml_rpc_support_pop_indent ();

	xml_rpc_support_multiple_write ("}\n\n",
					"(autoconf --version) < /dev/null > /dev/null 2>&1 || {\n",
					NULL);
	/* push the indent file */
	xml_rpc_support_push_indent ();

	xml_rpc_support_multiple_write (
		"echo;\n",
		"echo \"You must have autoconf installed to compile $PACKAGE\";\n",
		"echo;\n",
		"exit;\n",
		NULL);

	/* pop the indent file */
	xml_rpc_support_pop_indent ();

	/* write the rest of the file using the same indentation */
	xml_rpc_support_multiple_write (
		"}\n\n",

		"echo \"Generating configuration files for $PACKAGE, please wait....\"\n",
		"echo;\n\n",

		"touch NEWS README AUTHORS ChangeLog\n",
		"libtoolize --force --copy\n",
		"aclocal $ACLOCAL_FLAGS;\n",
		"autoheader;\n",
		"automake --add-missing --gnu;\n",
		"autoconf;\n\n",
		"./configure $@ --enable-maintainer-mode --enable-compile-warnings\n",
		NULL);
	
	/* close the autogen.sh file */
	xml_rpc_support_close_file ();

	/* make it to be executable */
	xml_rpc_support_make_executable ("%s/autogen.sh", out_dir);

	return;
}

/** 
 * @internal
 *
 * Creates the configure.ac file for the selcted component at the
 * output dir.
 * 
 * @param doc The axlDoc that represents the user interface.
 *
 * @param out_dir The output dir where will be placed the configure.ac
 * file.
 *
 * @param comp_name The component name.
 *
 * @param is_server Writes a configure.ac file for a server or for a
 * stub implementation.
 */
void xml_rpc_autoconf_configure_ac_create (axlDoc    * doc, 
					   char      * out_dir, 
					   char      * comp_name,
					   axl_bool    is_server)
{

	char  * comp_name_upper;
	char  * comp_name_lower;

	/* make a copy for the component name that is upper-cased */
	comp_name_upper = xml_rpc_support_to_upper (comp_name);
	comp_name_lower = xml_rpc_support_to_lower (comp_name);

	/* open the autogen.sh file */
	xml_rpc_support_open_file ("%s/configure.ac", out_dir);
	
	xml_rpc_support_write ("dnl generated by xml-rpc-gen autoconf assistant\n");
	
	if (is_server)
		xml_rpc_support_write ("AC_INIT(main.c)\n");
	else
		xml_rpc_support_write ("AC_INIT(%s_xml_rpc.h)\n", comp_name_lower);

	xml_rpc_support_write ("%s_XML_RPC_VERSION=1.0.0\n", comp_name_upper);
	xml_rpc_support_write ("AC_SUBST(%s_XML_RPC_VERSION)\n\n", comp_name_upper);

	xml_rpc_support_write ("AM_INIT_AUTOMAKE(%s%s-xml-rpc, $%s_XML_RPC_VERSION)\n\n",
			       /* write the server part, if defined */
			       is_server ? "server-" : "", 
			       comp_name, 
			       comp_name_upper);

	xml_rpc_support_write ("AM_CONFIG_HEADER(config.h)\n");
	xml_rpc_support_write ("AM_MAINTAINER_MODE\n\n");

	xml_rpc_support_multiple_write ("AC_PROG_CC\n",
					"AC_ISC_POSIX\n",
					"AC_HEADER_STDC\n",
					"AM_PROG_LIBTOOL\n",
					NULL);

	xml_rpc_support_write ("\nPKG_CHECK_MODULES(LIBRARIES, vortex axl)\n");

	xml_rpc_support_multiple_write ("AC_SUBST(LIBRARIES_CFLAGS)\n",
					"AC_SUBST(LIBRARIES_LIBS)\n\n",
					"AC_OUTPUT([\n"
					"Makefile\n",
					NULL);

	if (! is_server)
		xml_rpc_support_write ("%s_xml_rpc.pc", comp_name_lower);

	xml_rpc_support_write ("])\n");

	/* close the file */
	xml_rpc_support_close_file ();

	/* release reference used */
	axl_free (comp_name_upper);
	axl_free (comp_name_lower);

	return;
}


void xml_rpc_autoconf_write_struct_and_arrays (axlDoc * doc, char  * comp_name)
{
	/* struct support */
	axlNode * _struct;
	axlNode * name;
	char    * struct_name;
	char    * struct_lower;
	char    * comp_name_lower;

	/* get component name */
	comp_name_lower = xml_rpc_support_to_lower (comp_name);

	/* write here all struct implementations */
	_struct = axl_doc_get (doc, "/xml-rpc-interface/struct");
	if (_struct) {
		do {
			/* write struct code */
			if (NODE_CMP_NAME (_struct, "struct")) {
				name         = axl_node_get_child_nth (_struct, 0);
				struct_name  = axl_node_get_content_trim (name, NULL);
				
				/* make a lower copy */
				struct_lower = xml_rpc_support_to_lower (struct_name);
				
				xml_rpc_support_write ("%s_struct_%s_xml_rpc.h %s_struct_%s_xml_rpc.c ",
						       comp_name_lower, struct_lower, comp_name_lower, struct_lower);
				
				/* unref the lower copy */
				axl_free (struct_lower);
				
			}
		}while ((_struct = axl_node_get_next (_struct)) != NULL);
	} /* end if */
	
	/* write here all array implementations */
	_struct = axl_doc_get (doc, "/xml-rpc-interface/array");
	if (_struct) {
		do {
			/* write struct code */
			if (NODE_CMP_NAME (_struct, "array")) {
				name         = axl_node_get_child_nth (_struct, 0);
				struct_name  = axl_node_get_content_trim (name, NULL);
				
				/* make a lower copy */
				struct_lower = xml_rpc_support_to_lower (struct_name);
				
				xml_rpc_support_write ("%s_array_%s_xml_rpc.h %s_array_%s_xml_rpc.c ",
						       comp_name_lower, struct_lower, comp_name_lower, struct_lower);
				
				/* unref the lower copy */
				axl_free (struct_lower);
				
			}
		}while ((_struct = axl_node_get_next (_struct)) != NULL);
	} /* end if */
	
	/* free resources */
	axl_free (comp_name_lower);
}
/** 
 * @internal
 * 
 * Writes the Makefile.am file for the selected component name at the
 * selected destination.
 * 
 * @param doc The xml documentat that represent the interface.
 *
 * @param new_out_dir The directory location for the file to be created.
 *
 * @param comp_name The component name.
 */
void xml_rpc_autoconf_makefile_am_create (axlDoc    * doc, 
					  char      * new_out_dir, 
					  char      * comp_name,
					  axl_bool    is_server)
{
	/* xml document */
	axlNode * service;
	axlNode * params;
	axlNode * name;

	/* string variables */
	char    * service_name;
	char    * type_prefix;
	char    * comp_name_lower;
	
	/* open the makefile.am file */
	xml_rpc_support_open_file ("%s/Makefile.am", new_out_dir);

	/* build lower component name */
	comp_name_lower = xml_rpc_support_to_lower (comp_name);

	xml_rpc_support_write ("INCLUDES = -g -I$(top_srcdir) $(LIBRARIES_CFLAGS)  -DCOMPILATION_DATE=`date +%%s` -Wall\n\n");
			       
	if (! is_server) {
		xml_rpc_support_write ("lib%s_xml_rpcincludedir = $(includedir)/%s_xml_rpc\n\n", 
				       comp_name_lower, comp_name_lower);
		
		xml_rpc_support_write ("lib_LTLIBRARIES = lib%s_xml_rpc.la\n\n", comp_name_lower);
		
		xml_rpc_support_write ("lib%s_xml_rpc_la_SOURCES = %s_xml_rpc.h %s_xml_rpc.c %s_types.h ",
				       comp_name_lower, comp_name_lower, comp_name_lower, comp_name_lower);

		/* write struct and array references */
		xml_rpc_autoconf_write_struct_and_arrays (doc, comp_name);

		xml_rpc_support_write ("\n\n");

		xml_rpc_support_write ("lib%s_xml_rpc_la_LIBADD = $(LIBRARIES_LIBS)\n\n",
				       comp_name_lower);

		xml_rpc_support_write ("EXTRA_DIST = %s_xml_rpc.pc.in\n\n", comp_name_lower);
		
		xml_rpc_support_write ("pkgconfigdir = $(libdir)/pkgconfig\n");
		xml_rpc_support_write ("pkgconfig_DATA =  %s_xml_rpc.pc\n", comp_name_lower);
	}else {
		xml_rpc_support_write ("bin_PROGRAMS = server-%s\n\n",
				       comp_name);

		xml_rpc_support_write ("EXTRA_DIST = conf.xml\n\n");

		xml_rpc_support_write ("server_%s_LDADD = $(LIBRARIES_LIBS)\n\n", comp_name_lower);
		
		xml_rpc_support_write_sl ("server_%s_SOURCES = main.c service_dispatch.c service_dispatch.h ", comp_name_lower);

		/* write struct and array references */
		xml_rpc_autoconf_write_struct_and_arrays (doc, comp_name);

		/* write a couple of file references for each
		 * service */
		service = axl_doc_get (doc, "/xml-rpc-interface/service");
		
		while (service != NULL) {

			/* check service */
			if (! NODE_CMP_NAME (service, "service")) {
				/* get the next services */
				service = axl_node_get_next (service);
				continue;
			}

			/* get the service name */
			name         = axl_node_get_child_nth (service, 0);
			service_name = axl_node_get_content_trim (name, NULL);

			/* get the params node */
			params = axl_node_get_child_nth (service, 2);
			if (NODE_CMP_NAME (params, "resource")) {
				/* it is defined the resource for this
				 * service, skip it*/
				params = axl_node_get_next (params);
			}

			/* get type prefix */
			type_prefix = xml_rpc_support_get_function_type_prefix (params);
			
			xml_rpc_support_sl_write ("%s_%s%s.h %s_%s%s.c ",
						  /* header values */
						  comp_name_lower, service_name, 
						  /* type prefix */
						  (type_prefix != NULL) ? type_prefix : "",
						  /* body values */
						  comp_name_lower, service_name,
						  /* type prefix */
						  (type_prefix != NULL) ? type_prefix : "");

			/* free service type prefix */
			axl_free (type_prefix);

			/* get next service */
			service = axl_node_get_next (service);
		} /* end while */
	}

	/* free comp name lower */
	axl_free (comp_name_lower);

	/* close the file */
	xml_rpc_support_close_file ();

	return;

}

/** 
 * @internal
 *
 * Creates the pkg-config file for the component requested. This
 * function should not be used by server skeleton components, mainly
 * because it is only required by libraries.
 * 
 * @param doc The \ref axlDoc reference that represents the interface.
 *
 * @param new_out_dir The output directory where the file will be
 * placed.
 * 
 * @param comp_name The component name.
 */
void xml_rpc_autoconf_pkg_config_create (axlDoc * doc, 
					 char   * new_out_dir, 
					 char   * comp_name)
{
	char  * comp_name_lower;

	/* get lower version */
	comp_name_lower   = xml_rpc_support_to_lower (comp_name);

	/* open the file */
	xml_rpc_support_open_file ("%s/%s_xml_rpc.pc.in", new_out_dir, comp_name_lower);

	/* write the content */
	xml_rpc_support_multiple_write ("prefix=@prefix@\n",
					"exec_prefix=@exec_prefix@\n",
					"libdir=@libdir@\n",
					"includedir=@includedir@\n\n",
					NULL);

	xml_rpc_support_write ("Name: %s-xml-rpc\n", comp_name);

	xml_rpc_support_write ("Description: C stub connector with %s XML-RPC component\n", 
			       comp_name);
	
	xml_rpc_support_multiple_write ("Requires: vortex axl\n",
					"Version: @VERSION@\n",
					NULL);
	
	xml_rpc_support_write ("Libs: -L${libdir} -l%s_xml_rpc\n", 
			       comp_name_lower);
	
	xml_rpc_support_write ("Cflags: -I${includedir}/%s_xml_rpc\n", comp_name_lower);

	/* free allocated memory */
	axl_free (comp_name_lower);
	
	/* close the file */
	xml_rpc_support_close_file ();

	return;
}

/** 
 * @internal
 *
 * Creates the autoconf structure for a provided XML-RPC C stub
 * component. 
 * 
 * @param doc The axlDoc that represents the interface read.
 *
 * @param out_dir The destination directory where the files will be
 * generated.
 *
 * @param comp_name The component name, that is, the XML-RPC component
 * name.
 */
void xml_rpc_autoconf_c_stub_create (axlDoc * doc, 
				     char   * out_dir, 
				     char   * comp_name)
{
	
	char  * new_out_dir;
	
	/* get the component directory */
	if (exarg_is_defined ("out-stub-dir")) {
		/* get configured out dir */
		out_dir     = exarg_get_string ("out-stub-dir");
		new_out_dir = axl_strdup (out_dir);
	} else
		new_out_dir = axl_strdup_printf ("%s/client-%s", out_dir, comp_name);
	
	/* create the autogen.sh file */
	xml_rpc_autoconf_autogen_sh_create (doc, new_out_dir, comp_name, axl_false);

	/* write the configure.ac file */
	xml_rpc_autoconf_configure_ac_create (doc, new_out_dir, comp_name, axl_false);

	/* write the Makefile.am file */
	xml_rpc_autoconf_makefile_am_create (doc, new_out_dir, comp_name, axl_false);

	/* write the pkg-config file */
	xml_rpc_autoconf_pkg_config_create (doc, new_out_dir, comp_name);

	/* free the component name */
	axl_free (new_out_dir);

	return;
}


/** 
 * @internal
 * 
 * Writes all autoconf files required to produce the server xml-rpc
 * implementation.
 * 
 * @param doc The xml document that represents the server interface.
 *
 * @param out_dir The directory where the autoconf files should be
 * produced.
 *
 * @param comp_name The autoconf component name.
 */
void xml_rpc_autoconf_c_server_create (axlDoc * doc,
				       char   * out_dir,
				       char   * comp_name)
{
	char  * new_out_dir;
	char  * new_comp_name;
	
	/* get the component directory */
	if (exarg_is_defined ("out-server-dir")) 
		new_out_dir = axl_strdup_printf (exarg_get_string ("out-server-dir"));
	else
		new_out_dir = axl_strdup_printf ("%s/server-%s", out_dir, comp_name);

	/* create the server public name */
	new_comp_name = axl_strdup_printf ("Server %s implementation", comp_name);
	
	/* create the autogen.sh file */
	xml_rpc_autoconf_autogen_sh_create (doc, new_out_dir, new_comp_name, axl_true);

	/* write the configure.ac file */
	xml_rpc_autoconf_configure_ac_create (doc, new_out_dir, comp_name, axl_true);

	/* write the Makefile.am file */
	xml_rpc_autoconf_makefile_am_create (doc, new_out_dir, comp_name, axl_true);

	/* free the component name */
	axl_free (new_out_dir);

	return;
}
