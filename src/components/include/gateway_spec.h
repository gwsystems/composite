/* System specific define's for gateway config */

/* Roomba can only take scripts up to 100 chars long */
#define MAX_SCRIPT_SZ 100

/* Coordinate AEP's between components */
#define AEP_PRIO 1
#define DRIVER_PRIO 1
#define BACKUP_DRIVER_AEP_KEY 7
#define DRIVER_AEP_KEY 8
#define ROBOT_CONT_AEP_KEY 18
#define IMAGE_AEP_KEY 3

/* Shmem id's */
#define CAMERA_UDP_SHMEM_ID 0
#define ROBOTCONT_UDP_SHMEM_ID 1

/* Literally arbitrary bullshit */
#define SCRIPT_END 66
#define TASK_DONE 99
#define REQ_JPEG 77
#define RECV_JPEG 79
#define SEND_SCRIPT 80
#define SCRIPT_RECV_ACK 100
#define SEND_SHUTDOWN 81

#define JPG_SZ 152000

#define BACKUP_COMP 8

#define DEMO1 1
#undef DEMO2 
#undef DEMO3 
