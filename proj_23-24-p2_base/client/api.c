#include "api.h"
#include "common/constants.h"
#include "common/io.h"

#include <fcntl.h>
#include <unistd.h>

#include <stdio.h> //remover, só para o print de teste
#include <stdlib.h>
#include <string.h>

char const* own_req_pipe_path;
char const* own_resp_pipe_path;

// Variáveis globais
int session_id;

int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {

  //FIXME aqui escreve para o pipe?????? e aguarda o retorno????

  own_req_pipe_path = req_pipe_path;
  own_resp_pipe_path = resp_pipe_path;
  mkfifo(req_pipe_path, 0640);
  mkfifo(resp_pipe_path, 0640);

  int fd = open(server_pipe_path, O_WRONLY | O_TRUNC);
  int command = 1;
  write(fd, &command, sizeof(int));
  write(fd, own_req_pipe_path, 40* sizeof(char));
  write(fd, own_resp_pipe_path, 40 * sizeof(char));
  close(fd);

  //ESPERAR PELA RESPOSTA DO SERVIDOR ANTES DE CONTINUAR
  fd = open(own_resp_pipe_path, O_RDONLY);
  read(fd, &session_id, sizeof(int));
  close(fd);
  //if isto falhar, imprime mensagem de erro e lança uma mensagem de login quando der
  //FIXME \n

  //FIXME RETURN???
  return session_id;
}

int ems_quit(void) { 
  int command = 2;

  int fd = open(own_req_pipe_path, O_WRONLY | O_TRUNC);
  write(fd, &command, sizeof(int));
  close(fd);

  sleep(2); //REMOVER
  fd = open(own_resp_pipe_path, O_RDONLY);
  int response;
  while(read(fd, &response, sizeof(int)) < 1) {
    sleep(1);
  }
  close(fd);

  return response;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  // send create request to the server (through the request pipe) and wait for the response (through the response pipe)
  int command = 3;
  int fd = open(own_req_pipe_path, O_WRONLY | O_TRUNC);
  write(fd, &command, sizeof(int));
  write(fd, &event_id, sizeof(unsigned int));
  write(fd, &num_rows, sizeof(size_t));
  write(fd, &num_cols, sizeof(size_t));
  close(fd);

  fd = open(own_resp_pipe_path, O_RDONLY);
  int response;
  while(read(fd, &response, sizeof(int)) < 1) {
    sleep(1);
  }
  close(fd);

  printf("create response: %d\n", response);
  return response;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  // send reserve request to the server (through the request pipe) and wait for the response (through the response pipe)
  int command = 4;
  int fd = open(own_req_pipe_path, O_WRONLY | O_TRUNC);
  write(fd, &command, sizeof(int));
  write(fd, &event_id, sizeof(unsigned int));
  write(fd, &num_seats, sizeof(size_t));
  write(fd, xs, num_seats* sizeof(size_t));
  write(fd, ys, num_seats* sizeof(size_t));
  close(fd);

  fd = open(own_resp_pipe_path, O_RDONLY);
  int response;
  while(read(fd, &response, sizeof(int)) < 1) {
    sleep(1);
  }
  close(fd);
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
  int fd = open(own_req_pipe_path, O_WRONLY | O_TRUNC);
  write(fd, &command, sizeof(int));
  write(fd, &event_id, sizeof(unsigned int));
  close(fd);

  fd = open(own_resp_pipe_path, O_RDONLY);
  int response;
  while(read(fd, &response, sizeof(int)) < 1) {
    sleep(1);
  }

  if (response == 0) {
    read(fd, &num_rows, sizeof(size_t));
    read(fd, &num_cols, sizeof(size_t));

    seats = malloc(num_rows * num_cols * sizeof(unsigned int));

    read(fd, seats, num_rows * num_cols * sizeof(unsigned int));
    close(fd);
  } else {
    close(fd);
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

  close(fd);
  free(seats);
  printf("show response: %d\n", response);
  return response;
}

int ems_list_events(int out_fd) {
  // send list request to the server (through the request pipe) and wait for the response (through the response pipe)
  size_t num_events;
  unsigned int *ids;
  
  int command = 6;
  int fd = open(own_req_pipe_path, O_WRONLY | O_TRUNC);
  write(fd, &command, sizeof(int));
  close(fd);

  fd = open(own_resp_pipe_path, O_RDONLY);
  int response;
  while(read(fd, &response, sizeof(int)) < 1) {
    sleep(1);
  }

  if (response == 0) {
    read(fd, &num_events, sizeof(size_t));

    ids = malloc(num_events * sizeof(unsigned int));

    read(fd, ids, num_events * sizeof(unsigned int));
    close(fd);
  } else {
    close(fd);
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
  
  close(fd);
  free(ids);
  printf("list events response: %d\n", response);
  return response;
}