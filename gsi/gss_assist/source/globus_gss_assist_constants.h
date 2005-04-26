/*
 * Portions of this file Copyright 1999-2005 University of Chicago
 * Portions of this file Copyright 1999-2005 The University of Southern California.
 *
 * This file or a portion of this file is licensed under the
 * terms of the Globus Toolkit Public License, found at
 * http://www.globus.org/toolkit/download/license.html.
 * If you redistribute this file, with or without
 * modifications, you must include this notice in the file.
 */

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file globus_gss_assist_constants.h
 * Globus GSI GSS Assist Library
 * @author Sam Lang, Sam Meder
 *
 * $RCSfile$
 * $Revision$
 * $Date$
 */
#endif

#ifndef GLOBUS_GSI_GSS_ASSIST_CONSTANTS_H
#define GLOBUS_GSI_GSS_ASSIST_CONSTANTS_H

#ifndef EXTERN_C_BEGIN
#    ifdef __cplusplus
#        define EXTERN_C_BEGIN extern "C" {
#        define EXTERN_C_END }
#    else
#        define EXTERN_C_BEGIN
#        define EXTERN_C_END
#    endif
#endif

EXTERN_C_BEGIN

/**
 * @defgroup globus_gsi_gss_assist_constants 
 * GSI GSS Assist Constants
 */
/* GSI GSS Assist Error codes
 * @ingroup globus_gsi_gss_assist_constants
 */
typedef enum
{
    GLOBUS_GSI_GSS_ASSIST_ERROR_SUCCESS = 0,
    GLOBUS_GSI_GSS_ASSIST_ERROR_WITH_ARGUMENTS = 1,
    GLOBUS_GSI_GSS_ASSIST_ERROR_USER_ID_DOESNT_MATCH = 2,
    GLOBUS_GSI_GSS_ASSIST_ERROR_IN_GRIDMAP_NO_USER_ENTRY = 3,
    GLOBUS_GSI_GSS_ASSIST_ERROR_WITH_GRIDMAP = 4,
    GLOBUS_GSI_GSS_ASSIST_ERROR_INVALID_GRIDMAP_FORMAT = 5,
    GLOBUS_GSI_GSS_ASSIST_ERROR_ERRNO = 6,
    GLOBUS_GSI_GSS_ASSIST_ERROR_WITH_INIT = 7,
    GLOBUS_GSI_GSS_ASSIST_ERROR_WITH_WRAP = 8,
    GLOBUS_GSI_GSS_ASSIST_ERROR_WITH_TOKEN = 9,
    GLOBUS_GSI_GSS_ASSIST_ERROR_EXPORTING_CONTEXT = 10,
    GLOBUS_GSI_GSS_ASSIST_ERROR_IMPORTING_CONTEXT = 11,
    GLOBUS_GSI_GSS_ASSIST_ERROR_INITIALIZING_CALLOUT_HANDLE = 12,
    GLOBUS_GSI_GSS_ASSIST_ERROR_WITH_CALLOUT_CONFIG = 13,
    GLOBUS_GSI_GSS_ASSIST_CALLOUT_ERROR = 14,
    GLOBUS_GSI_GSS_ASSIST_GSSAPI_ERROR = 15,
    GLOBUS_GSI_GSS_ASSIST_GRIDMAP_LOOKUP_FAILED = 16,
    GLOBUS_GSI_GSS_ASSIST_BUFFER_TOO_SMALL = 17,
    GLOBUS_GSI_GSS_ASSIST_ERROR_CANONICALIZING_HOSTNAME = 18,
    GLOBUS_GSI_GSS_ASSIST_ERROR_LAST = 19
} globus_gsi_gss_assist_error_t;

EXTERN_C_END

#endif
