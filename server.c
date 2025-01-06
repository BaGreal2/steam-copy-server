#include "cJSON.h"
#include <arpa/inet.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024

typedef struct {
  char *data;
  size_t size;
} ResponseBuffer;

int callback_all_games(void *buffer, int argc, char **argv, char **colName)
{
  cJSON *json_array = (cJSON *)buffer;

  cJSON *json_row = cJSON_CreateObject();
  if (!json_row) {
    fprintf(stderr, "ERROR: Failed to create JSON object.\n");
    return 1;
  }

  for (int i = 0; i < argc; i++) {
    const char *key = colName[i];
    const char *value = argv[i] ? argv[i] : NULL;

    if (value) {
      cJSON_AddStringToObject(json_row, key, value);
    } else {
      cJSON_AddNullToObject(json_row, key);
    }
  }

  cJSON_AddItemToArray(json_array, json_row);

  return 0;
}

int callback_game_by_id(void *buffer, int argc, char **argv, char **colName)
{
  cJSON *json_object = (cJSON *)buffer;

  for (int i = 0; i < argc; i++) {
    const char *key = colName[i];
    const char *value = argv[i] ? argv[i] : NULL;

    if (value) {
      cJSON_AddStringToObject(json_object, key, value);
    } else {
      cJSON_AddNullToObject(json_object, key);
    }
  }

  return 0;
}

void db_request(sqlite3 *db, const char *sql,
                int (*callback)(void *, int, char **, char **), void *data,
                char **err_msg, void *description)
{
  int rc = sqlite3_exec(db, sql, callback, data, err_msg);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "ERROR: Failed to execute SQL: %s\n", *err_msg);
    sqlite3_free(*err_msg);
    *err_msg = NULL;
  } else {
    if (description) {
      printf("LOG: %s\n", (char *)description);
    } else {
      printf("LOG: SQL query executed successfully.\n");
    }
  }
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

char *construct_response(char *body)
{
  const char *header = "HTTP/1.1 200 OK\r\n"
                       "Content-Type: text/plain\r\n"
                       "Content-Length: %zu\r\n"
                       "\r\n";

  size_t header_len = strlen(header) + strlen(body) + 20;
  char *response = malloc(header_len);

  if (!response) {
    fprintf(stderr, "ERROR: Memory allocation failed.\n");
    return NULL;
  }

  sprintf(response, header, strlen(body));
  strcat(response, body);

  return response;
}

int main()
{
  sqlite3 *db;
  char *err_msg = 0;
  int rc;

  rc = sqlite3_open("steam.db", &db);
  if (rc) {
    fprintf(stderr, "ERROR: Can't open database: %s\n", sqlite3_errmsg(db));
    exit(EXIT_FAILURE);
  }
  printf("LOG: Opened database successfully.\n");

  const char *create_users_table_sql =
      "CREATE TABLE IF NOT EXISTS Users("
      "user_id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "username TEXT NOT NULL UNIQUE, "
      "email TEXT NOT NULL UNIQUE, "
      "created_at DATETIME DEFAULT CURRENT_TIMESTAMP);";
  const char *create_games_table_sql =
      "CREATE TABLE IF NOT EXISTS Games("
      "game_id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "title TEXT NOT NULL, "
      "genre TEXT NOT NULL, "
      "release_date DATE, "
      "developer TEXT);";
  const char *create_libraries_table_sql =
      "CREATE TABLE IF NOT EXISTS Libraries("
      "library_id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "user_id INTEGER NOT NULL, "
      "game_id INTEGER NOT NULL, "
      "purchased_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
      "FOREIGN KEY (user_id) REFERENCES Users(user_id) ON DELETE CASCADE, "
      "FOREIGN KEY (game_id) REFERENCES Games(game_id) ON DELETE CASCADE, "
      "UNIQUE(user_id, game_id));";
  const char *create_reviews_table_sql =
      "CREATE TABLE IF NOT EXISTS Reviews("
      "review_id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "user_id INTEGER NOT NULL, "
      "game_id INTEGER NOT NULL, "
      "rating INTEGER NOT NULL CHECK (rating >= 1 AND rating <=5), "
      "review_text TEXT, "
      "created_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
      "FOREIGN KEY (user_id) REFERENCES Users(user_id) ON DELETE CASCADE, "
      "FOREIGN KEY (game_id) REFERENCES Games(game_id) ON DELETE CASCADE);";
  db_request(db, create_users_table_sql, 0, 0, &err_msg,
             "Users table created.");
  db_request(db, create_games_table_sql, 0, 0, &err_msg,
             "Games table created.");
  db_request(db, create_libraries_table_sql, 0, 0, &err_msg,
             "Libraries table created.");
  db_request(db, create_reviews_table_sql, 0, 0, &err_msg,
             "Reviews table created.");

  // insert data...

  int server_fd, new_socket;
  struct sockaddr_in address;
  int addrlen = sizeof(address);
  char buffer[BUFFER_SIZE] = {0};

  // Step 1: Create a socket
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == 0) {
    perror("ERROR: Socket failed");
    exit(EXIT_FAILURE);
  }

  // Step 2: Bind the socket to an address and port
  address.sin_family = AF_INET;         // IPv4
  address.sin_addr.s_addr = INADDR_ANY; // Accept connections from any IP
  address.sin_port = htons(PORT);       // Convert port to network byte order

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("ERROR: Bind failed");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  // Step 3: Listen for incoming connections
  if (listen(server_fd, 3) < 0) {
    perror("ERROR: Listen failed");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  printf("HTTP server is running on port %d\n", PORT);

  // Step 4: Accept a connection and handle requests
  while (1) {
    new_socket =
        accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
    if (new_socket < 0) {
      perror("ERROR: Accept failed");
      continue;
    }

    // Step 5: Read the HTTP request
    read(new_socket, buffer, BUFFER_SIZE);
    printf("Request:\n%s\n", buffer);

    // Step 6: Send an HTTP response
    char *path = extract_path(buffer);
    char *path_base = extract_path_base(path);
    char *path_id = extract_path_id(path);
    char *method = extract_method(buffer);

    printf("Path: %s\n", path);
    printf("Path base: %s\n", path_base);
    printf("Path id: %s\n", path_id);
    printf("Method: %s\n", method);

    char *response;

    if (strcmp(path_base, "/games") == 0) {
      if (strcmp(method, "GET") == 0) {
        // GET /games
        if (!path_id) {
          const char *all_games_sql = "SELECT * FROM Games;";

          cJSON *json_array = cJSON_CreateArray();
          if (!json_array) {
            fprintf(stderr, "ERROR: Failed to create JSON array.\n");
            return 0;
          }
          db_request(db, all_games_sql, callback_all_games, json_array, &err_msg,
                     "Fetched all games");

          char *json_string = cJSON_PrintUnformatted(json_array);

          if (json_string) {
            response = construct_response(json_string);
            free(json_string);
          } else {
            response = construct_response(
                "{\"error\": \"Failed to serialize JSON.\"}");
          }
          cJSON_Delete(json_array);

          // GET /games/:id
        } else {
          const char *game_sql = "SELECT * FROM Games WHERE game_id = %s;";
          char *game_format_sql =
              malloc(strlen(game_sql) + strlen(path_id) + 1);
          sprintf(game_format_sql, game_sql, path_id);

          cJSON *json_array = cJSON_CreateObject();
          if (!json_array) {
            fprintf(stderr, "ERROR: Failed to create JSON object.\n");
            return 0;
          }
          db_request(db, game_format_sql, callback_game_by_id, json_array, &err_msg,
                     "Fetched game by id");

          char *json_string = cJSON_PrintUnformatted(json_array);

          if (json_string) {
            response = construct_response(json_string);
            free(json_string);
          } else {
            response = construct_response(
                "{\"error\": \"Failed to serialize JSON.\"}");
          }
          cJSON_Delete(json_array);
        }
      }
    } else {
      printf("404 Not Found\n");
      response = "HTTP/1.1 404 Not Found\r\n";
    }

    send(new_socket, response, strlen(response), 0);

    printf("Response sent.\n");

    // Close the connection
    close(new_socket);
  }

  // Cleanup
  close(server_fd);
  return 0;
}
