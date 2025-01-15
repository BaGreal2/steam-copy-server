#include "http.h"
#include "defines.h"
#include "routes.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int is_integer(const char *str)
{
  if (str == 0 || *str == '\0') {
    return 0;
  }

  char *endptr;
  strtol(str, &endptr, 10);

  if (*endptr != '\0' || str == endptr) {
    return 0;
  }

  return 1;
}

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

  const char *header_format =
      "HTTP/1.1 %s\r\n"
      "Content-Type: application/json\r\n"
      "Access-Control-Allow-Origin: *\r\n"
      "Access-Control-Allow-Methods: GET, POST, PATCH, DELETE, OPTIONS\r\n"
      "Access-Control-Allow-Headers: Content-Type\r\n"
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
  if (!path || *path == '\0') {
    return 0;
  }

  const char *last_slash = strrchr(path, '/');
  if (!last_slash || last_slash == path) {
    return strdup(path);
  }

  const char *potential_id = last_slash + 1;
  for (const char *p = potential_id; *p != '\0'; p++) {
    if (*p < '0' || *p > '9') {
      return strdup(path);
    }
  }

  size_t base_length = last_slash - path;
  char *base_path = malloc(base_length + 1);
  if (!base_path) {
    return 0;
  }

  strncpy(base_path, path, base_length);
  base_path[base_length] = '\0';

  return base_path;
}

char *extract_path_id(char *path)
{
  const char *id_start = strrchr(path, '/') + 1;
  int id_length = strlen(id_start);
  if (id_length == (int)strlen(path) - 1) {
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

void handle_request(sqlite3 *db, char **err_msg, char buffer[BUFFER_SIZE],
                    int socket)
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

  if (strcmp(method, "OPTIONS") == 0) {
    const char *options_response =
        "HTTP/1.1 204 No Content\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, PATCH, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Content-Length: 0\r\n"
        "\r\n";

    send(socket, options_response, strlen(options_response), 0);
    printf("Preflight OPTIONS response sent.\n");
  }

  if (strcmp(path_base, "/games") == 0) {
    if (strcmp(method, "GET") == 0 && is_integer(path_id)) {
      // GET /games/:id
      request_get_game_by_id(db, path_id, &response, err_msg);
    } else if (strcmp(method, "GET") == 0) {
      // GET /games
      request_get_games(db, &response, err_msg);
    } else if (strcmp(method, "POST") == 0) {
      // POST /games
      request_post_game(db, body, &response, err_msg, socket);
    } else if (strcmp(method, "DELETE") == 0 && is_integer(path_id)) {
      // DELETE /games/:id
      request_delete_game_by_id(db, path_id, &response, err_msg);
    } else if (strcmp(method, "PATCH") == 0 && is_integer(path_id)) {
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
  } else if (strcmp(path_base, "/reviews/game") == 0) {
    if (strcmp(method, "GET") == 0 && is_integer(path_id)) {
      // GET /reviews/game/:id
      request_get_reviews_by_game_id(db, path_id, &response, err_msg);
    } else if (strcmp(method, "POST") == 0 && is_integer(path_id)) {
      // POST /reviews/game/:id
      request_post_review(db, path_id, body, &response, err_msg, socket);
    }
  } else if (strcmp(path_base, "/me/games") == 0) {
    if (strcmp(method, "GET") == 0) {
      // GET /me/games
      request_get_my_games(db, body, &response, err_msg, socket);
    } else if (strcmp(method, "POST") == 0) {
      // POST /me/games
      request_post_my_game(db, body, &response, err_msg, socket);
    } else if (strcmp(method, "DELETE") == 0 && is_integer(path_id)) {
      // DELETE /me/games/:id
      request_delete_my_game(db, path_id, body, &response, err_msg, socket);
    }
  } else if (strcmp(path_base, "/achievements") == 0) {
    if (strcmp(method, "GET") == 0 && is_integer(path_id)) {
      // GET /achievements/:id
      request_get_achievement_by_id(db, path_id, &response, err_msg);
    } else if (strcmp(method, "POST") == 0) {
      // POST /achievements
      request_post_achievement(db, body, &response, err_msg, socket);
    } else if (strcmp(method, "PATCH") == 0 && is_integer(path_id)) {
      // PATCH /achievements/:id
      request_patch_achievement_by_id(db, path_id, body, &response, err_msg);
    } else if (strcmp(method, "DELETE") == 0 && is_integer(path_id)) {
      // DELETE /achievements/:id
      request_delete_achievement_by_id(db, path_id, &response, err_msg);
    }
  } else if (strcmp(path_base, "/achievements/game") == 0) {
    if (strcmp(method, "GET") == 0 && is_integer(path_id)) {
      // GET /achievements/game/:id
      request_get_achievements_by_game_id(db, path_id, &response, err_msg);
    }
  } else if (strcmp(path_base, "/me/achievements") == 0) {
    if (strcmp(method, "GET") == 0 && is_integer(path_id)) {
      // GET /me/achievements/:id
      request_get_user_achievements_by_game_id(db, path_id, body, &response,
                                               err_msg, socket);
    } else if (strcmp(method, "GET") == 0) {
      // GET /me/achievements
      request_get_user_achievements(db, body, &response, err_msg, socket);
    } else if (strcmp(method, "POST") == 0) {
      // POST /me/achievements
      request_post_user_achievement(db, body, &response, err_msg, socket);
    }
  } else {
    printf("404 Not Found\n");
    response = construct_response(NOT_FOUND, "{\"error\": \"Not Found.\"}");
  }

  send(socket, response, strlen(response), 0);
  printf("Response sent.\n");
}
