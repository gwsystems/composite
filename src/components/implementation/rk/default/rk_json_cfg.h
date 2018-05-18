#ifndef RK_JSON_CFG_H
#define RK_JSON_CFG_H

/* DEFAULT CFG used in PAWS, UDPSERV APPLICATIONS */
//#define RK_JSON_DEFAULT_QEMU "{,\"net\":{,\"if\":\"vioif0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"10.0.120.101\",\"mask\":\"24\",},\"cmdline\":\"paws.bin\",},\0"
//
//#define RK_JSON_DEFAULT_HW "{,\"net\":{,\"if\":\"wm0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"192.168.119.2\",\"mask\":\"24\",},\"cmdline\":\"paws.bin\",},\0"
//
//#define RK_JSON_NGINX_QEMU "{,\"blk\":{,\"source\":\"dev\",\"path\":\"/dev/paws\",\"fstype\":\"cd9660\",\"mountpoint\":\"/data\",},\"net\":{,\"if\":\"vioif0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"10.0.120.101\",\"mask\":\"24\",},\"cmdline\":\"nginx.bin\",},\0"
//
//#define RK_JSON_NGINX_HW "{,\"blk\":{,\"source\":\"dev\",\"path\":\"/dev/paws\",\"fstype\":\"cd9660\",\"mountpoint\":\"/data\",},\"net\":{,\"if\":\"wm0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"10.0.120.101\",\"mask\":\"24\",},\"cmdline\":\"nginx.bin\",},\0"

/*
 * Application names!!!!
 * I got multiple apps working, booting on top of rumpkernel.. what that means..
 * 1. only a maximum of 8 "main" stubs can be baked in.. Idea is, we'd bake as many stubs depending on the number of applications that require communication with RK..!
 * 2. When rk runs, it parses the "cmdline" and arg[0] is a "." separated list of app names..
 *    this allows, rumpcalls to dynamically identify the apps to match "application" components without hardcoding it..
 */

#define RK_JSON_UDPSTUB_HTTP_QEMU "{,\"net\":{,\"if\":\"vioif0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"10.0.120.101\",\"mask\":\"24\",},\"cmdline\":\"udpstub.http\",},\0"
#define RK_JSON_HTTP_UDPSERV_QEMU "{,\"net\":{,\"if\":\"vioif0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"10.0.120.101\",\"mask\":\"24\",},\"cmdline\":\"http.udpserv\",},\0"
#define RK_JSON_HTTP_QEMU "{,\"net\":{,\"if\":\"vioif0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"10.0.120.101\",\"mask\":\"24\",},\"cmdline\":\"http\",},\0"
#define RK_JSON_UDPSERV_QEMU "{,\"net\":{,\"if\":\"vioif0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"10.0.120.101\",\"mask\":\"24\",},\"cmdline\":\"udpserv\",},\0"
#define RK_JSON_HTTP_UDPSERV_IPERF_QEMU "{,\"net\":{,\"if\":\"vioif0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"10.0.120.101\",\"mask\":\"24\",},\"cmdline\":\"http.udpserv.iperf\",},\0"
#define RK_JSON_I42STUB_HTTP_QEMU "{,\"net\":{,\"if\":\"vioif0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"10.0.120.101\",\"mask\":\"24\",},\"cmdline\":\"i42stub.http\",},\0"

#define RK_JSON_HTTP_UDPSERV_HW "{,\"net\":{,\"if\":\"vioif0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"192.168.0.1\",\"mask\":\"24\",},\"cmdline\":\"http.udpserv\",},\0"
#define RK_JSON_HTTP_HW "{,\"net\":{,\"if\":\"vioif0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"192.168.0.1\",\"mask\":\"24\",},\"cmdline\":\"http\",},\0"
#define RK_JSON_UDPSERV_HW "{,\"net\":{,\"if\":\"vioif0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"192.168.0.1\",\"mask\":\"24\",},\"cmdline\":\"udpserv\",},\0"
#define RK_JSON_HTTP_UDPSERV_IPERF_HW "{,\"net\":{,\"if\":\"vioif0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"192.168.0.1\",\"mask\":\"24\",},\"cmdline\":\"http.udpserv.iperf\",},\0"

#endif /* RK_JSON_CFG_H */
