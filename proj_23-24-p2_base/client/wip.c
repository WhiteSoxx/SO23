#include "api.h"
#include "common/constants.h"
#include "common/io.h"

#include <fcntl.h>
#include <unistd.h>

#include <stdio.h> //remover, só para o print de teste
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

char const* own_req_pipe_path;
char const* own_resp_pipe_path;
int own_req_pipe_fd;
int own_resp_pipe_fd;

// Variáveis globais
int session_id;

int wait_for_answer() {
  int answer;
  while(read(own_resp_pipe_fd, &answer, sizeof(int)) == 0)
  return answer;
}

int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {

  //FIXME aqui escreve para o pipe?????? e aguarda o retorno????

  own_req_pipe_path = req_pipe_path;
  own_resp_pipe_path = resp_pipe_path;
  mkfifo(req_pipe_path, 0640);
  mkfifo(resp_pipe_path, 0640);
  own_req_pipe_fd = open(req_pipe_path, O_RDONLY | O_NONBLOCK);
  own_resp_pipe_fd = open(resp_pipe_path, O_WRONLY | O_NONBLOCK);

  int fd = open(server_pipe_path, O_WRONLY);
  int command = 1;
  write(fd, &command, sizeof(int));
  write(fd, own_req_pipe_path, 40* sizeof(char));
  write(fd, own_resp_pipe_path, 40 * sizeof(char));
  close(fd);

  //ESPERAR PELA RESPOSTA DO SERVIDOR ANTES DE CONTINUAR
  session_id = wait_for_answer();
  //if isto falhar, imprime mensagem de erro e lança uma mensagem de login quando der
  //FIXME \n

  //FIXME RETURN???
  return 0;
}

int ems_quit(void) { 
  int command = 2;

  write(own_req_pipe_fd, &command, sizeof(int));

  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  // send create request to the server (through the request pipe) and wait for the response (through the response pipe)
  int command = 3;
  write(own_req_pipe_fd, &command, sizeof(int));
  write(own_req_pipe_fd, &event_id, sizeof(unsigned int));
  write(own_req_pipe_fd, &num_rows, sizeof(size_t));
  write(own_req_pipe_fd, &num_cols, sizeof(size_t));


  int response = wait_for_answer();

  printf("create response: %d\n", response);
  return response;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  // send reserve request to the server (through the request pipe) and wait for the response (through the response pipe)
  int command = 4;
  write(own_req_pipe_fd, &command, sizeof(int));
  write(own_req_pipe_fd, &event_id, sizeof(unsigned int));
  write(own_req_pipe_fd, &num_seats, sizeof(size_t));
  write(own_req_pipe_fd, xs, num_seats* sizeof(size_t));
  write(own_req_pipe_fd, ys, num_seats* sizeof(size_t));

  int response;
  response = wait_for_answer();
  printf("Reserve response: %d\n", response);
  return response;
}

int ems_show(int out_fd, unsigned int event_id) {
  size_t num_rows, num_cols;
  unsigned int *seats;
    char newline[] = "\n";
    char space[] = " ";

  // end show request to the server (through the request pipe) and wait for the response (through the response pipe)
  int command = 5;
  write(own_req_pipe_fd, &command, sizeof(int));
  write(own_req_pipe_fd, &event_id, sizeof(unsigned int));

  int response;
  response = wait_for_answer();

  if (response == 0) {
    read(own_resp_pipe_fd, &num_rows, sizeof(size_t));
    read(own_resp_pipe_fd, &num_cols, sizeof(size_t));

    seats = malloc(num_rows * num_cols * sizeof(unsigned int));

    read(own_resp_pipe_fd, seats, num_rows * num_cols * sizeof(unsigned int));
  }
  
  for (size_t i = 1; i <= num_rows; i++) {
    for (size_t j = 1; j <= num_cols; j++) {
      char buffer[16];

      sprintf(buffer, "%u", seats[(i - 1) * num_cols + j - 1]);

      if (print_str(out_fd, buffer)) {
        perror("Error writing to file descriptor");
        return 1;
      }

      if (j < num_cols) {
        if (print_str(out_fd, space)) {
          perror("Error writing to file descriptor");
          return 1;
        }
      }
    }

    if (print_str(out_fd, newline)) {
      perror("Error writing to file descriptor");
      return 1;
    }
  }

  free(seats);
  printf("show response: %d\n", response);
  return response;
}

int ems_list_events(int out_fd) {
  // send list request to the server (through the request pipe) and wait for the response (through the response pipe)
  size_t num_events;
  unsigned int *ids;
  
  int command = 6;
  write(own_req_pipe_fd, &command, sizeof(int));

  int response;
  response = wait_for_answer();

  if (response == 0) {
    read(own_resp_pipe_fd, &num_events, sizeof(size_t));

    ids = malloc(num_events * sizeof(unsigned int));

    read(own_resp_pipe_fd, ids, num_events * sizeof(unsigned int));
  }

  if (num_events == 0) {
    char buff[] = "No events\n";
    if (print_str(out_fd, buff)) {
      perror("Error writing to file descriptor");
      return 1;
    }

    return 0;
  }

  for (size_t i = 0; i < num_events; i++) {
    char buff[] = "Event: ";
    if (print_str(out_fd, buff)) {
      perror("Error writing to file descriptor");
      return 1;
    }

    char id[16];
    sprintf(id, "%u\n", ids[i]);
    if (print_str(out_fd, id)) {
      perror("Error writing to file descriptor");
      return 1;
    }
  }
  
  free(ids);
  printf("list events response: %d\n", response);
  return response;
}