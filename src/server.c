#include "db.h"
#include "defines.h"
#include "http.h"
#include <arpa/inet.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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

  init_tables(db, &err_msg);

  int server_fd, new_socket;
  struct sockaddr_in address;
  int addrlen = sizeof(address);
  char buffer[BUFFER_SIZE] = {0};

  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == 0) {
    perror("ERROR: Socket failed");
    exit(EXIT_FAILURE);
  }

  // Bind the socket to an address and port
  address.sin_family = AF_INET;         // IPv4
  address.sin_addr.s_addr = INADDR_ANY; // Accept connections from any IP
  address.sin_port = htons(PORT);       // Convert port to network byte order

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("ERROR: Bind failed");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd, 3) < 0) {
    perror("ERROR: Listen failed");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  printf("HTTP server is running on port %d\n", PORT);

  while (1) {
    new_socket =
        accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
    if (new_socket < 0) {
      perror("ERROR: Accept failed");
      continue;
    }

    handle_request(db, &err_msg, buffer, new_socket);

    close(new_socket);
  }

  close(server_fd);
  return 0;
}
