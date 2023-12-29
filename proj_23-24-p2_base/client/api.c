#include "api.h"
#include "common/constants.h"

#include <fcntl.h>

char const* own_req_pipe_path;
char const* own_resp_pipe_path;

// Variáveis globais
int session_id;

int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {

  //FIXME aqui escreve para o pipe?????? e aguarda o retorno????

  own_req_pipe_path = req_pipe_path;
  own_resp_pipe_path = resp_pipe_path;

  int fd = open(server_pipe_path, O_WRONLY);
  write(fd, 1, sizeof(int));
  write(fd, own_req_pipe_path, 40* sizeof(char));
  write(fd, own_resp_pipe_path, 40 * sizeof(char));

  //ESPERAR PELA RESPOSTA DO SERVIDOR ANTES DE CONTINUAR
  read(own_resp_pipe_path, &session_id, sizeof(int));
  //if isto falhar, imprime mensagem de erro e lança uma mensagem de login quando der
  //FIXME \n

  //FIXME RETURN???
  return session_id;
}

int ems_quit(void) { 

  return 1;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  //TODO: send create request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  //TODO: send reserve request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}

int ems_show(int out_fd, unsigned int event_id) {
  //TODO: send show request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}

int ems_list_events(int out_fd) {
  //TODO: send list request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}
