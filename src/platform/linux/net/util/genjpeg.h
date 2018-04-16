#include <string.h>
#include <stdio.h>
#include <stdlib.h>

//In the background: camera app running
//openrtsp <url> -n -t -P 5 -i
//openRTSP -n -t -P 5 -i rtsp://192.168.XXX.XXX:554/onvif1
// the XXX changes, refer to app to get ip

//try -v at some point
//rtsp generates .avi files in __ intervals
//when request comes in, this app takes the latest .avi produced and extracts 1 jpeg from it

//buffer to send

int createjpeg() {
	int validjpg = 0;
	int size;
//	char szcmd[20] = "ls *.jpg -t1 | wc -l";
	char buf[1000];
	while(!validjpg) {
		printf("here");
		char cmdp1[100] = "avconv -i ";
		char cmdp2[100] = " -r 2 -f image2 ztest.jpg";
		//find size of .avi set
		char tmp[256];
		FILE *sz = popen("ls *.avi -t1 | wc -l", "r");
		fgets(tmp, 255, sz);
		size = atoi(tmp);
		char list[size][256];
		//find latest .avi
		FILE *p = popen("ls *.avi -t1 |  head -n 3 >&1", "r");
		fgets(list[0], 255, p);
		int i;
		for(i = 0; i < size; i++) {
		    fgets(list[i], 255, p);
		    printf("%s\n", list[i]);
		}
		strncat(cmdp1, list[0], 22); //put together the cmd
		printf("LIST[SIZE]: %s", list[0]);
		printf("%d\n", strlen(list[0]));		
		strcat(cmdp1, cmdp2);
		char l[256][256];
		FILE *k = popen(cmdp1, "r"); //execute on latest avi
		printf("%s\n", cmdp1);
		int err = 0;
		for(i = 0; i < 40; i++) {
			fgets(l[i], 255, k);
			if (strstr(l[i], "Error opening filters") != NULL) {
				err = 1;
				break;
			}
		}
		if (!err) break; //valid jpg
		//insert 1 second delay
	}
	//valid jpg is available
}

