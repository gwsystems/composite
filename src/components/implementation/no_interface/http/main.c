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

/*
 * This is a HTTP server.  It accepts connections on port 8080, and
 * serves a local static document.
 *
 * The clients you can use are
 * - httperf (e.g., httperf --port=8080),
 * - wget (e.g. wget localhost:8080 /),
 * - or even your browser.
 *
 * To measure the efficiency and concurrency of your server, use
 * httperf and explore its options using the manual pages (man
 * httperf) to see the maximum number of connections per second you
 * can maintain over, for example, a 10 second period.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <sys/wait.h>
#include <pthread.h>

#include <util.h> 		/* client_process */
#include <server.h>		/* server_accept and server_create */

#include <rk.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <rk_inv.h>
#include <rk_libc_override.h>

/*
 * This is the function for handling a _single_ request.  Understand
 * what each of the steps in this function do, so that you can handle
 * _multiple_ requests.  Use this function as an _example_ of the
 * basic functionality.  As you increase the server in functionality,
 * you will want to probably keep all of the functions called in this
 * function, but define different code to use them.
 */

void
server_single_request(int accept_fd)
{
	int fd;

	/*
	 * The server thread will always want to be doing the accept.
	 * That main thread will want to hand off the new fd to the
	 * new threads/processes/thread pool.
	 */
	fd = server_accept(accept_fd);
	client_process(fd);

	return;
}

int
main(int argc, char *argv[])
{
	short int port;
	int accept_fd;

	if (argc != 2) {
		printf("Proper usage of http server is:\n%s <port> where\n"
		       "port is the port to serve on\n",
		       argv[0]);
		return -1;
	}

	port = atoi(argv[1]);
	accept_fd = server_create(port);
	if (accept_fd < 0) return -1;

	int i = 0;
	for (; i < 5 ; i++) {
		server_single_request(accept_fd);
	}
	while(1) printf("ERROR\n");
	/* NO CLOSE!!! */
	//close(accept_fd);

	return 0;
}

int spdid;
extern struct cos_component_information cos_comp_info;

void
cos_init(void)
{
	printc("Welcome to the simple webserver\n");
	printc("cos_component_infomration spdid: %ld\n", cos_comp_info.cos_this_spd_id);

	spdid = cos_comp_info.cos_this_spd_id;

	/* Using cos_defcompinfo_init to enable us to use printf... */
	printc("cos_defcompinfo_init\n");
	cos_defcompinfo_init();

	/* Test RK entry */
	printc("calling rk_inv_entry\n");
	get_boot_done();
	test_entry(0, 1, 2, 3);

	rk_libcmod_init();

	printc("Calling printf...\n");
	printf("testing, testing, this is printf\n");

	char *argv[2];
	argv[0] = "http.o";
	argv[1] = "8080";

	main(2, argv);
	while (1);
}
