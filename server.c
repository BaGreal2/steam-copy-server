#include <arpa/inet.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024

// Callback function for SELECT queries
int callback(void *unused, int argc, char **argv, char **colName)
{
  for (int i = 0; i < argc; i++) {
    printf("%s = %s\n", colName[i], argv[i] ? argv[i] : "NULL");
  }
  printf("\n");
  return 0;
}

const char *extract_path(const char *request)
{
  const char *path_start = strchr(request, ' ') + 1;
  const char *path_end = strchr(path_start, ' ');
  int path_length = path_end - path_start;
  char *path = malloc(path_length + 1);
  strncpy(path, path_start, path_length);
  path[path_length] = '\0';
  return path;
}

const char *construct_response(const char *body)
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
  char *errMsg = 0;
  int rc;

  rc = sqlite3_open("steam.db", &db);
  if (rc) {
    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
    return 1;
  }
  printf("Opened database successfully.\n");

  // create tables...
  // insert data...

  int server_fd, new_socket;
  struct sockaddr_in address;
  int addrlen = sizeof(address);
  char buffer[BUFFER_SIZE] = {0};

  // Step 1: Create a socket
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == 0) {
    perror("Socket failed");
    exit(EXIT_FAILURE);
  }

  // Step 2: Bind the socket to an address and port
  address.sin_family = AF_INET;         // IPv4
  address.sin_addr.s_addr = INADDR_ANY; // Accept connections from any IP
  address.sin_port = htons(PORT);       // Convert port to network byte order

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("Bind failed");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  // Step 3: Listen for incoming connections
  if (listen(server_fd, 3) < 0) {
    perror("Listen failed");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  printf("HTTP server is running on port %d\n", PORT);

  // Step 4: Accept a connection and handle requests
  while (1) {
    new_socket =
        accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
    if (new_socket < 0) {
      perror("Accept failed");
      continue;
    }

    // Step 5: Read the HTTP request
    read(new_socket, buffer, BUFFER_SIZE);
    printf("Request:\n%s\n", buffer);

    // Step 6: Send an HTTP response
    printf("Path:\n%s\n", extract_path(buffer));
    const char *path = extract_path(buffer);

    if (strcmp(path, "/") == 0) {
      const char *response = construct_response("Hello, world!");
      send(new_socket, response, strlen(response), 0);
    } else if (strcmp(path, "/example") == 0) {
      const char *response = construct_response("Example!");
      send(new_socket, response, strlen(response), 0);
    } else {
      printf("404 Not Found\n");
      send(new_socket, "HTTP/1.1 404 Not Found\r\n", 24, 0);
    }

    printf("Response sent.\n");

    // Close the connection
    close(new_socket);
  }

  // Cleanup
  close(server_fd);
  return 0;
}
