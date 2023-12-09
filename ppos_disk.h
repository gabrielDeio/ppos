// PingPongOS - PingPong Operating System
// Prof. Carlos A. Maziero, DINF UFPR
// Versão 1.2 -- Julho de 2017

// interface do gerente de disco rígido (block device driver)

#ifndef __DISK_MGR__
#define __DISK_MGR__

// estruturas de dados e rotinas de inicializacao e acesso
// a um dispositivo de entrada/saida orientado a blocos,
// tipicamente um disco rigido.

#include "ppos.h"

/* 
typedef struct disk_tasks {
    task_t *task;
    int block; //Bloco onde deseja realizar a operação
    int operation; //Tipo da operação
    int status; //0 - Significa que a tarefa ainda não foi processada. 1- Significa que tarefa já foi processada
    unsigned char* buffer; //Conteudo que será escrito ou espaço onde o disco irá colocar o dado para ser lido
    struct disk_tasks *next;
} disk_tasks;

// estrutura que representa um disco no sistema operacional
typedef struct {
  semaphore_t *queue_sem;
  //int handle_count;
  semaphore_t * disk_sem;
   task_t scheduler;
   disk_tasks *task_queue;
} disk_t ;
*/



typedef struct waiting {
    task_t *task;
    int block; //Bloco onde deseja realizar a operação
    int operation; //Tipo da operação
    unsigned char* buffer; //Conteudo que será escrito ou espaço onde o disco irá colocar o dado para ser lido
    struct waiting *next;
} waiting;

// estrutura que representa um disco no sistema operacional
typedef struct {
  semaphore_t * disk_sem;
  semaphore_t *queue_sem;
  mutex_t * queue_mutex;
  mutex_t * disk_mutex;
   task_t scheduler;
   waiting *task_queue;
   waiting *ready_task_queue;
} disk_t ;


/*
typedef struct disk_task {
  struct disk_task *prev;
  struct disk_task *next;
  int operation;
  int block;
  void* buffer;
} disk_task;

// estrutura que representa um disco no sistema operacional
typedef struct
{
    semaphore_t* semaphore;
    task_t* task;
} disk_t ;

*/

// inicializacao do gerente de disco
// retorna -1 em erro ou 0 em sucesso
// numBlocks: tamanho do disco, em blocos
// blockSize: tamanho de cada bloco do disco, em bytes
int disk_mgr_init (int *numBlocks, int *blockSize) ;

// leitura de um bloco, do disco para o buffer
int disk_block_read (int block, void *buffer) ;

// escrita de um bloco, do buffer para o disco
int disk_block_write (int block, void *buffer) ;

#endif
