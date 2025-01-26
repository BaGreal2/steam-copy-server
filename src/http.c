#include "http.h"
#include "defines.h"
#include "requests.h"
#include <arpa/inet.h>
#include <ctype.h>
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
    return NULL;
  }

  // Find the position of the query string (if any)
  const char *query_start = strchr(path, '?');
  size_t path_length = query_start ? (query_start - path) : strlen(path);

  // Allocate memory for a temporary buffer
  char *temp_path = malloc(path_length + 1);
  if (!temp_path) {
    return NULL;
  }

  // Copy the path up to the query string (or the full path if no query string)
  strncpy(temp_path, path, path_length);
  temp_path[path_length] = '\0';

  // Build the cleaned base path by skipping numeric segments
  char *base_path =
      malloc(path_length + 1); // Over-allocate; we'll shrink it later
  if (!base_path) {
    free(temp_path);
    return NULL;
  }

  size_t base_index = 0;
  const char *segment_start = temp_path;
  const char *segment_end;

  while ((segment_end = strchr(segment_start, '/')) || *segment_start != '\0') {
    // Handle the last segment (no more '/')
    if (!segment_end) {
      segment_end = temp_path + strlen(temp_path);
    }

    size_t segment_length = segment_end - segment_start;

    // Skip numeric segments
    int is_numeric = 1;
    for (size_t i = 0; i < segment_length; i++) {
      if (!isdigit(segment_start[i])) {
        is_numeric = 0;
        break;
      }
    }

    // Append non-numeric segments to the base path
    if (!is_numeric && segment_length > 0) {
      base_path[base_index++] = '/';
      strncpy(base_path + base_index, segment_start, segment_length);
      base_index += segment_length;
    }

    // Move to the next segment
    segment_start = segment_end + 1;
    if (*segment_end == '\0') {
      break;
    }
  }

  // Null-terminate the cleaned base path
  base_path[base_index] = '\0';

  // Free the temporary buffer
  free(temp_path);

  return base_path;
}

QueryParams extract_query(char *path)
{
  QueryParams result = {NULL, NULL, 0};

  if (!path || *path == '\0') {
    return result;
  }

  // Find the start of the query string
  const char *query_start = strchr(path, '?');
  if (!query_start || *(query_start + 1) == '\0') {
    return result; // No query parameters found
  }

  // Skip the '?' character
  query_start++;

  // Count the number of key-value pairs
  const char *p = query_start;
  while (*p) {
    if (*p == '&') {
      result.count++;
    }
    p++;
  }
  result.count++; // Add one more for the last key-value pair

  // Allocate memory for keys and values
  result.keys = malloc(result.count * sizeof(char *));
  result.values = malloc(result.count * sizeof(char *));
  if (!result.keys || !result.values) {
    free(result.keys);
    free(result.values);
    result.keys = result.values = NULL;
    result.count = 0;
    return result;
  }

  size_t index = 0;
  const char *key_start = query_start;
  while (key_start) {
    const char *key_end = strchr(key_start, '=');
    const char *value_start = key_end ? key_end + 1 : NULL;
    const char *value_end = value_start ? strchr(value_start, '&') : NULL;

    if (key_end) {
      // Extract the key
      size_t key_length = key_end - key_start;
      result.keys[index] = malloc(key_length + 1);
      if (result.keys[index]) {
        strncpy(result.keys[index], key_start, key_length);
        result.keys[index][key_length] = '\0';
      }
    } else {
      result.keys[index] = NULL;
    }

    if (value_start) {
      // Extract the value
      size_t value_length =
          value_end ? value_end - value_start : strlen(value_start);
      result.values[index] = malloc(value_length + 1);
      if (result.values[index]) {
        strncpy(result.values[index], value_start, value_length);
        result.values[index][value_length] = '\0';
      }
    } else {
      result.values[index] = NULL;
    }

    index++;
    key_start = value_end ? value_end + 1 : NULL;
  }

  return result;
}

void free_query_params(QueryParams *params)
{
  if (!params)
    return;

  for (size_t i = 0; i < params->count; i++) {
    free(params->keys[i]);
    free(params->values[i]);
  }
  free(params->keys);
  free(params->values);

  params->keys = NULL;
  params->values = NULL;
  params->count = 0;
}

char *extract_path_id(char *path)
{
  const char *id_start = strrchr(path, '/') + 1;
  if (!id_start || *id_start == '\0') {
    return NULL;
  }

  const char *id_end = strchr(id_start, '?');
  if (!id_end) {
    id_end = id_start + strlen(id_start);
  }

  size_t id_length = id_end - id_start;
  char *id = malloc(id_length + 1);
  if (!id) {
    return NULL;
  }

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

void handle_request(sqlite3 *db, char **err_msg, int socket)
{
  char *buffer = malloc(MAX_REQUEST_SIZE);
  if (!buffer) {
    perror("ERROR: Memory allocation failed");
    close(socket);
    return;
  }

  size_t total_bytes_read = 0;
  ssize_t bytes_read;
  int request_complete = 0;
  size_t content_length = 0;

  while ((bytes_read = read(socket, buffer + total_bytes_read, CHUNK_SIZE)) >
         0) {
    total_bytes_read += bytes_read;

    // Ensure we do not exceed MAX_REQUEST_SIZE
    if (total_bytes_read >= MAX_REQUEST_SIZE) {
      fprintf(stderr, "ERROR: Request exceeds maximum allowed size\n");
      free(buffer);
      close(socket);
      return;
    }

    // Null-terminate the buffer for string processing
    buffer[total_bytes_read] = '\0';

    // Check if headers are complete (find double CRLF)
    if (!request_complete && strstr(buffer, "\r\n\r\n") != NULL) {
      request_complete = 1;

      // Extract Content-Length from headers if it exists
      char *content_length_str = strstr(buffer, "Content-Length:");
      if (content_length_str) {
        content_length_str += strlen("Content-Length:");
        content_length = strtol(content_length_str, NULL, 10);
      }
    }

    // Check if the full body is read (if Content-Length is specified)
    if (request_complete && content_length > 0) {
      char *body_start = strstr(buffer, "\r\n\r\n") + 4;
      size_t body_bytes_read = total_bytes_read - (body_start - buffer);

      if (body_bytes_read >= content_length) {
        break; // Full request (headers + body) received
      }
    } else if (request_complete) {
      break; // No body or body length unspecified
    }
  }
  // read(socket, buffer, BUFFER_SIZE);

  printf("Total bytes read: %zu\n", total_bytes_read);
  printf("Request:\n%s\n\n", buffer);

  char *path = extract_path(buffer);
  char *path_base = extract_path_base(path);
  char *path_id = extract_path_id(path);
  QueryParams query = extract_query(path);
  char *method = extract_method(buffer);
  char *body = extract_body(buffer);

  printf("Path        : %s\n", path);
  printf("Path base   : %s\n", path_base);
  printf("Path id     : %s\n", path_id);
  printf("Method      : %s\n", method);
  printf("Query params:\n");
  for (size_t i = 0; i < query.count; i++) {
    printf("%zu: %s=%s\n", i, query.keys[i], query.values[i]);
  }
  printf("\n");

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
  } else {
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
    } else if (strcmp(path_base, "/login") == 0 &&
               strcmp(method, "POST") == 0) {
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
      if (strcmp(method, "GET") == 0 && query.count > 0) {
        // GET /me/games
        request_get_my_games(db, &query, &response, err_msg, socket);
      } else if (strcmp(method, "POST") == 0) {
        // POST /me/games
        request_post_my_game(db, body, &response, err_msg, socket);
      } else if (strcmp(method, "DELETE") == 0 && is_integer(path_id)) {
        // DELETE /me/games/:id
        request_delete_my_game(db, path_id, &query, &response, err_msg, socket);
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
    } else if (strcmp(path_base, "/me") == 0) {
      if (strcmp(method, "PATCH") == 0) {
        // PATCH /me
        request_patch_user(db, &query, body, &response, err_msg, socket);
      }
    } else if (strcmp(path_base, "/me/achievements") == 0) {
      if (strcmp(method, "GET") == 0 && is_integer(path_id)) {
        // GET /me/achievements/:id
        request_get_user_achievements_by_game_id(db, path_id, &query, &response,
                                                 err_msg, socket);
      } else if (strcmp(method, "GET") == 0) {
        // GET /me/achievements
        request_get_user_achievements(db, &query, &response, err_msg, socket);
      } else if (strcmp(method, "POST") == 0) {
        // POST /me/achievements
        request_post_user_achievement(db, body, &response, err_msg, socket);
      }
    } else if (strcmp(path_base, "/me/posted-games") == 0) {
      if (strcmp(method, "GET") == 0) {
        // GET /me/posted-games
        request_get_my_posted_games(db, &query, &response, err_msg, socket);
      }
    } else {
      printf("404 Not Found\n");
      response = construct_response(NOT_FOUND, "{\"error\": \"Not Found.\"}");
    }

    free_query_params(&query);
    send(socket, response, strlen(response), 0);
    printf("Response sent.\n");
  }
}
