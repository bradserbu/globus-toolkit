#include "globus_i_xio_http.h"

/**
 * @defgroup globus_i_xio_http_client Internal HTTP Client Implementation
 */

static
globus_result_t
globus_l_xio_http_client_write_request(
    globus_xio_operation_t              op,
    globus_i_xio_http_handle_t *        http_handle);

static
void
globus_l_xio_http_client_write_request_callback(
    globus_xio_operation_t              op,
    globus_result_t                     result,
    globus_size_t                       nbytes,
    void *                              user_arg);

static
void
globus_l_xio_http_client_read_response_callback(
    globus_xio_operation_t              op,
    globus_result_t                     result,
    globus_size_t                       nbytes,
    void *                              user_arg);

static
globus_result_t
globus_l_xio_http_client_parse_response(
    globus_i_xio_http_handle_t *        http_handle,
    globus_bool_t *                     done);

static
void
globus_l_xio_http_client_call_ready_callback(
    globus_i_xio_http_handle_t *        http_handle,
    globus_result_t                     result);

/**
 * Client-side connection open callback
 * @ingroup globus_i_xio_http_client
 *
 * Called as a result of open at the transport level. If this was
 * successful, we will write the HTTP request which corresponds to our
 * target to this new connection.
 *
 * If this function succeeds, the open will remain unfinished until that
 * write completes. If an error happens, this function will close the handle
 * internally and call globus_xio_driver_finished_open() before returning.
 * 
 * @param op
 *     operation associated with the open call.
 * @param result
 *     Lower-level protocol result from open.
 * @param user_arg
 *     A void pointer pointing to a #globus_i_xio_http_driver_t 
 *
 * @return void
 */
void
globus_i_xio_http_client_open_callback(
    globus_xio_operation_t              op,
    globus_result_t                     result,
    void *                              user_arg)
{
    globus_i_xio_http_handle_t *        http_handle = user_arg;
    globus_xio_driver_handle_t          handle;
    globus_result_t                     result2;

    handle = http_handle->handle;
    if (result != GLOBUS_SUCCESS)
    {
        http_handle->send_state = GLOBUS_XIO_HTTP_CLOSE;

        goto error_exit;
    }

    result = globus_l_xio_http_client_write_request(op, http_handle);

    if (result != GLOBUS_SUCCESS)
    {
        http_handle->send_state = GLOBUS_XIO_HTTP_EOF;
        goto error_exit;
    }

    return;

error_exit:
    if (http_handle->send_state == GLOBUS_XIO_HTTP_EOF)
    {
        result2 = globus_xio_driver_operation_create(
                &http_handle->close_operation,
                http_handle->handle);

        if (result2 != GLOBUS_SUCCESS)
        {
            /*
             * We can't close the transport, we'll make the best of the
             * situation. Resetting this flag makes the lower code destroy
             * the handle.
             */
            http_handle->send_state = GLOBUS_XIO_HTTP_CLOSE;

            goto destroy_handle_exit;
        }
        result2 = globus_xio_driver_pass_close(
                http_handle->close_operation,
                globus_i_xio_http_close_callback,
                http_handle);
        if (result2 == GLOBUS_SUCCESS)
        {
            http_handle->user_close = GLOBUS_FALSE;
        }
        else
        {
            http_handle->send_state = GLOBUS_XIO_HTTP_CLOSE;
        }
    }
destroy_handle_exit:
    if (http_handle->send_state == GLOBUS_XIO_HTTP_CLOSE)
    {
        globus_i_xio_http_handle_destroy(http_handle);
        globus_libc_free(http_handle);
        http_handle = NULL;
    }
    globus_xio_driver_finished_open(
            handle,
            http_handle,
            op,
            result);
}
/* globus_i_xio_http_client_open_callback() */

/**
 * Write an HTTP request
 * @ingroup globus_i_xio_http_client
 *
 * Composes and writes an HTTP request which corresponds to the attributes and
 * target which were passed to the HTTP driver's open implementation.
 *
 * @param op
 *     Previously allocated XIO operation which is associaed with the current
 *     open being processed by the driver.
 * @param http_handle
 *     An HTTP handle containing state about this connection.
 *
 * @returns This function returns GLOBUS_SUCCESS, GLOBUS_XIO_ERROR_MEMORY, or
 * other errors from the XIO system or transport driver.
 *
 * @retval GLOBUS_SUCCESS
 *     The request was successfully composed and passed to the transport
 *     driver.
 * @retval GLOBUS_XIO_ERROR_MEMORY
 *     Unable to compose or write the request because of memory constraints.
 */
static
globus_result_t
globus_l_xio_http_client_write_request(
    globus_xio_operation_t              op,
    globus_i_xio_http_handle_t *        http_handle)
{
    globus_result_t                     result;
    int                                 rc;
    globus_fifo_t                       iovecs;
    char *                              str;
    globus_size_t                       send_size;
    globus_xio_iovec_t *                iov;
    globus_xio_http_header_t *          current_header;
    int                                 i;
    char                                size_buffer[sizeof(globus_size_t)+3];
    GlobusXIOName(globus_i_xio_http_client_write_request);

    globus_assert(http_handle->send_state == GLOBUS_XIO_HTTP_REQUEST_LINE);

    /*
     * Compose HTTP request:
     * Method URI HTTP-Version\r\n
     * Header-Name: Header-Value\r\n
     * ...
     * \r\n
     */
    rc = globus_fifo_init(&iovecs);

    if (rc != GLOBUS_SUCCESS)
    {
        result = GlobusXIOErrorMemory("iovecs");

        goto error_exit;
    }

    /* Default request method is GET, if none is provided in open attrs */
    if ((str = http_handle->request_info.method) == NULL)
    {
        str = "GET";
    }

    /* Certain HTTP methods should be accompanied by an entity-body.  We handle
     * the cases described in RFC 2616, but the user may use some other method
     * perhaps, we allow an override in the open attr.
     *
     * In cases where an entity is included, we require the application to use
     * the GLOBUS_XIO_HTTP_HANDLE_SET_END_OF_ENTITY handle command to signal
     * this.
     */
    if (! http_handle->request_info.headers.entity_needed)
    {
        http_handle->request_info.headers.entity_needed =
            globus_i_xio_http_method_requires_entity(str);
    }

    GLOBUS_XIO_HTTP_COPY_BLOB(&iovecs, str, strlen(str), free_iovecs_exit);
    GLOBUS_XIO_HTTP_COPY_BLOB(&iovecs, " ", 1, free_iovecs_exit);

    if (((str = http_handle->request_info.uri) == NULL) &&
            ((str = http_handle->target_info.uri) == NULL))
    {
        str = "/";
    }
    GLOBUS_XIO_HTTP_COPY_BLOB(&iovecs, str, strlen(str), free_iovecs_exit);

    if (http_handle->request_info.http_version == GLOBUS_XIO_HTTP_VERSION_1_0)
    {
        str = " HTTP/1.0\r\n";
    }
    else
    {
        str = " HTTP/1.1\r\n";
    }
    GLOBUS_XIO_HTTP_COPY_BLOB(&iovecs, str, strlen(str), free_iovecs_exit);

    current_header = globus_hashtable_first(
            &http_handle->request_info.headers.headers);

    while (current_header)
    {
        GLOBUS_XIO_HTTP_COPY_BLOB(&iovecs,
                current_header->name,
                strlen(current_header->name),
                free_iovecs_exit);
        GLOBUS_XIO_HTTP_COPY_BLOB(&iovecs,
                ": ",
                2,
                free_iovecs_exit);
        GLOBUS_XIO_HTTP_COPY_BLOB(&iovecs,
                current_header->value,
                strlen(current_header->value),
                free_iovecs_exit);
        GLOBUS_XIO_HTTP_COPY_BLOB(&iovecs,
                "\r\n",
                2,
                free_iovecs_exit);
        current_header = globus_hashtable_next(
                &http_handle->request_info.headers.headers);
    }

    /*
     * Special headers we generate, related to entity size management. These
     * will be intercepted in the attr command which sets a header.
     */
    if (http_handle->request_info.http_version != GLOBUS_XIO_HTTP_VERSION_1_0)
    {
        /* HTTP/1.1 clients MUST send a Host header */
        GLOBUS_XIO_HTTP_COPY_BLOB(&iovecs,
                "Host: ",
                6,
                free_iovecs_exit);
        GLOBUS_XIO_HTTP_COPY_BLOB(&iovecs,
                http_handle->target_info.host,
                strlen(http_handle->target_info.host),
                free_iovecs_exit);
        GLOBUS_XIO_HTTP_COPY_BLOB(&iovecs,
                "\r\n",
                2,
                free_iovecs_exit);
    }
    if (http_handle->request_info.headers.entity_needed)
    {
        if ((http_handle->request_info.http_version ==
                GLOBUS_XIO_HTTP_VERSION_1_0) || 
                ((http_handle->request_info.headers.transfer_encoding ==
                    GLOBUS_XIO_HTTP_TRANSFER_ENCODING_IDENTITY) &&
                    http_handle->request_info.headers.content_length_set))
        {
            if (http_handle->request_info.http_version !=
                    GLOBUS_XIO_HTTP_VERSION_1_0)
            {
                GLOBUS_XIO_HTTP_COPY_BLOB(&iovecs,
                        "Transfer-Encoding: identity\r\n",
                        29,
                        free_iovecs_exit);
            }

            GLOBUS_XIO_HTTP_COPY_BLOB(&iovecs,
                    "Content-Length: ",
                    16,
                    free_iovecs_exit);

            rc = sprintf(size_buffer,
                    "%u\r\n",
                    http_handle->request_info.headers.content_length);

            globus_assert (rc > 0);

            GLOBUS_XIO_HTTP_COPY_BLOB(&iovecs,
                    size_buffer,
                    rc,
                    free_iovecs_exit);
            http_handle->request_info.headers.transfer_encoding =
                    GLOBUS_XIO_HTTP_TRANSFER_ENCODING_IDENTITY;
        }
        else
        {
            http_handle->request_info.headers.transfer_encoding =
                GLOBUS_XIO_HTTP_TRANSFER_ENCODING_CHUNKED;

            GLOBUS_XIO_HTTP_COPY_BLOB(&iovecs,
                    "Transfer-Encoding: chunked\r\n",
                    28,
                    free_iovecs_exit);
        }
    }

    GLOBUS_XIO_HTTP_COPY_BLOB(&iovecs,
            "\r\n",
            2,
            free_iovecs_exit);

    http_handle->header_iovcnt = globus_fifo_size(&iovecs);
    http_handle->header_iovec = globus_libc_malloc(
            http_handle->header_iovcnt * sizeof(globus_xio_iovec_t));
    if (http_handle->header_iovec == NULL)
    {
        goto free_iovecs_exit;
    }

    /* Convert fifo to iovec array, counting up size for wait_for_nbytes
     * parameter to globus_xio_driver_pass_write.
     */
    for (i = 0, send_size = 0; i < http_handle->header_iovcnt; i++)
    {
        iov = globus_fifo_dequeue(&iovecs);

        globus_assert(iov != NULL);

        http_handle->header_iovec[i].iov_base = iov->iov_base;
        http_handle->header_iovec[i].iov_len = iov->iov_len;

        send_size += iov->iov_len;

        globus_libc_free(iov);
    }

    result = globus_xio_driver_pass_write(
            op,
            http_handle->header_iovec,
            http_handle->header_iovcnt,
            send_size,
            globus_l_xio_http_client_write_request_callback,
            http_handle);

    if (result != GLOBUS_SUCCESS)
    {
        goto free_headers_exit;
    }
    globus_fifo_destroy(&iovecs);

    return GLOBUS_SUCCESS;

free_headers_exit:
    for (i = 0; i < http_handle->header_iovcnt; i++)
    {
        globus_libc_free(http_handle->header_iovec[i].iov_base);
    }
    globus_libc_free(http_handle->header_iovec);

    http_handle->header_iovec = NULL;
    http_handle->header_iovcnt = 0;

free_iovecs_exit:
    while (! globus_fifo_empty(&iovecs))
    {
        iov = globus_fifo_dequeue(&iovecs);

        free(iov->iov_base);
        free(iov);
    }
    globus_fifo_destroy(&iovecs);

error_exit:
    return result;
}
/* globus_l_xio_http_client_write_request() */

/**
 * Request Written Callback 
 * @ingroup globus_i_xio_http_write_request_callback
 *
 * Called when the response has been completely written by the transport.
 * The driver then registers a read for the HTTP response.
 * After this has been registered, the HTTP driver is done "opening" the HTTP
 * handle, and will signal this to the user via
 * globus_xio_driver_finished_open(). The user may then send any data
 * payload associated with the request via the globus_xio_write() family of
 * functions.
 *
 * @param op
 *     XIO operation originally associated with the open of this handle.
 * @param result
 *     Result from the transport of writing the HTTP request.
 * @param nbytes
 *     Number of bytes in the message that were written.
 * @param user_arg
 *     Void pointer containing the #globus_i_xio_http_handle_t.
 *
 * @return void
 */
static
void
globus_l_xio_http_client_write_request_callback(
    globus_xio_operation_t              op,
    globus_result_t                     result,
    globus_size_t                       nbytes,
    void *                              user_arg)
{
    globus_i_xio_http_handle_t *        http_handle = user_arg;
    GlobusXIOName(globus_l_xio_http_client_write_request_callback);

    if (result != GLOBUS_SUCCESS)
    {
        goto error_exit;
    }

    /* Synthesize read operation for response */
    result = globus_xio_driver_operation_create(
            &http_handle->response_info.read_operation,
            http_handle->handle);

    if (result != GLOBUS_SUCCESS)
    {
        goto error_exit;
    }

    /* This may have to be different if we have persistent connections */
    http_handle->read_buffer.iov_len = GLOBUS_XIO_HTTP_CHUNK_SIZE;
    http_handle->read_buffer.iov_base = globus_libc_malloc(
            GLOBUS_XIO_HTTP_CHUNK_SIZE);

    http_handle->read_iovec.iov_base = http_handle->read_buffer.iov_base;
    http_handle->read_iovec.iov_len = http_handle->read_buffer.iov_len;

    if (http_handle->read_buffer.iov_base == NULL)
    {
        result = GlobusXIOErrorMemory("read_buffer");

        goto destroy_op_exit;
    }

    http_handle->parse_state = GLOBUS_XIO_HTTP_STATUS_LINE;

    if (!http_handle->request_info.headers.entity_needed)
    {
        http_handle->send_state = GLOBUS_XIO_HTTP_EOF;
    }
    else if (http_handle->request_info.headers.transfer_encoding ==
            GLOBUS_XIO_HTTP_TRANSFER_ENCODING_IDENTITY)
    {
        http_handle->send_state = GLOBUS_XIO_HTTP_IDENTITY_BODY;
    }
    else
    {
        http_handle->send_state = GLOBUS_XIO_HTTP_CHUNK_BODY;
    }

    result = globus_xio_driver_pass_read(
            http_handle->response_info.read_operation,
            &http_handle->read_buffer,
            1,
            1,
            globus_l_xio_http_client_read_response_callback,
            http_handle);

    if (result != GLOBUS_SUCCESS)
    {
        goto free_read_buffer_exit;
    }

    globus_xio_driver_finished_open(
            http_handle->handle,
            http_handle,
            op,
            result);

    return;

destroy_op_exit:
    globus_xio_driver_operation_destroy(
            http_handle->response_info.read_operation);
    http_handle->response_info.read_operation = NULL;
free_read_buffer_exit:
    globus_libc_free(http_handle->read_buffer.iov_base);
    http_handle->read_buffer.iov_len = 0;
error_exit:
    /* Does the http handle need to be freed here? */
    globus_xio_driver_finished_open(
            http_handle->handle,
            http_handle,
            op,
            result);
}
/* globus_i_xio_http_client_write_request_callback() */

/**
 * Read response callback
 * @ingroup globus_i_xio_http_client
 *
 * Called when part of the response has been read by the transport driver.
 * If all of the response headers are present, or some error occurs at the
 * transport layer, or the response isn't well-formed, then the response ready
 * callback set in the handle's open attribute will be called. If the header
 * information isn't all present, then another read will be passed
 * to the transport.
 *
 * @param op
 *     XIO operation associated with the response read.
 * @param result
 *     Transport-level result of reading the response.
 * @param nbytes
 *     Amount of data read by the transport.
 * @param user_arg
 *     Void * pointing to the #globus_i_xio_http_handle_t associated
 *     with this response.
 *
 * @return void
 */
static
void
globus_l_xio_http_client_read_response_callback(
    globus_xio_operation_t              op,
    globus_result_t                     result,
    globus_size_t                       nbytes,
    void *                              user_arg)
{
    globus_i_xio_http_handle_t *        http_handle = user_arg;
    globus_bool_t                       eof = GLOBUS_FALSE;
    globus_bool_t                       done;
    globus_bool_t                       copy_residue = GLOBUS_FALSE;
    GlobusXIOName(globus_l_xio_http_client_read_response_callback);

    if (result != GLOBUS_SUCCESS)
    {
        if (globus_xio_error_is_eof(result))
        {
            eof = GLOBUS_TRUE;
        }
        else
        {
            goto error_exit;
        }
    }

    globus_assert(!http_handle->parsed_headers);

    http_handle->read_buffer_valid += nbytes;

    /* Parsed response line and headers. */
    result = globus_l_xio_http_client_parse_response(http_handle, &done);

    if (result == GLOBUS_SUCCESS && !done)
    {
        goto reregister_read;
    }

    /* Parsed all header information */
    http_handle->parsed_headers = GLOBUS_TRUE;

    /* If user registered a read before we finished parsing, we'll
     * have to handle it after we call the ready callback
     */
    if (http_handle->read_operation.operation != NULL)
    {
        copy_residue = GLOBUS_TRUE;
    }

    globus_l_xio_http_client_call_ready_callback(http_handle, result);

    if (copy_residue)
    {
        globus_i_xio_http_parse_residue(http_handle);
    }

    return;

reregister_read:
    if (eof)
    {
        /* Header block wasn't complete before eof. */
        goto error_exit;
    }

    result = globus_i_xio_http_clean_read_buffer(http_handle);

    if (result != GLOBUS_SUCCESS)
    {
        goto error_exit;
    }

    result = globus_xio_driver_pass_read(
            http_handle->response_info.read_operation,
            &http_handle->read_iovec,
            1,
            1,
            globus_l_xio_http_client_read_response_callback,
            http_handle);

    if (result != GLOBUS_SUCCESS)
    {
        goto error_exit;
    }

    return;
error_exit:
    if (http_handle->read_operation.operation != NULL)
    {
        copy_residue = GLOBUS_TRUE;
    }

    globus_l_xio_http_client_call_ready_callback(http_handle, result);

    if (copy_residue)
    {
        /* User registered a read before we parsed everything, handle
         * residue.
         */
        globus_i_xio_http_parse_residue(http_handle);
    }
}
/* globus_l_xio_http_client_read_response_callback() */

/**
 * Parse the response to an HTTP request
 * @ingroup globus_i_xio_http_client
 *
 * Parses the response line and then uses globus_i_xio_http_header_parse() to
 * parse the header block. If the entire response header section is read, 
 * the boolean pointed to by @a done will be modified to be GLOBUS_TRUE.
 *
 * @param http_handle
 * @param done
 *
 * @return
 *     This function returns GLOBUS_SUCCESS, GLOBUS_XIO_HTTP_ERROR_PARSE,
 *     or GLOBUS_XIO_ERROR_MEMORY. Other errors may be returned from
 *     globus_i_xio_http_header_parse().
 * @retval GLOBUS_SUCCESS
 *     No parsing errors occurred while parsing the status line or headers.
 *     Parsing may still be incomplete, depending on the final value of 
 *     @a done.
 * @retval GLOBUS_XIO_HTTP_ERROR_PARSE
 *     Parse error reading the HTTP Status line
 * @retval GLOBUS_XIO_ERROR_MEMORY
 *     Parsing failed because of memory constraints.
 */
static
globus_result_t
globus_l_xio_http_client_parse_response(
    globus_i_xio_http_handle_t *        http_handle,
    globus_bool_t *                     done)
{
    globus_result_t                     result;
    char *                              eol;
    char *                              current_offset;
    int                                 parsed;
    unsigned int                        http_major;
    unsigned int                        http_minor;
    int                                 rc;
    GlobusXIOName(globus_l_xio_http_client_parse_response);

    if (http_handle->parse_state == GLOBUS_XIO_HTTP_STATUS_LINE)
    {
        /* Parse the status line:
         *
         * HTTP-Version SP Status-Code SP Reason-Phrase CRLF
         */
        current_offset = ((char *) (http_handle->read_buffer.iov_base))
                + http_handle->read_buffer_offset;

        eol = globus_i_xio_http_find_eol(
                current_offset,
                http_handle->read_buffer_valid);
        if (eol == NULL)
        {
            *done = GLOBUS_FALSE;

            return GLOBUS_SUCCESS;
        }
        *eol = '\0';

        rc = sscanf(current_offset,
                "HTTP/%u.%u %n",
                &http_major,
                &http_minor,
                &parsed);

        if (rc < 2)
        {
            result = GlobusXIOHttpErrorParse("Http-Version", current_offset);

            goto error_exit;
        }

        http_handle->response_info.http_version = 
            globus_i_xio_http_guess_version(http_major, http_minor);

        current_offset += parsed;

        /* Status-Code */
        rc = sscanf(current_offset,
                "%d %n",
                &http_handle->response_info.status_code,
                &parsed);

        if (http_handle->response_info.status_code < 100 ||
                http_handle->response_info.status_code > 599)
        {
            result = GlobusXIOHttpErrorParse("Status-Code", current_offset);

            goto error_exit;
        }

        current_offset += parsed;

        /* Reason Phrase */
        http_handle->response_info.reason_phrase =
            globus_libc_strdup(current_offset);

        if (http_handle->response_info.reason_phrase == NULL)
        {
            result = GlobusXIOErrorMemory("reason_phrase");

            goto error_exit;
        }

        /* Set current offset after the end of CRLF at end of this line */
        current_offset = eol+2;

        parsed = current_offset - ((char *) http_handle->read_buffer.iov_base
                + http_handle->read_buffer_offset);
        http_handle->read_buffer_valid -= parsed;
        http_handle->read_buffer_offset += parsed;

        http_handle->parse_state = GLOBUS_XIO_HTTP_HEADERS;
    }

    return globus_i_xio_http_header_parse(http_handle, done);

error_exit:
    parsed = current_offset - ((char *) http_handle->read_buffer.iov_base
                + http_handle->read_buffer_offset);

    /* Chop of what we managed to parse from the buffer */
    http_handle->read_buffer_valid -= parsed;
    http_handle->read_buffer_offset += parsed;

    return result;
}
/* globus_i_xio_http_client_parse_response() */

/**
 * Call the response ready callback associated with a handle.
 * @ingroup globus_i_xio_http_client
 *
 * @param http_handle
 *     Handle which is done parsing the response.
 * @param result
 *     Result to pass to the user.
 *
 * @return void
 */
static
void
globus_l_xio_http_client_call_ready_callback(
    globus_i_xio_http_handle_t *        http_handle,
    globus_result_t                     result)
{
    if (http_handle->request_info.callback == NULL)
    {
        /* User is missing out */
        return;
    }
    http_handle->request_info.callback(
            http_handle->request_info.callback_arg,
            result,
            http_handle->response_info.status_code,
            http_handle->response_info.reason_phrase,
            http_handle->response_info.http_version,
            http_handle->response_info.headers.headers);
}
/* globus_l_xio_http_client_call_ready_callback() */
