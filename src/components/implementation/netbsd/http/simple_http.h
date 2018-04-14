/**
 * MIT License
 *
 * Copyright (c) 2012 Gabriel Parmer
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef SIMPLE_HTTP_H
#define SIMPLE_HTTP_H

struct http_req {
	int   fd;

	/* Request information */
	char *request;
	int   req_len;
	char *path; 		/* points to string inside of request */

	/* Response information */
	char *resp_head, *response;
	int   resp_hd_len, resp_len;
};


/*
 * Allocate a new http_req for the file descriptor, and with the
 * specific request.
 */
struct http_req *shttp_alloc_req(int fd, char *request);

/*
 * Will free the memory for the request, response, and will close the
 * file descriptor.
 */
void shttp_free_req(struct http_req *r);

/* populate the ->path filed in http_req */
int shttp_get_path(struct http_req *r);

/*
 * Take the answer, which is the response (of length len) to the
 * request with the given path, and formulate the response to be
 * written out to the client in ->response.  Note that answer will not
 * be freed by this function, and must be freed by the caller.
 */
int shttp_alloc_response_head(struct http_req *r, char *resp, int rlen);

#endif
