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

int wait_for_response(int fd) {
  int response;
  while(read(fd, &response, sizeof(int))) {
    if (response == 0 || response == 1) {
      return response;
    }
    sleep(1);
  }
  return 0;
}

int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {

  //FIXME aqui escreve para o pipe?????? e aguarda o retorno????

  own_req_pipe_path = req_pipe_path;
  own_resp_pipe_path = resp_pipe_path;
  mkfifo(req_pipe_path, 0640);
  mkfifo(resp_pipe_path, 0640);

  char command = '1';
  char buffer[sizeof(char) + 40 * sizeof(char) + 40 * sizeof(char)];
  memcpy(buffer, &command, sizeof(char));
  memcpy(buffer + sizeof(char), req_pipe_path, 40 * sizeof(char));
  memcpy(buffer + sizeof(char) + 40 * sizeof(char), resp_pipe_path, 40 * sizeof(char));

  int fd = open(server_pipe_path, O_WRONLY);
  write(fd, buffer, sizeof(char) + 40 * sizeof(char) + 40 * sizeof(char));
  close(fd);

  //ESPERAR PELA RESPOSTA DO SERVIDOR ANTES DE CONTINUAR
  fd = open(own_resp_pipe_path, O_RDONLY);
  wait_for_response(fd);
  close(fd);
  //if isto falhar, imprime mensagem de erro e lança uma mensagem de login quando der
  //FIXME \n

  //FIXME RETURN???
  return 0;
}

int ems_quit(void) { 
  char command = '2';

  int fd = open(own_req_pipe_path, O_WRONLY | O_TRUNC);
  write(fd, &command, sizeof(char));
  close(fd);

  exit(0);

  return 1;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  // send create request to the server (through the request pipe) and wait for the response (through the response pipe)
  char command = '3';
  
  char buffer[sizeof(char) + sizeof(unsigned int) + sizeof(size_t) + sizeof(size_t)];
  memcpy(buffer, &command, sizeof(char));
  memcpy(buffer + sizeof(char), &event_id, sizeof(unsigned int));
  memcpy(buffer + sizeof(char) + sizeof(unsigned int), &num_rows, sizeof(size_t));
  memcpy(buffer + sizeof(char) + sizeof(unsigned int) + sizeof(size_t), &num_cols, sizeof(size_t));

  int fd = open(own_req_pipe_path, O_WRONLY | O_TRUNC);
  write(fd, buffer, sizeof(char) + sizeof(unsigned int) + sizeof(size_t) + sizeof(size_t));
  close(fd);

  fd = open(own_resp_pipe_path, O_RDONLY);
  int response = wait_for_response(fd);
  close(fd);

  printf("create response: %d\n", response);
  return response;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  // send reserve request to the server (through the request pipe) and wait for the response (through the response pipe)
  char command = '4';
  char buffer[sizeof(char) + sizeof(unsigned int) + sizeof(size_t) + num_seats * sizeof(size_t) + num_seats * sizeof(size_t)];

  memcpy(buffer, &command, sizeof(char));
  memcpy(buffer + sizeof(char), &event_id, sizeof(unsigned int));
  memcpy(buffer + sizeof(char) + sizeof(unsigned int), &num_seats, sizeof(size_t));
  memcpy(buffer + sizeof(char) + sizeof(unsigned int) + sizeof(size_t), xs, num_seats * sizeof(size_t));
  memcpy(buffer + sizeof(char) + sizeof(unsigned int) + sizeof(size_t) + num_seats * sizeof(size_t), ys, num_seats * sizeof(size_t));

  int fd = open(own_req_pipe_path, O_WRONLY | O_TRUNC);
  write(fd, buffer, sizeof(char) + sizeof(unsigned int) + sizeof(size_t) + num_seats * sizeof(size_t) + num_seats * sizeof(size_t));
  close(fd);

  fd = open(own_resp_pipe_path, O_RDONLY);
  int response = wait_for_response(fd);
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
  char command = '5';
  char buffer[sizeof(char) + sizeof(unsigned int)];
  
  memcpy(buffer, &command, sizeof(char));
  memcpy(buffer + sizeof(char), &event_id, sizeof(unsigned int));

  int fd = open(own_req_pipe_path, O_WRONLY | O_TRUNC);
  write(fd, buffer, sizeof(char) + sizeof(unsigned int));
  close(fd);

  fd = open(own_resp_pipe_path, O_RDONLY);
  int response = wait_for_response(fd);

  if (response == 0) {
    read(fd, &num_rows, sizeof(size_t));
    read(fd, &num_cols, sizeof(size_t));

    seats = malloc(num_rows * num_cols * sizeof(unsigned int));

    read(fd, seats, num_rows * num_cols * sizeof(unsigned int));
    close(fd);
  
  
  for (size_t i = 1; i <= num_rows; i++) {
    for (size_t j = 1; j <= num_cols; j++) {
      char line_buffer[16];

      sprintf(line_buffer, "%u", seats[(i - 1) * num_cols + j - 1]);

      if (print_str(out_fd, line_buffer)) {
        perror("Error writing to file descriptor");
        free(seats);
        return 1;
      }

      if (j < num_cols) {
        if (print_str(out_fd, space)) {
          perror("Error writing to file descriptor");
          free(seats);
          return 1;
        }
      }
    }

    if (print_str(out_fd, newline)) {
      perror("Error writing to file descriptor");
      free(seats);
      return 1;
    }
  }
  free(seats);
  }
  close(fd);
  printf("show response: %d\n", response);
  return response;
}

int ems_list_events(int out_fd) {
  // send list request to the server (through the request pipe) and wait for the response (through the response pipe)
  size_t num_events;
  unsigned int *ids;
  
  char command = '6';
  int fd = open(own_req_pipe_path, O_WRONLY | O_TRUNC);
  write(fd, &command, sizeof(char));
  close(fd);

  fd = open(own_resp_pipe_path, O_RDONLY);
  int response = wait_for_response(fd);

  if (response == 0) {
    read(fd, &num_events, sizeof(size_t));

    ids = malloc(num_events * sizeof(unsigned int));
    //malloc pode ser 0, aloca???

    read(fd, ids, num_events * sizeof(unsigned int));
    close(fd);
  

  if (num_events == 0) {
    char buff[] = "No events\n";
    if (print_str(out_fd, buff)) {
      perror("Error writing to file descriptor");
      free(ids);
      return 1;
    }
    free(ids); //se malloc for 0, isto aloca?
    return 0;
  }

  for (size_t i = 0; i < num_events; i++) {
    char buff[] = "Event: ";
    if (print_str(out_fd, buff)) {
      perror("Error writing to file descriptor");
      free(ids);
      return 1;
    }

    char id[16];
    sprintf(id, "%u\n", ids[i]);
    if (print_str(out_fd, id)) {
      perror("Error writing to file descriptor");
      free(ids);
      return 1;
    }
  }
  free(ids);
  }
  
  close(fd);
  printf("list events response: %d\n", response);
  return response;
}