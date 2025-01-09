#pragma once

#include "defines.h"
#include <sqlite3.h>

typedef enum {
  SUCCESS = 200,
  EMPTY = 204,
  BAD_REQUEST = 400,
  NOT_FOUND = 404,
  INTERNAL_SERVER_ERROR = 500
} StatusCode;

char *construct_response(StatusCode status_code, const char *body);

char *extract_path(char *request);
char *extract_path_base(char *path);
char *extract_path_id(char *path);
char *extract_method(char *request);
char *extract_body(char *request);

void handle_request(sqlite3 *db, char **err_msg, char buffer[BUFFER_SIZE],
                    int socket);
