#include "ppos_disk.h"
#include "ppos-core-globals.h"
#include "disk.h"
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>

struct sigaction usrsig;
int deslocatedBlocks = 0;
disk_t* disk;


void handler(int signum){
    task_resume(disk->ready_task_queue->task);
}


void append_disk_task(disk_task *t){
    disk_task *aux = disk->task_queue;
     if(disk->task_queue == NULL) {
        disk->task_queue = t;
        sem_up(disk->queue_sem);
        //mutex_unlock(disk->queue_mutex);
    }
    else{
        while(aux->next != NULL){
        aux = aux->next;   
        }
        aux->next = t;
        //t->prev = aux;
        sem_up(disk->queue_sem);
        //mutex_unlock(disk->queue_mutex);
    }
}

void remove_from_disk_queue(disk_task *t){
    disk_task *aux = disk->task_queue;
    if(aux == NULL) return;
    while(aux->next != t){
        aux = aux->next;
    }
    if(aux->next->next == NULL) {
        aux->next = NULL;
        return;    
    }

    aux->next = aux->next->next;
    return;

}


void fcsc_body(void *arg){
    clock_t start = clock();

    while(sleepQueue != NULL || readyQueue != NULL){
        if(disk->task_queue != NULL) {
            disk_task *t = disk->task_queue;
            disk->task_queue = disk->task_queue->next;
            //disk->task_queue->prev = NULL;
            deslocatedBlocks += disk->currentBlock > t->block ? (disk->currentBlock - t->block) : (t->block - disk->currentBlock);             
            disk->currentBlock = t->block;
            
            task_resume(t->task);
        }
            taskExec->state = PPOS_TASK_STATE_SUSPENDED;
            task_suspend(taskExec, &sleepQueue);
            task_yield();

    }

    clock_t endTime = clock();

    printf("total blocks deslocated: %d\n", deslocatedBlocks);
    printf("total time : %.f\n", (double)endTime - start);

}

void sstf_body(void *arg){
    clock_t start = clock();
    while(sleepQueue != NULL || readyQueue != NULL){
        if(disk->task_queue != NULL){
            disk_task *t = disk->task_queue;
            disk_task *shortest = disk->task_queue;
            int currentDistance = disk->currentBlock > t->block? disk->currentBlock - t->block : t->block - disk->currentBlock;

            while(t->next != NULL){
                int candidateResult = disk->currentBlock > t->block? disk->currentBlock - t->block : t->block - disk->currentBlock;
                if(currentDistance > candidateResult){
                    shortest = t;
                    currentDistance = candidateResult;
                }

                t = t->next;
            }

            t = disk->task_queue;
            if(t == shortest){
                disk->task_queue = NULL;
            }
            else{
                while(t->next != shortest) t = t->next;
                t->next = t->next->next;
            }

            deslocatedBlocks += disk->currentBlock > shortest->block ? (disk->currentBlock - shortest->block) : (shortest->block - disk->currentBlock);             
            disk->currentBlock = shortest->block;
            task_resume(shortest->task);
            
        }
        taskExec->state = PPOS_TASK_STATE_SUSPENDED;
        task_suspend(taskExec, &sleepQueue);
        task_yield();
        
    }
    clock_t endTime = clock();
    printf("total blocks deslocated: %d\n", deslocatedBlocks);
    printf("total time : %.f\n", (double)endTime - start);
}

    void cscan_body(void *arg){
    clock_t start = clock();
    while(sleepQueue != NULL || readyQueue != NULL){
        if(disk->task_queue != NULL){
            disk_task *t = disk->task_queue;
            disk_task *candidateResult = t;
            int currentDistance  = (disk_cmd(DISK_CMD_DISKSIZE, 0, 0) - 1);

            if(t->next == NULL){
                disk->task_queue = NULL;
            }
            else{   
                while(t->next != NULL){
                    if(t->block > disk->currentBlock && currentDistance < (t->block - disk->currentBlock)){
                        currentDistance = t->block - disk->currentBlock;
                        candidateResult = t;
                        
                    }
                    t = t->next;
                }
                remove_from_disk_queue(candidateResult);
            }

            task_resume(candidateResult->task);
            deslocatedBlocks += disk->currentBlock > candidateResult->block ? (disk->currentBlock - candidateResult->block) : (candidateResult->block - disk->currentBlock);
            disk->currentBlock = candidateResult->block;
            if(disk->currentBlock == (disk_cmd(DISK_CMD_DISKSIZE, 0, 0) - 1)) disk->currentBlock = 0;
        }
        taskExec->state = PPOS_TASK_STATE_SUSPENDED;
        task_suspend(taskExec, &sleepQueue);
        task_yield();
    }

    clock_t endTime = clock();
    printf("total blocks deslocated: %d\n", deslocatedBlocks);
    printf("total time : %.f\n", (double)endTime - start);
}



int disk_mgr_init (int *numBlocks, int *blockSize){
    disk = malloc(sizeof(disk_t));
    disk->task_queue = NULL;
    disk->queue_sem = malloc(sizeof(semaphore_t));
    disk->disk_sem = malloc(sizeof(semaphore_t));
    disk->currentBlock = 0;

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
    

    
    
    printf("\n%d\n", task_create(&disk->scheduler, cscan_body, ""));

    return 0;
}

int disk_block_read(int block, void* buffer){
    //mutex_lock(disk->queue_mutex);
    sem_down(disk->queue_sem);

    disk_task *t = malloc(sizeof(disk_task));

    t->operation = DISK_CMD_READ;
    t->block = block;
    t->buffer = buffer;
    t->next = NULL;
    t->task = taskExec;
    t->task->state = PPOS_TASK_STATE_SUSPENDED;

    append_disk_task(t);
    
    task_suspend(t->task, &sleepQueue);
    task_yield();
    

    //mutex_lock(disk->disk_mutex);
    sem_down(disk->disk_sem);
    disk->ready_task_queue = t;
    
    disk_cmd(t->operation, t->block, t->buffer);
    disk->ready_task_queue->task->state = PPOS_TASK_STATE_SUSPENDED;
    task_suspend(disk->ready_task_queue->task, &sleepQueue);
    task_yield();
    

    while (disk_cmd(DISK_CMD_STATUS, 0, 0) != DISK_STATUS_IDLE)
    {
    }
    sem_up(disk->disk_sem);
    //mutex_unlock(disk->disk_mutex);

    return 0;
}

int disk_block_write(int block, void* buffer){
    //mutex_lock(disk->queue_mutex);
    sem_down(disk->queue_sem);
    disk_task *t = malloc(sizeof(disk_task));

    t->operation = DISK_CMD_WRITE;
    t->block = block;
    t->buffer = buffer;
    t->next = NULL;
    t->task = taskExec;
    t->task->state = PPOS_TASK_STATE_SUSPENDED;

    append_disk_task(t);
    
    task_suspend(t->task, &sleepQueue);
    task_yield();
    

    //mutex_lock(disk->disk_mutex);
    sem_down(disk->disk_sem);
    disk->ready_task_queue = t;
    
    disk_cmd(t->operation, t->block, t->buffer);
    disk->ready_task_queue->task->state = PPOS_TASK_STATE_SUSPENDED;
    task_suspend(disk->ready_task_queue->task, &sleepQueue);
    task_yield();
    

    while (disk_cmd(DISK_CMD_STATUS, 0, 0) != DISK_STATUS_IDLE)
    {
    }
    sem_up(disk->disk_sem);
    //mutex_unlock(disk->disk_mutex);

    return 0;
}