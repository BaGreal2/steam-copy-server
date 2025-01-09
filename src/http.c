#include "http.h"
#include "defines.h"
#include "routes.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *construct_response(StatusCode status_code, const char *body)
{
  const char *status_text;
  switch (status_code) {
  case SUCCESS:
    status_text = "200 OK";
    break;
  case EMPTY:
    status_text = "204 No Content";
    break;
  case INTERNAL_SERVER_ERROR:
    status_text = "500 Internal Server Error";
    break;
  case NOT_FOUND:
    status_text = "404 Not Found";
    break;
  default:
    status_text = "400 Bad Request";
    break;
  }

  const char *header_format = "HTTP/1.1 %s\r\n"
                              "Content-Type: application/json\r\n"
                              "Content-Length: %zu\r\n"
                              "\r\n";

  size_t header_len =
      strlen(header_format) + strlen(status_text) + strlen(body) + 20;
  char *response = malloc(header_len);
  if (!response) {
    fprintf(stderr, "ERROR: Memory allocation failed.\n");
    return NULL;
  }

  sprintf(response, header_format, status_text, strlen(body));
  strcat(response, body);

  return response;
}

char *extract_path(char *request)
{
  const char *path_start = strchr(request, ' ') + 1;
  const char *path_end = strchr(path_start, ' ');

  int path_length = path_end - path_start;
  char *path = malloc(path_length + 1);
  strncpy(path, path_start, path_length);
  path[path_length] = '\0';
  return path;
}

char *extract_path_base(char *path)
{
  const char *path_base_end = strchr(path + 1, '/');
  if (!path_base_end) {
    return path;
  }
  int path_base_length = path_base_end - path;
  char *path_base = malloc(path_base_length + 1);
  strncpy(path_base, path, path_base_length);
  path_base[path_base_length] = '\0';
  return path_base;
}

char *extract_path_id(char *path)
{
  const char *id_start = strrchr(path, '/') + 1;
  int id_length = strlen(id_start);
  if (id_length == strlen(path) - 1) {
    return 0;
  }
  char *id = malloc(id_length + 1);
  strncpy(id, id_start, id_length);
  id[id_length] = '\0';
  return id;
}

char *extract_method(char *request)
{
  const char *method_end = strchr(request, ' ');

  int method_length = method_end - request;
  char *method = malloc(method_length + 1);
  strncpy(method, request, method_length);
  method[method_length] = '\0';
  return method;
}

char *extract_body(char *request)
{
  const char *body_start = strstr(request, "\r\n\r\n") + 4;
  if (!body_start) {
    return 0;
  }
  int body_length = strlen(body_start);
  char *body = malloc(body_length + 1);
  strncpy(body, body_start, body_length);
  body[body_length] = '\0';
  return body;
}

void handle_request(sqlite3 *db, char **err_msg, char buffer[BUFFER_SIZE], int socket)
{
  read(socket, buffer, BUFFER_SIZE);
  printf("Request:\n%s\n\n", buffer);

  char *path = extract_path(buffer);
  char *path_base = extract_path_base(path);
  char *path_id = extract_path_id(path);
  char *method = extract_method(buffer);
  char *body = extract_body(buffer);

  printf("Path     : %s\n", path);
  printf("Path base: %s\n", path_base);
  printf("Path id  : %s\n", path_id);
  printf("Method   : %s\n", method);

  char *response;

  if (strcmp(path_base, "/games") == 0) {
    if (strcmp(method, "GET") == 0 && path_id) {
      // GET /games/:id
      request_get_game_by_id(db, path_id, &response, err_msg);
    } else if (strcmp(method, "GET") == 0) {
      // GET /games
      request_get_games(db, &response, err_msg);
    } else if (strcmp(method, "POST") == 0) {
      // POST /games
      request_post_game(db, body, &response, err_msg, socket);
    } else if (strcmp(method, "DELETE") == 0 && path_id) {
      // DELETE /games/:id
      request_delete_game_by_id(db, path_id, &response, err_msg);
    } else if (strcmp(method, "PATCH") == 0 && path_id) {
      // PATCH /games/:id
      request_patch_game_by_id(db, path_id, body, &response, err_msg);
    }
  } else if (strcmp(path_base, "/register") == 0 &&
             strcmp(method, "POST") == 0) {
    // POST /register
    request_post_register(db, body, &response, err_msg, socket);
  } else if (strcmp(path_base, "/login") == 0 && strcmp(method, "POST") == 0) {
    // POST /login
    request_post_login(db, body, &response, err_msg, socket);
  } else {
    printf("404 Not Found\n");
    response = construct_response(NOT_FOUND, "{\"error\": \"Not Found.\"}");
  }

  send(socket, response, strlen(response), 0);
  printf("Response sent.\n");
}
