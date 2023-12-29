#include "api.h"
#include "common/constants.h"

#include <fcntl.h>
#include <unistd.h>

#include <stdio.h> //remover, só para o print de teste

char const* own_req_pipe_path;
char const* own_resp_pipe_path;

// Variáveis globais
int session_id;

int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {

  //FIXME aqui escreve para o pipe?????? e aguarda o retorno????

  own_req_pipe_path = req_pipe_path;
  own_resp_pipe_path = resp_pipe_path;

  int fd = open(server_pipe_path, O_WRONLY | O_TRUNC);
  int command = 1;
  write(fd, &command, sizeof(int));
  write(fd, own_req_pipe_path, 40* sizeof(char));
  write(fd, own_resp_pipe_path, 40 * sizeof(char));
  close(fd);

  //ESPERAR PELA RESPOSTA DO SERVIDOR ANTES DE CONTINUAR
  fd = open(own_resp_pipe_path, O_RDONLY);
  while(read(fd, &session_id, sizeof(int)) < 1) {
    sleep(1);
  }
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
  //TODO: send create request to the server (through the request pipe) and wait for the response (through the response pipe)
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
  //TODO: send reserve request to the server (through the request pipe) and wait for the response (through the response pipe)
  
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
  printf("reserve response: %d\n", response);
  return response;
}

int ems_show(int out_fd, unsigned int event_id) {
  //TODO: send show request to the server (through the request pipe) and wait for the response (through the response pipe)
  int command = 5;
  int fd = open(own_req_pipe_path, O_WRONLY | O_TRUNC);
  write(fd, &command, sizeof(int));
  write(fd, &out_fd, sizeof(int));
  write(fd, &event_id, sizeof(unsigned int));
  close(fd);

  fd = open(own_resp_pipe_path, O_RDONLY | O_TRUNC);
  int response;
  while(read(fd, &response, sizeof(int)) < 1) {
    sleep(1);
  }
  close(fd);
  printf("show response: %d\n", response);
  return response;
}

int ems_list_events(int out_fd) {
  //TODO: send list request to the server (through the request pipe) and wait for the response (through the response pipe)
  int command = 6;
  int fd = open(own_req_pipe_path, O_WRONLY | O_TRUNC);
  write(fd, &command, sizeof(int));
  write(fd, &out_fd, sizeof(int));
  close(fd);

  fd = open(own_resp_pipe_path, O_RDONLY);
  int response;
  while(read(fd, &response, sizeof(int)) < 1) {
    sleep(1);
  }
  close(fd);

  return response;
}
