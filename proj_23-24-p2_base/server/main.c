#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>

#include "common/constants.h"
#include "common/io.h"
#include "operations.h"

#define S 1

int new_session_id = 0;
//session id tem de ser novo para todos os futuros clientes ou
// pode servir como index para o array de sessions?
sessions_t *sessions;

/*
// estrutura do buffer produtor-consumidor
typedef struct {
    int* buffer;
    int size;
    int front;
    int rear;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} request_buffer_t; 

request_buffer_t request_buffer;
*/

void* process_client_requests(void* arg);

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

  mkfifo(argv[1], 0640);

  //TODO: Intialize server, create worker threads FIXME
  pthread_t hostThread;
  pthread_create(&hostThread, NULL, process_client_requests, (void*)argv[1]);

  process_client_requests(argv[1]);

  /*
  // Criação de threads trabalhadoras
  pthread_t workerThreads[S];
  */

  pthread_join(hostThread, NULL);  // espera que a thread do cliente termine

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

// função para processar os pedidos dos clientes
void* process_client_requests(void* arg) {
  const char* server_pipe = (const char*)arg;
  int active_sessions = 0;
  int operation_code;
  int fd;

  unsigned int event_id;
  size_t num_rows, num_cols, num_seats;
  int resp;

  while (1) {
    //TODO: Read from each client pipe
    // (different functions, the client one will be a loop where the thread never exits,
    // and the main one will be a loop where the thread exits when the server pipe is closed,
    // always checking for new login requests)

    if(active_sessions == 0) {
    fd = open(server_pipe, O_RDONLY);
    if(read(fd, &operation_code, sizeof(int)) > 0) {
      printf("operation_code: %d\n", operation_code);
    
    // Read from pipe
    switch(operation_code) { // login
      case 1:
        char a[40];
        char b[40];
        memset(a, '\0', 40);
        memset(b, '\0', 40);

        read(fd, a, 40);
        read(fd, b, 40);
        close(fd);

      // if applicable, wait until session can be created

        sessions[new_session_id].session_id = new_session_id;
        sessions[new_session_id].req_pipe_path = a;
        sessions[new_session_id].resp_pipe_path = b;

        active_sessions++;
        new_session_id++;

        fd = open(sessions[0].resp_pipe_path, O_WRONLY | O_TRUNC);
        write(fd, &sessions[0].session_id, sizeof(int));
        break;
      } 
    } close(fd); 
    }
    
    fd = open(sessions[0].req_pipe_path, O_RDONLY);
    if(read(fd, &operation_code, sizeof(int)) > 0) {
      printf("operation_code: %d\n", operation_code);
      switch(operation_code) {
        case 2: // ems_quit
        // não suporta mais que uma sessão
          close(fd);

          fd = open(sessions[0].resp_pipe_path, O_WRONLY | O_TRUNC);
        
          resp = 0;
          write(fd, &resp, sizeof(int));
          // FIXME atenção a isto, ver enunciado (retorno do quit)

          unlink(sessions[0].req_pipe_path);
          unlink(sessions[0].resp_pipe_path);
          remove(sessions[0].req_pipe_path);
          remove(sessions[0].resp_pipe_path);
          active_sessions--;
          sessions[0].session_id = -1; //FIXME, FORMA DE DISTINGUIR ATIVIDADE
          sessions[0].req_pipe_path = NULL;
          sessions[0].resp_pipe_path = NULL;
          exit(0);
          break;

        case 3: // ems_create
          
            // ler pipe
            read(fd, &event_id, sizeof(unsigned int));
            read(fd, &num_rows, sizeof(size_t));
            read(fd, &num_cols, sizeof(size_t));
            close(fd);

            // pipe de resposta
            fd = open(sessions[0].resp_pipe_path, O_WRONLY | O_TRUNC);
            resp = ems_create(event_id, num_rows, num_cols);
            write(fd, &resp, sizeof(int));
            break;

        case 4: // ems_reserve

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

          fd = open(sessions[0].resp_pipe_path, O_WRONLY | O_TRUNC);

          // pipe de resposta
          resp = ems_reserve(event_id, num_seats, xs, ys);

          write(fd, &resp, sizeof(int));

          free(xs);
          free(ys);
          break;
      
        case 5: // ems_show
          // ler pipe
          read(fd, &event_id, sizeof(unsigned int));
          close(fd);

          fd = open(sessions[0].resp_pipe_path, O_WRONLY | O_TRUNC);
          
          // pipe de resposta dentro do ems_show
          ems_show(fd, event_id);
          break;

        case 6: // ems_list_events
          close(fd);

          fd = open(sessions[0].resp_pipe_path, O_WRONLY | O_TRUNC);

          // pipe de resposta dentro do ems_list_events
          ems_list_events(fd);
          break;
    }
    
    //TODO: Write new client to the producer-consumer buffer
    } 
    close(fd);
  }
  pthread_exit(NULL);
}