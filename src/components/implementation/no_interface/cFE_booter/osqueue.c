#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cobj_format.h>

#include "cFE_util.h"

typedef unsigned long uint32;//remove wnen cFE header correctly implement uint32

struct queue{

	const char *queue_name;
	uint32 queue_id;  //should MAXNUMOFQUEUES be the max number of unique ids, or 2,494,967,295
	uint32 data_size;
	queue_depth;
	queue* head;  //pointer to head of queue
	queue* tail;  //pointer to last filled elemnt of queue
}

const int MAXNUMOFQUEUES=256;//change this

int numofqueues;

queue queuelist[MAXNUMOFQUEUES];

/**
*	Description: (From NASA's OSAL Library API documentation)
*
* This is the function used to create a queue in the operating system. Depending on the
* underlying operating system, the memory for the queue will be allocated automatically or
* allocated by the code that sets up the queue. Queue names must be unique; if the name
* already exists this function fails. Names cannot be NULL.
**/

int32 OS_QueueCreate( uint32 *queue_id, const char *queue_name, uint32 queue_depth, uint32 data_size, uint32 flags ){
	
	//how do I get a page of memory from composite
	//should I use an entire page for each queue 
	//should I allow a queue to span more than one page in a single mem ring

	if(numofqueues==MAXNUMOFQUEUES)return OS_ERR_NO_FREE_IDS;
	//other error checking

	numofqueues++;
	queue newqueue;
	//somehow get a page(?) of memory to store the memory ring in

}


/**
*	Description: (From NASA's OSAL Library API documentation)
*
* This is the function used to delete a queue in the operating system. This also frees the
* respective queue_id to be used again when another queue is created.
**/

int32 OS_QueueDelete ( uint32 queue_id ){
	
}

/**
*	Description: (From NASA's OSAL Library API documentation)

* This function is used to retrieve a data item from an existing queue. The queue can be
* checked, pended on, or pended on with a timeout.
**/

int32 OS_QueueGet ( uint32 queue_id, void *data, uint32 size, uint32 *size_copied, int32 timeout){
	
}

/**
*	Description: (From NASA's OSAL Library API documentation)

* This function takes a queue name and looks for a valid queue with this name and returns
* the id of that queue.
**/

int32 OS_QueueGetIdByName (uint32 *queue_id, const char *queue_name){

	for(int i=0;i<MAXNUMOFQUEUES;i++){
		if(queuelist[i].name==queue_name){	//what is the behavior of == for char array in c
			return queuelist[i].queue_id;
	}

	return OS_ERR_NAME_NOT_FOUND;  //how to return this?
}


/**
*	Description: (From NASA's OSAL Library API documentation)

* This function takes queue_id, and looks it up in the OS table. It puts all of the
* information known about that queue into a structure pointer to by queue_prop.
**/

int32 OS_QueueGetInfo (uint32 queue_id, OS_queue_prop_t *queue_prop){
	//make a struct somewhere to deal with this.  for now returns the queue struct

	for(int i=0;i<MAXNUMOFQUEUES;i++){
		if(queuelist[i].queue_id==queue_id){	//what is the behavior of == for char array in c
			return queuelist[i];
	}

	return OS_ERR_NAME_NOT_FOUND;  //how to return this?
}


/**
*	Description: (From NASA's OSAL Library API documentation)

* This function is used to send data on an existing queue. The flags can be used to specify
* the behavior of the queue if it is full.
**/

int32 OS_QueuePut ( uint32 queue_id, void *data, uint32 size, uint32 flags){


}


/**
*	Description: 
*
* A temporary method used to test the function of queue related methods
**/

void queue_test(){

	prints("queue test works");
}