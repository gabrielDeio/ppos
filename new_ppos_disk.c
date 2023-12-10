#include "ppos_disk.h"
#include "ppos-core-globals.h"
#include "disk.h"
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

struct sigaction usrsig;
disk_t* disk;

void handler(int signum){
    disk->scheduler.state = PPOS_TASK_STATE_READY; 
    task_resume(&disk->scheduler);
    disk->status = 1;
    
}

disk_task* create_task(task_t *current_task ,int operation, int block, void *buffer){
    disk_task *task = malloc(sizeof(disk_task));

    task->operation = operation;
    task->block = block;
    task->buffer = buffer;
    task->next = NULL;
    task->task = current_task;
    task->status = 0;

    return task;
}

void disk_task_append(disk_task *task){
    
    if(disk->task_queue == NULL){
        //printf("\nEmpty Queue\n");
        disk->task_queue = task;
        return;
    }

    disk_task *aux = disk->task_queue;
    while(aux->next != NULL){
        aux = aux->next;
    }

    aux->next = task;
    
    return;
}

disk_task* disk_task_remove(){
    if(disk->scheduler->task->queue == NULL){
        printf("\nEmpty Queue\n");
        return NULL;
    }

    disk_task *aux = disk->scheduler->task->queue;
    disk->scheduler->task->queue = aux->next;
    aux->next = NULL;

    return aux;
}


void fcsc_body(){
    while(1){
        printf("passou no body\n");
        sem_down(disk->disk_sem);
        if(disk->status != 0) {
            disk_task *task_to_finish = disk_task_remove();
            //task_to_finish->task->state = PPOS_TASK_STATE_READY;
            task_resume(task_to_finish);
            disk->status = 0;
        }

        if(disk_cmd(DISK_CMD_STATUS, 0, 0) == DISK_STATUS_IDLE && disk->scheduler->task->queue != NULL){
            printf("requisitou\n");
            disk_cmd(disk->scheduler->task->queue->operation, disk->scheduler->task->queue->block, disk->scheduler->task->queue->buffer);
        }
        
        //sem_up(disk->queue_sem);
        disk->scheduler.state = PPOS_TASK_STATE_SUSPENDED;
        task_suspend(&disk->scheduler, &sleepQueue);
        sem_up(disk->disk_sem);
        task_yield();
    }
}



int disk_mgr_init (int *numBlocks, int *blockSize){
    disk = malloc(sizeof(disk_t));
    
    disk->task_queue = NULL;

    
    disk->queue_sem = malloc(sizeof(semaphore_t));
    disk->disk_sem = malloc(sizeof(semaphore_t));
    //disk->handler_sem = malloc(sizeof(semaphore_t));
    disk->status = 0;

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
        perror("Error em criacao de semaforo");
        exit(1);
    }

    
    if(sem_create(disk->disk_sem, 1)){
        perror("Error em criacao de semaforo");
        exit(1);
    }

    printf("\n%d\n", task_create(&disk->scheduler, fcsc_body, ""));

    return 0;
}


int disk_block_read(int block, void* buffer){
    sem_down(disk->queue_sem);
    disk_task *task = create_task(taskExec, DISK_CMD_READ, block, buffer);

    disk_task_append(task);
    
    if(disk->scheduler.state == PPOS_TASK_STATE_SUSPENDED){
        disk->scheduler.state = PPOS_TASK_STATE_READY;
        task_resume(&disk->scheduler);
    }

    task->task->state = PPOS_TASK_STATE_SUSPENDED;
    task_suspend(task->task, disk->scheduler->queue);
    sem_up(disk->queue_sem);
    task_yield();

    return 0;
}

int disk_block_write(int block, void* buffer){
    sem_down(disk->queue_sem);
    disk_task *task = create_task(taskExec, DISK_CMD_WRITE, block, buffer);

    disk_task_append(task);

    if(disk->scheduler.state == PPOS_TASK_STATE_SUSPENDED){
        disk->scheduler.state = PPOS_TASK_STATE_READY;
        task_resume(&disk->scheduler);
    }
    
    task->task->state = PPOS_TASK_STATE_SUSPENDED;
    task_suspend(task->task, disk->scheduler->queue);
    sem_up(disk->queue_sem);
    task_yield();

    return 0;
}