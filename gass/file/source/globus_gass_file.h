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

/******************************************************************************
globus_gass_file_api.h
 
Description:
    This header contains the GASS File Access API definitions
 
CVS Information:
 
    $Source$
    $Date$
    $Revision$
    $Author$
******************************************************************************/
#ifndef _GLOBUS_GASS_INCLUDE_GLOBUS_GASS_FILE_API_H
#define _GLOBUS_GASS_INCLUDE_GLOBUS_GASS_FILE_API_H

#ifndef EXTERN_C_BEGIN
#ifdef __cplusplus
#define EXTERN_C_BEGIN extern "C" {
#define EXTERN_C_END }
#else
#define EXTERN_C_BEGIN
#define EXTERN_C_END
#endif
#endif

#include "globus_common.h"

#include <stdio.h>
#include <fcntl.h>

EXTERN_C_BEGIN

int
globus_gass_open(
    char *				file,
    int					oflags,
    ...);

FILE *
globus_gass_fopen(
    char *				file,
    char *				mode);
int
globus_gass_close(
    int					fd);
int
globus_gass_fclose(
    FILE *				f);

/******************************************************************************
 *                    Module Definition
 *****************************************************************************/
extern globus_module_descriptor_t globus_i_gass_file_module;
#define GLOBUS_GASS_FILE_MODULE (&globus_i_gass_file_module)

EXTERN_C_END

#endif
