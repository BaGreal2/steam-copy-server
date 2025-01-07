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

int callback_array(void *buffer, int argc, char **argv, char **colName)
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

int callback_object(void *buffer, int argc, char **argv, char **colName)
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

char *construct_response(int status_code, const char *body)
{
  const char *status_text;
  switch (status_code) {
  case 200:
    status_text = "200 OK";
    break;
  case 500:
    status_text = "500 Internal Server Error";
    break;
  case 404:
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
      "password TEXT NOT NULL, "
      "profile_image TEXT, "
      "created_at DATETIME DEFAULT CURRENT_TIMESTAMP);";
  const char *create_games_table_sql =
      "CREATE TABLE IF NOT EXISTS Games("
      "game_id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "title TEXT NOT NULL, "
      "genre TEXT NOT NULL, "
      "cover_image TEXT NOT NULL, "
      "icon_image TEXT NOT NULL, "
      "release_date DATETIME DEFAULT CURRENT_TIMESTAMP, "
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
  const char *create_achievements_table_sql =
      "CREATE TABLE IF NOT EXISTS Achievements("
      "achievement_id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "game_id INTEGER NOT NULL, "
      "name TEXT NOT NULL, "
      "description TEXT NOT NULL, "
      "points INTEGER NOT NULL, "
      "FOREIGN KEY (game_id) REFERENCES Games(game_id) ON DELETE CASCADE);";
  const char *create_user_achievements_table_sql =
      "CREATE TABLE IF NOT EXISTS User_Achievements("
      "user_achievement_id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "user_id INTEGER NOT NULL, "
      "achievement_id INTEGER NOT NULL, "
      "unlocked_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
      "FOREIGN KEY (user_id) REFERENCES Users(user_id) ON DELETE CASCADE, "
      "FOREIGN KEY (achievement_id) REFERENCES Achievements(achievement_id) ON "
      "DELETE CASCADE);";

  db_request(db, create_users_table_sql, 0, 0, &err_msg,
             "Users table created.");
  db_request(db, create_games_table_sql, 0, 0, &err_msg,
             "Games table created.");
  db_request(db, create_libraries_table_sql, 0, 0, &err_msg,
             "Libraries table created.");
  db_request(db, create_reviews_table_sql, 0, 0, &err_msg,
             "Reviews table created.");
  db_request(db, create_achievements_table_sql, 0, 0, &err_msg,
             "Achievements table created.");
  db_request(db, create_user_achievements_table_sql, 0, 0, &err_msg,
             "User_Achievements table created.");

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
    char *body = extract_body(buffer);

    printf("Path: %s\n", path);
    printf("Path base: %s\n", path_base);
    printf("Path id: %s\n", path_id);
    printf("Method: %s\n", method);

    char *response;

    if (strcmp(path_base, "/games") == 0) {
      if (strcmp(method, "GET") == 0) {
        if (path_id) {
          // GET /games/:id
          const char *game_by_id_sql =
              "SELECT * FROM Games WHERE game_id = %s;";
          char *game_by_id_format_sql =
              malloc(strlen(game_by_id_sql) + strlen(path_id) + 1);
          sprintf(game_by_id_format_sql, game_by_id_sql, path_id);

          cJSON *json = cJSON_CreateObject();
          if (!json) {
            fprintf(stderr, "ERROR: Failed to create JSON object.\n");
            return 0;
          }
          db_request(db, game_by_id_format_sql, callback_object, json, &err_msg,
                     "Fetched game by id");

          int response_code = 200;
          if (cJSON_GetArraySize(json) == 0) {
            response_code = 404;
            cJSON_AddStringToObject(json, "error", "Game not found.");
          }

          char *json_string = cJSON_PrintUnformatted(json);

          if (json_string) {
            response = construct_response(response_code, json_string);
            free(json_string);
          } else {
            response = construct_response(
                500, "{\"error\": \"Failed to serialize JSON.\"}");
          }

          cJSON_Delete(json);
          free(game_by_id_format_sql);
        } else {
          // GET /games
          const char *all_games_sql = "SELECT * FROM Games;";

          cJSON *json_array = cJSON_CreateArray();
          if (!json_array) {
            fprintf(stderr, "ERROR: Failed to create JSON array.\n");
            return 0;
          }
          db_request(db, all_games_sql, callback_array, json_array, &err_msg,
                     "Fetched all games");

          char *json_string = cJSON_PrintUnformatted(json_array);

          if (json_string) {
            response = construct_response(200, json_string);
            free(json_string);
          } else {
            response = construct_response(
                500, "{\"error\": \"Failed to serialize JSON.\"}");
          }

          cJSON_Delete(json_array);
        }
      } else if (strcmp(method, "POST") == 0) {
        // POST /games
        const char *insert_game_sql = "INSERT INTO Games (title, genre, "
                                      "cover_image, icon_image, developer) "
                                      "VALUES ('%s', '%s', '%s', '%s', '%s');";
        char *insert_game_format_sql = malloc(strlen(insert_game_sql) + 200);

        cJSON *json = cJSON_Parse(body);
        if (!json) {
          response = construct_response(
              400, "{\"error\": \"Failed to parse JSON request body.\"}");
        } else {
          cJSON *title = cJSON_GetObjectItem(json, "title");
          if (!title) {
            response = construct_response(
                400, "{\"error\": \"Missing required field: title.\"}");
            send(new_socket, response, strlen(response), 0);
            continue;
          }
          cJSON *genre = cJSON_GetObjectItem(json, "genre");
          if (!genre) {
            response = construct_response(
                400, "{\"error\": \"Missing required field: genre.\"}");
            send(new_socket, response, strlen(response), 0);
            continue;
          }
          cJSON *cover_image = cJSON_GetObjectItem(json, "cover_image");
          if (!cover_image) {
            response = construct_response(
                400, "{\"error\": \"Missing required field: cover_image.\"}");
            send(new_socket, response, strlen(response), 0);
            continue;
          }
          cJSON *icon_image = cJSON_GetObjectItem(json, "icon_image");
          if (!icon_image) {
            response = construct_response(
                400, "{\"error\": \"Missing required field: icon_image.\"}");
            send(new_socket, response, strlen(response), 0);
            continue;
          }
          cJSON *developer = cJSON_GetObjectItem(json, "developer");
          if (!developer) {
            response = construct_response(
                400, "{\"error\": \"Missing required field: developer.\"}");
            send(new_socket, response, strlen(response), 0);
            continue;
          }

          sprintf(insert_game_format_sql, insert_game_sql, title->valuestring,
                  genre->valuestring, cover_image->valuestring,
                  icon_image->valuestring, developer->valuestring);

          db_request(db, insert_game_format_sql, 0, 0, &err_msg,
                     "Inserted game");

          response =
              construct_response(200, "{\"message\": \"Game inserted.\"}");
        }

        cJSON_Delete(json);
        free(insert_game_format_sql);
      } else if (strcmp(method, "DELETE") == 0 && path_id) {
        // DELETE /games/:id
        const char *delete_game_sql = "DELETE FROM Games WHERE game_id = %s;";
        char *delete_game_format_sql =
            malloc(strlen(delete_game_sql) + strlen(path_id) + 1);
        sprintf(delete_game_format_sql, delete_game_sql, path_id);

        db_request(db, delete_game_format_sql, 0, 0, &err_msg,
                   "Deleted game by id");

        response = construct_response(200, "{\"message\": \"Game deleted.\"}");
        free(delete_game_format_sql);
      } else if (strcmp(method, "PATCH") == 0 && path_id) {
        // PUT /games/:id
        char *update_game_sql = malloc(1024);
        sprintf(update_game_sql, "UPDATE Games SET ");
        int has_updates = 0;

        cJSON *json = cJSON_Parse(body);
        if (!json) {
          response = construct_response(
              400, "{\"error\": \"Failed to parse JSON request body.\"}");
        } else {
          cJSON *title = cJSON_GetObjectItem(json, "title");
          cJSON *genre = cJSON_GetObjectItem(json, "genre");
          cJSON *cover_image = cJSON_GetObjectItem(json, "cover_image");
          cJSON *icon_image = cJSON_GetObjectItem(json, "icon_image");
          cJSON *release_date = cJSON_GetObjectItem(json, "release_date");
          cJSON *developer = cJSON_GetObjectItem(json, "developer");

          if (title && cJSON_IsString(title)) {
            strcat(update_game_sql, "title = '");
            strcat(update_game_sql, title->valuestring);
            strcat(update_game_sql, "', ");
            has_updates = 1;
          }
          if (genre && cJSON_IsString(genre)) {
            strcat(update_game_sql, "genre = '");
            strcat(update_game_sql, genre->valuestring);
            strcat(update_game_sql, "', ");
            has_updates = 1;
          }
          if (cover_image && cJSON_IsString(cover_image)) {
            strcat(update_game_sql, "cover_image = '");
            strcat(update_game_sql, cover_image->valuestring);
            strcat(update_game_sql, "', ");
            has_updates = 1;
          }
          if (icon_image && cJSON_IsString(icon_image)) {
            strcat(update_game_sql, "icon_image = '");
            strcat(update_game_sql, icon_image->valuestring);
            strcat(update_game_sql, "', ");
            has_updates = 1;
          }
          if (release_date && cJSON_IsString(release_date)) {
            strcat(update_game_sql, "release_date = '");
            strcat(update_game_sql, release_date->valuestring);
            strcat(update_game_sql, "', ");
            has_updates = 1;
          }
          if (developer && cJSON_IsString(developer)) {
            strcat(update_game_sql, "developer = '");
            strcat(update_game_sql, developer->valuestring);
            strcat(update_game_sql, "', ");
            has_updates = 1;
          }

          if (has_updates) {
            // Remove the trailing comma and space
            update_game_sql[strlen(update_game_sql) - 2] = '\0';

            // Add the WHERE clause
            strcat(update_game_sql, " WHERE game_id = ");
            strcat(update_game_sql, path_id);
            strcat(update_game_sql, ";");

            // Execute the SQL query
            db_request(db, update_game_sql, 0, 0, &err_msg,
                       "Updated game by id");

            response =
                construct_response(200, "{\"message\": \"Game updated.\"}");
          } else {
            response = construct_response(
                400, "{\"error\": \"No fields provided to update.\"}");
          }
        }

        free(update_game_sql);
        cJSON_Delete(json);
      }
    } else if (strcmp(path_base, "/register") == 0 &&
               strcmp(method, "POST") == 0) {
      // POST /register
      char *create_user_sql =
          "INSERT INTO Users (username, email, password, profile_image) "
          "VALUES ('%s', '%s', '%s', '%s');";
      char *create_user_format_sql = malloc(strlen(create_user_sql) + 200);

      cJSON *json = cJSON_Parse(body);

      if (!json) {
        response = construct_response(
            400, "{\"error\": \"Failed to parse JSON request body.\"}");
      } else {
        cJSON *username = cJSON_GetObjectItem(json, "username");
        if (!username) {
          response = construct_response(
              400, "{\"error\": \"Missing required field: username.\"}");
          send(new_socket, response, strlen(response), 0);
          continue;
        }
        cJSON *email = cJSON_GetObjectItem(json, "email");
        if (!email) {
          response = construct_response(
              400, "{\"error\": \"Missing required field: email.\"}");
          send(new_socket, response, strlen(response), 0);
          continue;
        }
        cJSON *password = cJSON_GetObjectItem(json, "password");
        if (!password) {
          response = construct_response(
              400, "{\"error\": \"Missing required field: password.\"}");
          send(new_socket, response, strlen(response), 0);
          continue;
        }
        cJSON *profile_image = cJSON_GetObjectItem(json, "profile_image");
        if (!profile_image) {
          response = construct_response(
              400, "{\"error\": \"Missing required field: profile_image.\"}");
          send(new_socket, response, strlen(response), 0);
          continue;
        }

        char *hashed_password = crypt(password->valuestring, "salt");

        sprintf(create_user_format_sql, create_user_sql, username->valuestring,
                email->valuestring, hashed_password,
                profile_image->valuestring);

        db_request(db, create_user_format_sql, 0, 0, &err_msg, "Inserted user");

        response =
            construct_response(200, "{\"message\": \"User registered.\"}");
      }
    } else if (strcmp(path_base, "/login") == 0 &&
               strcmp(method, "POST") == 0) {
      // POST /login
      cJSON *json = cJSON_Parse(body);
      if (!json) {
        response = construct_response(
            400, "{\"error\": \"Failed to parse JSON request body.\"}");
      } else {
        cJSON *email = cJSON_GetObjectItem(json, "email");
        if (!email) {
          response = construct_response(
              400, "{\"error\": \"Missing required field: email.\"}");
          send(new_socket, response, strlen(response), 0);
          continue;
        }
        cJSON *password = cJSON_GetObjectItem(json, "password");
        if (!password) {
          response = construct_response(
              400, "{\"error\": \"Missing required field: password.\"}");
          send(new_socket, response, strlen(response), 0);
          continue;
        }

        char *hashed_password = crypt(password->valuestring, "salt");

        const char *login_sql =
            "SELECT * FROM Users WHERE email = '%s' AND password = '%s';";
        char *login_format_sql = malloc(strlen(login_sql) + 200);
        sprintf(login_format_sql, login_sql, email->valuestring,
                hashed_password);

        cJSON *json = cJSON_CreateObject();
        if (!json) {
          fprintf(stderr, "ERROR: Failed to create JSON object.\n");
          return 0;
        }
        db_request(db, login_format_sql, callback_object, json, &err_msg,
                   "Fetched user by email and password");

        int response_code = 200;
        if (cJSON_GetArraySize(json) == 0) {
          response_code = 404;
          cJSON_AddStringToObject(json, "error", "User not found.");
        }

        char *json_string = cJSON_PrintUnformatted(json);

        if (json_string) {
          response = construct_response(response_code, json_string);
          free(json_string);
        } else {
          response = construct_response(
              500, "{\"error\": \"Failed to serialize JSON.\"}");
        }

        cJSON_Delete(json);
        free(login_format_sql);
      }
    } else {
      printf("404 Not Found\n");
      response = construct_response(404, "{\"error\": \"Not Found.\"}");
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
