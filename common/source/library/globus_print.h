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

#if !defined(GLOBUS_INCLUDE_GLOBUS_PRINT_H)
#define GLOBUS_INCLUDE_GLOBUS_PRINT_H 1

#include "globus_common_include.h"
#include "globus_module.h"

EXTERN_C_BEGIN

/**
 */
extern void globus_fatal(char *msg, ...);
/**
 */
extern void globus_silent_fatal(void);
/**
 */
extern void globus_error(char *msg, ...);
/**
 */
extern void globus_warning(char *msg, ...);
/**
 */
extern void globus_notice(char *msg, ...);
/**
 */
extern void globus_perror(char *msg, ...);
/**
 */
extern void globus_fatal_perror(char *msg, ...);
/**
 */
extern char *globus_assert_sprintf(char *msg, ...);
/**
 */
extern char *globus_get_unique_session_string(void);

void
globus_panic(
    globus_module_descriptor_t *        module,
    globus_result_t                     result,
    const char *                        message,
    ...);
    
EXTERN_C_END

#endif


