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

  int active_sessions = 0;
  sessions = malloc(S * sizeof(sessions_t));

  mkfifo(argv[1], 0640);

  //TODO: Intialize server, create worker threads FIXME

  while (1) {
    //TODO: Read from server pipe
    //TODO: Read from each client pipe
    // (different functions, the client one will be a loop where the thread never exits,
    // and the main one will be a loop where the thread exits when the server pipe is closed,
    // always checking for new login requests)
    int operation_code;

    unsigned int event_id;
    size_t num_rows, num_cols, num_seats;


    int fd = open(argv[1], O_RDONLY);
    int resp;
    read(fd, &operation_code, sizeof(int));
    
    //TODO: Read from pipe
    switch(operation_code) {
      case 1:
        char a[40];
        char b[40];
        memset(a, '\0', 40);
        memset(b, '\0', 40);

        read(fd, a, 40);
        read(fd, b, 40);

      //if applicable, wait until session can be created
        mkfifo(a, 0640);
        mkfifo(b, 0640);

        sessions[new_session_id].session_id = new_session_id;
        sessions[new_session_id].req_pipe_path = a;
        sessions[new_session_id].resp_pipe_path = b;

        active_sessions++;
        new_session_id++;
        break;
      
      case 2: // ems_quit
        //não suporta mais que uma sessão
        unlink(sessions[0].req_pipe_path);
        unlink(sessions[0].resp_pipe_path);
        active_sessions--;
        sessions[0].session_id = -1; //FIXME, FORMA DE DISTINGUIR ATIVIDADE
        sessions[0].req_pipe_path = NULL;
        sessions[0].resp_pipe_path = NULL;
        break;
    }

    close(fd);
    fd = open(sessions[0].req_pipe_path, O_RDONLY);
    read(fd, &operation_code, sizeof(int));
      switch(operation_code) {
        case 3: // ems_create
          
          // ler os parâmetros do pipe
          read(fd, &event_id, sizeof(unsigned int));
          read(fd, &num_rows, sizeof(size_t));
          read(fd, &num_cols, sizeof(size_t));
          close(fd);

          // pipe de resposta
          fd = open(sessions[0].resp_pipe_path, O_WRONLY);
          resp = ems_create(event_id, num_rows, num_cols);
          write(fd, &resp, sizeof(int));
          close(fd);
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

          fd = open(sessions[0].resp_pipe_path, O_WRONLY);

        // pipe de resposta

          resp = ems_reserve(event_id, num_seats, xs, ys);

          write(fd, &resp, sizeof(int));

          close(fd);
          free(xs);
          free(ys);
          break;
      
      case 5: // ems_show

        read(fd, &event_id, sizeof(unsigned int));
        close(fd);

        fd = open(sessions[0].resp_pipe_path, O_WRONLY);

        //não deve imprimir para aqui!!!
        resp = ems_show(STDOUT_FILENO, event_id);

        close(fd);

        //ver enunciado e dar print de acordo, há outros dados que são precisos escrever,
        //como o int retorno e num_seats etc

        /* // pipe de resposta
        int resp = ems_show(sbvnmkd, event_id);

        write(argv[1], &resp, sizeof(int));

        size_t num_rows, num_cols;
        unsigned int* seats;

        num_rows = ;
        num_cols = ;
        seats = malloc();

        write(argv[1], &num_rows, sizeof(size_t));
        write(argv[1], &num_cols, sizeof(size_t));
        write(argv[1], seats, num_rows * num_cols * sizeof(unsigned int));

        free(seats);
        */
        break;

      case 6: // ems_list_events

        read(fd, &event_id, sizeof(unsigned int));

        close(fd);

        fd = open(sessions[0].resp_pipe_path, O_WRONLY);
        
        //não deve imprimir para aqui!!!
        resp = ems_list_events(STDOUT_FILENO);
        close(fd);

        //ver enunciado e dar print de acordo, há outros dados que são precisos escrever,
        //como o int retorno e num_seats etc
        
        
        /* // pipe de resposta
        int resp = ems_list_events(fghj); 
        
        write(argv[1], &resp, sizeof(int));

        size_t num_events;
        unsigned int* ids;
        
        num_events = ;
        ids = malloc();
        
        write(argv[1], &num_events, sizeof(size_t));
        write(argv[1], ids, num_events * sizeof(unisgned int));

        free(ids);
        */
        break;
    }
    //TODO: Write new client to the producer-consumer buffer
  }

  //TODO: Close Server
  unlink(argv[1]);

  ems_terminate();
}