#pragma once

#include "defines.h"
#include <sqlite3.h>
#include <stdlib.h>

typedef enum {
  SUCCESS = 200,
  EMPTY = 204,
  BAD_REQUEST = 400,
  NOT_FOUND = 404,
  INTERNAL_SERVER_ERROR = 500
} StatusCode;

typedef struct {
  char **keys;
  char **values;
  size_t count;
} QueryParams;

char *construct_response(StatusCode status_code, const char *body);

char *extract_path(char *request);
char *extract_path_base(char *path);
QueryParams extract_query(char *path);
void free_query_params(QueryParams *params);
char *extract_path_id(char *path);
char *extract_method(char *request);
char *extract_body(char *request);

int is_integer(const char *str);

void handle_request(sqlite3 *db, char **err_msg, int socket);
