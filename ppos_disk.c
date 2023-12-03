#include "ppos_disk.h"
#include "ppos-core-globals.h"
#include "disk.h"
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>

struct sigaction usrsig;
disk_t* disk;


void handler(int signum){
    task_resume(disk->ready_task_queue->task);
}

void fcsc_body(void *arg){

    while(sleepQueue != NULL || readyQueue != NULL){
        waiting *t = disk->task_queue;
        if(disk->task_queue != NULL) disk->task_queue = disk->task_queue->next;
        
        if(t != NULL){
            task_resume(t->task);
        }
        else{
            taskExec->state = PPOS_TASK_STATE_SUSPENDED;
            task_suspend(taskExec, &sleepQueue);
            task_yield();
        }
    }


}

int disk_mgr_init (int *numBlocks, int *blockSize){
    disk = malloc(sizeof(disk_t));
    disk->task_queue = NULL;

    usrsig.sa_handler = handler;
    sigemptyset(&usrsig.sa_mask);
    usrsig.sa_flags = 0;

    if (sigaction (SIGUSR1, &usrsig, 0) < 0){
      perror ("Erro em sigaction: ") ;
      exit (1);
    }

    if(disk_cmd(DISK_CMD_INIT, 0, 0)){
    perror("Erro em criacao de disco");
    exit(1);
    }

    printf("\n%d\n", task_create(&disk->scheduler, fcsc_body, ""));

    return 0;
}

int disk_block_read(int block, void* buffer){
    waiting *t = malloc(sizeof(waiting));

    t->operation = DISK_CMD_READ;
    t->block = block;
    t->buffer = buffer;
    t->next = NULL;
    t->task = taskExec;
    t->task->state = PPOS_TASK_STATE_SUSPENDED;

    waiting *aux = disk->task_queue;
    if(disk->task_queue == NULL) disk->task_queue = t;
    else{
        while(aux->next != NULL){
        aux = aux->next;   
        }
        aux->next = t;
    }
    
    task_suspend(t->task, &sleepQueue);
    task_yield();
    

    
    disk->ready_task_queue = t;
    
    disk_cmd(t->operation, t->block, t->buffer);
    disk->ready_task_queue->task->state = PPOS_TASK_STATE_SUSPENDED;
    task_suspend(disk->ready_task_queue->task, &sleepQueue);
    task_yield();
    

    while (disk_cmd(DISK_CMD_STATUS, 0, 0) != DISK_STATUS_IDLE)
    {
    }

    return 0;
}

int disk_block_write(int block, void* buffer){
     waiting *t = malloc(sizeof(waiting));

    t->operation = DISK_CMD_WRITE;
    t->block = block;
    t->buffer = buffer;
    t->next = NULL;
    t->task = taskExec;
    t->task->state = PPOS_TASK_STATE_SUSPENDED;

    waiting *aux = disk->task_queue;
    if(disk->task_queue == NULL) disk->task_queue = t;
    else{
        while(aux->next != NULL){
        aux = aux->next;   
        }
        aux->next = t;
    }
    
    task_suspend(t->task, &sleepQueue);
    task_yield();
    

    
    disk->ready_task_queue = t;
    
    disk_cmd(t->operation, t->block, t->buffer);
    disk->ready_task_queue->task->state = PPOS_TASK_STATE_SUSPENDED;
    task_suspend(disk->ready_task_queue->task, &sleepQueue);
    task_yield();
    

    while (disk_cmd(DISK_CMD_STATUS, 0, 0) != DISK_STATUS_IDLE)
    {
    }

    return 0;
}