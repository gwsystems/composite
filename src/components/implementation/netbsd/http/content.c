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

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* 10 MB is max size */
#define MAX_CONTENT_SZ (1024*1024*10)

char *
error_resp(char *path, int *len)
{
	const char eresponse[] = "<html><head><title>X-P</title></head><body><font face=\"sans-serif\"><center><h1>X-P</h1><p>Could not find content at <b>%s</b>.</p></font></center></body>";
	char *resp;
	int sz = strlen(eresponse) + strlen(path);

	resp = malloc(sz);
	if (!resp) return NULL;
	*len = sprintf(resp, eresponse, path);
	return resp;
}

int
sanity_check(char *path)
{ return (path[0] == '.' || path[0] == '/'); }


char resp[] = "<HTML><h1>GW SHC says <a href=\"http://giphy.com/gifs/14kdiJUblbWBXy/tile\">HACK THE PLANET!</a></h1></HTML>";

char *
content_get(char *path, int *content_len)
{
	int content_fd, amnt_read = 0;
	struct stat s;

#ifdef THINK_TIME
	sleep(1);
#endif

	/* Bad path?  No file?  Too large? */
	/* TODO implement */
	//if (sanity_check(path) ||
	//    stat(path, &s)     ||
	//    s.st_size > MAX_CONTENT_SZ) goto err;

	/* NO, we static reply */
	//content_fd = open(path, O_RDONLY);
	//if (content_fd < 0) goto err;

	//resp = malloc(s.st_size);
	//if (!resp) goto err_close;

	//while (amnt_read < s.st_size) {
	//	int ret = read(content_fd, resp + amnt_read,
	//		       s.st_size - amnt_read);

	//	if (ret < 0) goto err_free;
	//	amnt_read += ret;
	//}

	*content_len = sizeof(resp);

	return resp;
}
