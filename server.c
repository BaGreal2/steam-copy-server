#include <arpa/inet.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024

typedef struct {
  const char *name;
  const char *sql;
} TableSchema;

// Callback function for SELECT queries
int callback(void *unused, int argc, char **argv, char **colName)
{
  for (int i = 0; i < argc; i++) {
    printf("%s = %s\n", colName[i], argv[i] ? argv[i] : "NULL");
  }
  printf("\n");
  return 0;
}

void db_request(sqlite3 *db, const char *sql,
                int (*callback)(void *, int, char **, char **), void *data,
                char **err_msg)
{
  int rc = sqlite3_exec(db, sql, callback, data, err_msg);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "ERROR: Failed to execute SQL: %s\n", *err_msg);
    sqlite3_free(*err_msg);
    *err_msg = NULL;
  } else {
    printf("SQL query executed successfully.\n");
  }
}

char *extract_path(const char *request)
{
  const char *path_start = strchr(request, ' ') + 1;
  const char *path_end = strchr(path_start, ' ');

  int path_length = path_end - path_start;
  char *path = malloc(path_length + 1);
  strncpy(path, path_start, path_length);
  path[path_length] = '\0';
  return path;
}

char *extract_method(const char *request)
{
  const char *method_end = strchr(request, ' ');

  int method_length = method_end - request;
  char *method = malloc(method_length + 1);
  strncpy(method, request, method_length);
  method[method_length] = '\0';
  return method;
}

char *construct_response(const char *body)
{
  char *response = malloc(strlen(body) + 100);
  sprintf(response,
          "HTTP/1.1 200 OK\r\n"
          "Content-Type: text/plain\r\n"
          "Content-Length: %ld\r\n"
          "\r\n"
          "%s",
          strlen(body), body);

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

  TableSchema tables[] = {
      {"Users", "CREATE TABLE IF NOT EXISTS Users("
                "user_id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "username TEXT NOT NULL UNIQUE, "
                "email TEXT NOT NULL UNIQUE, "
                "created_at DATETIME DEFAULT CURRENT_TIMESTAMP);"},
      {"Games", "CREATE TABLE IF NOT EXISTS Games("
                "game_id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "title TEXT NOT NULL, "
                "genre TEXT NOT NULL, "
                "release_date DATE, "
                "developer TEXT);"},
      {"Libraries",
       "CREATE TABLE IF NOT EXISTS Libraries("
       "library_id INTEGER PRIMARY KEY AUTOINCREMENT, "
       "user_id INTEGER NOT NULL, "
       "game_id INTEGER NOT NULL, "
       "purchased_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
       "FOREIGN KEY (user_id) REFERENCES Users(user_id) ON DELETE CASCADE, "
       "FOREIGN KEY (game_id) REFERENCES Games(game_id) ON DELETE CASCADE, "
       "UNIQUE(user_id, game_id));"},
      {"Reviews",
       "CREATE TABLE IF NOT EXISTS Reviews("
       "review_id INTEGER PRIMARY KEY AUTOINCREMENT, "
       "user_id INTEGER NOT NULL, "
       "game_id INTEGER NOT NULL, "
       "rating INTEGER NOT NULL CHECK (rating >= 1 AND rating <=5), "
       "review_text TEXT, "
       "created_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
       "FOREIGN KEY (user_id) REFERENCES Users(user_id) ON DELETE CASCADE, "
       "FOREIGN KEY (game_id) REFERENCES Games(game_id) ON DELETE CASCADE);"}};

  for (size_t i = 0; i < sizeof(tables) / sizeof(tables[0]); i++) {
    rc = sqlite3_exec(db, tables[i].sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
      fprintf(stderr, "ERROR: Failed to create table %s: %s\n", tables[i].name,
              err_msg);
      sqlite3_free(err_msg);
    } else {
      printf("LOG: Table %s created successfully.\n", tables[i].name);
    }
  }

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
    printf("Path:\n%s\n", extract_path(buffer));
    printf("Method:\n%s\n", extract_method(buffer));

    const char *path = extract_path(buffer);
    const char *method = extract_method(buffer);

    char *response;

    if (strcmp(path, "/games") == 0 && strcmp(method, "GET") == 0) {
      const char *all_games_sql = "SELECT * FROM Users;";
      db_request(db, all_games_sql, callback, 0, &err_msg);
      response = construct_response("Games:");
    } else if (strcmp(path, "/example") == 0 && strcmp(method, "GET") == 0) {
      response = construct_response("Example!");
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
