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

#include "globus_gram_protocol.h"
#include <string.h>

int main(int argc, char * argv[])
{
    int rc = 0;
    int i;
    char * str;
    int verbose = 0;
    int testno = 0;

    if(argc > 1)
    {
	testno = atoi(argv[1]);
    }
    if(argc > 2)
    {
	verbose = 1;
    }

    rc = globus_module_activate(GLOBUS_GRAM_PROTOCOL_MODULE);
    if(rc != GLOBUS_SUCCESS)
    {
	goto out;
    }
    if(testno == 0 || testno == 1)
    {
	for(i = -1; i < GLOBUS_GRAM_PROTOCOL_ERROR_LAST+1; i++)
	{
	    str = (char *) globus_gram_protocol_error_string(i);

	    if(str == NULL)
	    {
		rc = 1;
		goto error_exit;
	    }

	    if(verbose) printf("%d: %s\n", i, str);
	}
    }
    if(testno == 0 || testno == 2)
    {
	char *error1 = "error1";
	char *error2 = "error2";
	char *error3 = "error3";

	globus_gram_protocol_error_7_hack_replace_message(error1);
	str = globus_gram_protocol_error_string(7);
	if(verbose)
	{
	    printf("comparing %p:%s to %p:%s\n", 
		    error1, error1, str, str);
	}
	if(strcmp(str, "error1") != 0)
	{
	    rc = GLOBUS_FAILURE;
	}
	globus_gram_protocol_error_7_hack_replace_message(error2);
	str = globus_gram_protocol_error_string(7);
	if(verbose)
	{
	    printf("comparing %p:%s to %p:%s\n", 
		    error2, error2, str, str);
	}
	if(strcmp(str, error2) != 0)
	{
	    rc = GLOBUS_FAILURE;
	}
	globus_gram_protocol_error_7_hack_replace_message(error3);
	str = globus_gram_protocol_error_string(7);
	if(verbose)
	{
	    printf("comparing %p:%s to %p:%s\n", 
		    error3, error3, str, str);
	}
	if(strcmp(str, error3) != 0)
	{
	    rc = GLOBUS_FAILURE;
	}
    }

error_exit:
    globus_module_deactivate_all();
out:
    if(rc == 0)
    {
	printf("ok\n");
    }
    return rc;
}
