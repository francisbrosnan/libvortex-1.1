/**
 * C client stub to invoke services exported by the XML-RPC component: test.
 *
 * This file was generated by xml-rpc-gen tool, from Vortex Library
 * project.
 *
 * Vortex Library homepage: http://vortex.aspl.es
 * Axl Library homepage: http://xml.aspl.es
 * Advanced Software Production Line: http://www.aspl.es
 */
#ifndef __XML_RPC_STRUCT_VALUES_H__
#define __XML_RPC_STRUCT_VALUES_H__

/* include base library */
#include <vortex.h>
/* include xml-rpc library */
#include <vortex_xml_rpc.h>
#include <test_types.h>

/* Values type declaration */
/* user definition declared at: test_types.h */
struct __Values {
	int count;
	double fraction;
	axl_bool  status;
};

/* support for c++ declarations */
BEGIN_C_DECLS

/* (un)marshaller support functions  */
XmlRpcStruct * test_values_marshall (VortexCtx * _ctx_, Values * ref, axl_bool  dealloc);
Values * test_values_unmarshall (XmlRpcStruct * ref, axl_bool  dealloc);

/* memory (de)allocation functions */
Values * test_values_new (int count, double fraction, axl_bool  status);

Values * test_values_copy (Values * ref);

void test_values_free (Values * ref);

END_C_DECLS

#endif