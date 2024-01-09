#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>

#include "common/constants.h"
#include "common/io.h"
#include "operations.h"

typedef struct sessions sessions_t;

// estrutura do buffer produtor-consumidor
typedef struct {
    sessions_t* buffer;
    int size;
    int front;
    int rear;
    pthsafe_read_mutex_t mutex;
    pthsafe_read_cond_t not_empty;
    pthsafe_read_cond_t not_full;
} request_buffer_t; 

typedef struct sessions {
  pthsafe_read_t thsafe_read;
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

request_buffer_t request_buffer;

void* thsafe_read_workplace(void* arg) {
  sessions_t* session = (sessions_t*)arg;
  int operation_code;
  int fd;

  unsigned int event_id;
  size_t num_rows, num_cols, num_seats;
  int resp;
  
  int active;

  while(1) {
  pthsafe_read_mutex_lock(&request_buffer.mutex);
  pthsafe_read_cond_wait(&request_buffer.not_empty, &request_buffer.mutex);

  session->req_pipe_path = request_buffer.buffer[request_buffer.front].req_pipe_path;
  session->resp_pipe_path = request_buffer.buffer[request_buffer.front].resp_pipe_path;
  request_buffer.front = (request_buffer.front + 1) % S;
  request_buffer.size--;
  pthsafe_read_cond_signal(&request_buffer.not_full);
  pthsafe_read_mutex_unlock(&request_buffer.mutex);

  printf("session_id: %d\n", session->session_id);
  active = 1;
  
  fd = open(session->resp_pipe_path, O_WRONLY);
  safe_open(fd);
  safe_write(fd, &session->session_id, sizeof(int));
  close(fd);

  while(active) {
  fd = open(session->req_pipe_path, O_RDONLY);
  safe_open(fd);
    if(safe_read(fd, &operation_code, sizeof(int)) > 0) {
      printf("operation_code: %d\n", operation_code);
      switch(operation_code) {
        case 2: // ems_quit
          close(fd);

          fd = open(session->resp_pipe_path, O_WRONLY);
          safe_open(fd);
        
          resp = 0;
          safe_write(fd, &resp, sizeof(int));

          unlink(session->req_pipe_path);
          unlink(session->resp_pipe_path);
          remove(session->req_pipe_path);
          remove(session->resp_pipe_path);
          session->req_pipe_path = NULL;
          session->resp_pipe_path = NULL;
          active = 0;
          printf("quit!\n");
          break;

        case 3: // ems_create
          
            // ler pipe
            safe_read(fd, &event_id, sizeof(unsigned int));
            safe_read(fd, &num_rows, sizeof(size_t));
            safe_read(fd, &num_cols, sizeof(size_t));
            close(fd);

            // pipe de resposta
            fd = open(session->resp_pipe_path, O_WRONLY | O_TRUNC);
            safe_open(fd);
            resp = ems_create(event_id, num_rows, num_cols);
            safe_write(fd, &resp, sizeof(int));
            break;

        case 4: // ems_reserve

          // ler pipe
          safe_read(fd, &event_id, sizeof(unsigned int));
          safe_read(fd, &num_seats, sizeof(size_t));

          // alocar arrays
          size_t* xs = malloc(num_seats * sizeof(size_t));
          size_t* ys = malloc(num_seats * sizeof(size_t));

          // ler xs e ys
          safe_read(fd, xs, num_seats * sizeof(size_t));
          safe_read(fd, ys, num_seats * sizeof(size_t));

          close(fd);

          fd = open(session->resp_pipe_path, O_WRONLY | O_TRUNC);
          safe_open(fd);

          // pipe de resposta
          resp = ems_reserve(event_id, num_seats, xs, ys);

          safe_write(fd, &resp, sizeof(int));

          free(xs);
          free(ys);
          break;
      
        case 5: // ems_show
          // ler pipe
          safe_read(fd, &event_id, sizeof(unsigned int));
          close(fd);

          fd = open(session->resp_pipe_path, O_WRONLY);
          safe_open(fd);
          
          // pipe de resposta dentro do ems_show
          ems_show(fd, event_id);
          break;

        case 6: // ems_list_events
          close(fd);

          fd = open(session->resp_pipe_path, O_WRONLY);
          safe_open(fd);

          // pipe de resposta dentro do ems_list_events
          ems_list_events(fd);
          break;
    }
    
    //TODO: safe_write new client to the producer-consumer buffer
    }
    close(fd);
  }
  }
  return NULL;
}


int process_client_requests(char* server_pipe) {
  // int active_sessions = 0;
  int operation_code;
  int fd = open(server_pipe, O_RDONLY | O_NONBLOCK);
  safe_open(fd);

    //TODO: safe_read from each client pipe
    // (different functions, the client one will be a loop where the thsafe_read never exits,
    // and the main one will be a loop where the thsafe_read exits when the server pipe is closed,
    // always checking for new login requests)
  while (1) {
    if(safe_read(fd, &operation_code, sizeof(int)) != 0) {
      printf("operation_code: %d\n", operation_code);
    
    // safe_read from pipe
    switch(operation_code) { // login
      case 1:
        char a[40];
        char b[40];
        memset(a, '\0', 40);
        memset(b, '\0', 40);

        safe_read(fd, a, 40);
        safe_read(fd, b, 40);

      // if applicable, wait until session can be created
        pthsafe_read_mutex_lock(&request_buffer.mutex);
        if(request_buffer.size == S)
          pthsafe_read_cond_wait(&request_buffer.not_full, &request_buffer.mutex);
        sessions_t* new_session = malloc(sizeof(sessions_t));
        new_session->req_pipe_path = a;
        new_session->resp_pipe_path = b;
        //new_session->request_buffer = &request_buffer;

        request_buffer.buffer[request_buffer.rear] = *new_session;
        request_buffer.rear = (request_buffer.rear + 1) % S;
        request_buffer.size++;

        pthsafe_read_cond_signal(&request_buffer.not_empty);
        pthsafe_read_mutex_unlock(&request_buffer.mutex); 
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
  pthsafe_read_mutex_init(&request_buffer.mutex, NULL);
  pthsafe_read_cond_init(&request_buffer.not_empty, NULL);
  pthsafe_read_cond_init(&request_buffer.not_full, NULL);

  mkfifo(argv[1], 0640);
  server_pipe_path = argv[1];

  for(int i = 0; i < S; i++) {
    sessions[i].session_id = i;
    sessions[i].req_pipe_path = NULL;
    sessions[i].resp_pipe_path = NULL;
    sessions[i].request_buffer = &request_buffer;
    pthsafe_read_create(&sessions[i].thsafe_read, NULL, thsafe_read_workplace, (void*)&sessions[i]);
  }

  process_client_requests(argv[1]);

  /*
  // Criação de thsafe_reads trabalhadoras
  pthsafe_read_t workerThsafe_reads[S];
  */



  /*
  // espera que as thsafe_reads trabalhadoras terminem
  for (int i = 0; i < S; ++i) {
      pthsafe_read_join(workerThsafe_reads[i], NULL);
  }
  */

  // Close Server
  unlink(argv[1]);
  remove(argv[1]);

  ems_terminate();
  return 0;
}