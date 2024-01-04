#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>

#include "common/constants.h"
#include "common/io.h"
#include "operations.h"

#define S 3 //FIXME

typedef struct sessions sessions_t;

// estrutura do buffer produtor-consumidor
typedef struct {
    sessions_t* buffer;
    int size;
    int front;
    int rear;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} request_buffer_t; 

typedef struct sessions {
  pthread_t thread;
  int session_id;
  char* req_pipe_path;
  char* resp_pipe_path;
  request_buffer_t* request_buffer;
} sessions_t;

char* server_pipe_path;
int new_session_id = 0;
//session id tem de ser novo para todos os futuros clientes ou
// pode servir como index para o array de sessions?
sessions_t* sessions;

int signal_received = 0;


request_buffer_t request_buffer;

void signal_handler(int signal) {
  if(signal == SIGUSR1)
    signal_received = 1;
  }

void* thread_workplace(void* thread_id) {
  int session_id = *(int*)thread_id;
  char operation_code;
  int fd;

  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGUSR1);
  pthread_sigmask(SIG_BLOCK, &mask, NULL);

  char req_pipe_path[40];
  char resp_pipe_path[40];

  unsigned int event_id;
  size_t num_rows, num_cols, num_seats;
  int resp;
  
  int active;

  while(1) {
  pthread_mutex_lock(&request_buffer.mutex);
  printf("got in: %d\n", session_id);
  while(request_buffer.size == 0) pthread_cond_wait(&request_buffer.not_empty, &request_buffer.mutex);
  printf("got out: %d\n", session_id);
  //session = &request_buffer.buffer[request_buffer.front];
  //FIXME
  strcpy(req_pipe_path, request_buffer.buffer[request_buffer.front].req_pipe_path);
  strcpy(resp_pipe_path, request_buffer.buffer[request_buffer.front].resp_pipe_path);
  request_buffer.front++;
  if(request_buffer.front == S) request_buffer.front = 0;
  request_buffer.size--;


  printf("session_id: %d\n", session_id);
  active = 1;
  
  fd = open(resp_pipe_path, O_WRONLY | O_TRUNC);
  if(fd == -1) {
    fprintf(stderr, "[Err]: open failed: %d\n",(errno));
    exit(EXIT_FAILURE);
  }
  write(fd, &session_id, sizeof(int));
  close(fd);

  pthread_cond_signal(&request_buffer.not_full);
  pthread_mutex_unlock(&request_buffer.mutex);

  while(active) {
  fd = open(req_pipe_path, O_RDONLY);
    if(read(fd, &operation_code, sizeof(char)) > 0) {
      puts("operation_code: ");
      printf("%d\n", session_id);
      switch(operation_code) {
        case '2': // ems_quit
        // não suporta mais que uma sessão
          close(fd);
        
          // FIXME atenção a isto, ver enunciado (retorno do quit)

          unlink(req_pipe_path);
          unlink(resp_pipe_path);
          active = 0;
          puts("quit!\n");
          break;

        case '3': // ems_create
          
            // ler pipe
            read(fd, &event_id, sizeof(unsigned int));
            read(fd, &num_rows, sizeof(size_t));
            read(fd, &num_cols, sizeof(size_t));
            close(fd);

            // pipe de resposta
            fd = open(resp_pipe_path, O_WRONLY | O_TRUNC);
            resp = ems_create(event_id, num_rows, num_cols);
            write(fd, &resp, sizeof(int));
            break;

        case '4': // ems_reserve

          // ler pipe
          read(fd, &event_id, sizeof(unsigned int));
          read(fd, &num_seats, sizeof(size_t));

          // alocar arrays
          size_t* xs = malloc(num_seats * sizeof(size_t));
          size_t* ys = malloc(num_seats * sizeof(size_t));

          // ler xs e ys
          read(fd, xs, num_seats * sizeof(size_t));
          read(fd, ys, num_seats * sizeof(size_t));

          close(fd);

          fd = open(resp_pipe_path, O_WRONLY | O_TRUNC);

          // pipe de resposta
          resp = ems_reserve(event_id, num_seats, xs, ys);

          write(fd, &resp, sizeof(int));

          free(xs);
          free(ys);
          break;
      
        case '5': // ems_show
          // ler pipe
          read(fd, &event_id, sizeof(unsigned int));
          close(fd);

          fd = open(resp_pipe_path, O_WRONLY | O_TRUNC);
          
          // pipe de resposta dentro do ems_show
          ems_show(fd, event_id);
          break;

        case '6': // ems_list_events
          close(fd);

          fd = open(resp_pipe_path, O_WRONLY | O_TRUNC);

          // pipe de resposta dentro do ems_list_events
          ems_list_events(fd);
          break;
    }
    
    //TODO: Write new client to the producer-consumer buffer
    }
    close(fd);
  }
  }
  return NULL;
}


int process_client_requests(char* server_pipe) {
  // int active_sessions = 0;
  char operation_code;
  int fd;
  fd = open(server_pipe, O_RDONLY);

  signal(SIGUSR1, signal_handler);

  while (1) {
    //TODO: Read from each client pipe
    // (different functions, the client one will be a loop where the thread never exits,
    // and the main one will be a loop where the thread exits when the server pipe is closed,
    // always checking for new login requests)

    if(signal_received) {
      status_signal();
      signal_received = 0;
    }

    
    if(read(fd, &operation_code, sizeof(char)) > 0) {
    // Read from pipe
    switch(operation_code) { // login
      case '1':
        char a[40];
        char b[40];
        memset(a, '\0', 40);
        memset(b, '\0', 40);

        read(fd, a, 40);
        read(fd, b, 40);
        puts("waiting!\n");
        printf("a: %s\n", a);
        printf("b: %s\n", b);


      // if applicable, wait until session can be created
        pthread_mutex_lock(&request_buffer.mutex);
        while(request_buffer.size == S) {
          puts("waiting for buffer to be empty!\n");
          pthread_cond_wait(&request_buffer.not_full, &request_buffer.mutex);
        }
        sessions_t* new_session = malloc(sizeof(sessions_t));
        new_session->req_pipe_path = a;
        new_session->resp_pipe_path = b;
        request_buffer.buffer[request_buffer.rear] = *new_session;
        request_buffer.rear++;
        if(request_buffer.rear == S) request_buffer.rear = 0;
        request_buffer.size++;

        pthread_cond_signal(&request_buffer.not_empty);
        pthread_mutex_unlock(&request_buffer.mutex); 
      } 
    }
  }
  return 0;
}

int main(int argc, char* argv[]) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "Usage: %s\n <pipe_path> [delay]\n", argv[0]);
    return 1;
  }

  char* endptr;
  unsigned int state_access_delay_us = STATE_ACCESS_DELAY_US;
  if (argc == 3) {
    unsigned long int delay = strtoul(argv[2], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_us = (unsigned int)delay;
  }

  if (ems_init(state_access_delay_us)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  sessions = malloc(S * sizeof(sessions_t));
  request_buffer.buffer = malloc(S * sizeof(sessions_t));
  request_buffer.size = 0;
  request_buffer.front = 0;
  request_buffer.rear = 0;
  pthread_mutex_init(&request_buffer.mutex, NULL);
  pthread_cond_init(&request_buffer.not_empty, NULL);
  pthread_cond_init(&request_buffer.not_full, NULL);

  mkfifo(argv[1], 0640);
  server_pipe_path = argv[1];

  for(int i = 0; i < S; i++) {
    int* session_id = malloc(sizeof(int));
    *session_id = i;
    pthread_create(&sessions[i].thread, NULL, thread_workplace, (void*)session_id);
  }

  process_client_requests(argv[1]);

  /*
  // Criação de threads trabalhadoras
  pthread_t workerThreads[S];
  */



  /*
  // espera que as threads trabalhadoras terminem
  for (int i = 0; i < S; ++i) {
      pthread_join(workerThreads[i], NULL);
  }
  */

  // Close Server
  unlink(argv[1]);
  remove(argv[1]);

  ems_terminate();
  return 0;
}