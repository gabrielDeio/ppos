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
    //printf("caiu no handler\n");
    //sem_down(disk->handle_sem);
    disk->task_queue->status = 1;
    disk->scheduler.state = PPOS_TASK_STATE_READY; 
    task_resume(&disk->scheduler);
}

disk_tasks* create_task(task_t *current_task ,int operation, int block, void *buffer){
    disk_tasks *task = malloc(sizeof(disk_tasks));

    task->operation = operation;
    task->block = block;
    task->buffer = buffer;
    task->next = NULL;
    task->task = current_task;
    task->status = 0;

    return task;
}

void disk_tasks_append(disk_tasks *task){
    if(disk->task_queue == NULL){
        //printf("\nEmpty Queue\n");
        disk->task_queue = task;
        return;
    }

    disk_tasks *aux = disk->task_queue;
    while(aux->next != NULL){
        aux = aux->next;
    }

    aux->next = task;
}

disk_tasks* disk_tasks_remove(){
    if(disk->task_queue == NULL){
        printf("\nEmpty Queue\n");
        return NULL;
    }

    disk_tasks *aux = disk->task_queue;
    disk->task_queue = aux->next;
    aux->next = NULL;

    return aux;
}


void fcsc_body(){
    while(sleepQueue != NULL || readyQueue != NULL){
        sem_down(disk->queue_sem);

        if(disk->task_queue->status != 0) {
            disk_tasks *task_to_finish = disk_tasks_remove();
            task_to_finish->task->state = PPOS_TASK_STATE_READY;
            //sem_up(disk->handle_sem);
            task_resume(task_to_finish->task);
        }

        if(disk_cmd(DISK_CMD_STATUS, 0, 0) == DISK_STATUS_IDLE && disk->task_queue != NULL){
            //printf("caiu no comando : %d\n", disk_cmd(DISK_CMD_STATUS, 0, 0));
            disk_cmd(disk->task_queue->operation, disk->task_queue->block, disk->task_queue->buffer);
             while (disk_cmd(DISK_CMD_STATUS, 0, 0) != DISK_STATUS_IDLE)
                {
                }
        }

        sem_up(disk->queue_sem);
        disk->scheduler.state = PPOS_TASK_STATE_SUSPENDED;
        task_suspend(&disk->scheduler, &sleepQueue);
        task_yield();
    }
}



int disk_mgr_init (int *numBlocks, int *blockSize){
    disk = malloc(sizeof(disk_t));
    
    disk->task_queue = NULL;
    
    disk->queue_sem = malloc(sizeof(semaphore_t));
    //disk->handle_sem = malloc(sizeof(semaphore_t));
    //disk->scheduler = malloc(sizeof(task_t));
    
    //disk->handle_count = 0;

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

    

    if(sem_create(disk->queue_sem, 1)){
        perror("Erro em criacao de semaforo da fila");
        exit(1);
    }

    

    //if(sem_create(disk->handle_sem, 1)){
        //perror("Error em criacao de semaforo do handle");
        //exit(1);
    //}



    task_create(&disk->scheduler, fcsc_body, "");

    return 0;
}


int disk_block_read(int block, void* buffer){
    sem_down(disk->queue_sem);

    disk_tasks *task = create_task(taskExec, DISK_CMD_READ, block, buffer);

    disk_tasks_append(task);

    if(disk->scheduler.state == PPOS_TASK_STATE_SUSPENDED){
        disk->scheduler.state = PPOS_TASK_STATE_READY;
        task_resume(&disk->scheduler);
    }

    sem_up(disk->queue_sem);
    
    task->status = PPOS_TASK_STATE_SUSPENDED;
    task_suspend(task->task, &sleepQueue);
    task_yield();

    return 0;
}

int disk_block_write(int block, void* buffer){
    sem_down(disk->queue_sem);

    disk_tasks *task = create_task(taskExec, DISK_CMD_WRITE, block, buffer);

    disk_tasks_append(task);

    if(disk->scheduler.state == PPOS_TASK_STATE_SUSPENDED){
        disk->scheduler.state = PPOS_TASK_STATE_READY;
        task_resume(&disk->scheduler);
    }

    sem_up(disk->queue_sem);
    
    task->status = PPOS_TASK_STATE_SUSPENDED;
    task_suspend(task->task, &sleepQueue);
    task_yield();

    return 0;
}