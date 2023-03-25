#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    
    // retrieving thread_data
    struct thread_data* threadfunc_args = (struct thread_data *) thread_param;
    
    // Setting status before mutex activation
    threadfunc_args-> thread_complete_success = false;

    // wait to lock mutex
    int rc = usleep(threadfunc_args->wait_to_start_mutex); 
    if( rc != 0){
        ERROR_LOG("usleep: Wait for mutex lock failed");
        return thread_param;
    }

    // Lock mutex
    rc = pthread_mutex_lock(threadfunc_args->mutex);
    if( rc != 0){
	ERROR_LOG("pthread_mutex_lock: Mutex lock failed");    
        return thread_param;
    }

    // Wait to release mutex
    rc = usleep(threadfunc_args->wait_to_release_mutex);
    if( rc != 0){
	ERROR_LOG("usleep: Wait for mutex release failed");
        return thread_param;
    }

    // release mutex
    rc = pthread_mutex_unlock(threadfunc_args->mutex);
    if( rc != 0){
	ERROR_LOG("pthread_mutex_unlock: Mutex release failed");
        printf("mutex release failed!!!!!\n"); 
        return thread_param;
    }

    DEBUG_LOG("Thread completed successfully");
    threadfunc_args-> thread_complete_success = true;	
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
     /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     * 
     * See implementation details in threading.h file comment block
     */
 

    // allocate memory for thread_data
    struct thread_data *data = (struct thread_data*)malloc(sizeof(struct thread_data));
    DEBUG_LOG("thread_data memory allocation complete");
    
    //set-up mutex and wait arguments
    data->mutex = mutex;
    data->wait_to_start_mutex = wait_to_obtain_ms;
    data->wait_to_release_mutex = wait_to_release_ms;
    DEBUG_LOG("thread_data updated");
    
    // pass thread_data to create thread using threadfunc()
    int rc = pthread_create(thread, NULL, threadfunc, data);     

    if(rc !=0 ){
        ERROR_LOG("pthread_create: Thread creation failed");
	return false;
    }

    return true;
}

