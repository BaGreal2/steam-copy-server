#include "db.h"
#include "defines.h"
#include "http.h"
#include <arpa/inet.h>
#include <sqlite3.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

int server_fd = -1;

void handle_sigint(int sig) {
    if (server_fd != -1) {
        printf("\nLOG: Cleaning up and closing the server socket...\n");
        close(server_fd);
    }
    exit(0);
}

int main()
{
  sqlite3 *db;
  char *err_msg = 0;
  int rc;

  rc = sqlite3_open("steam.db", &db);
  if (rc) {
    fprintf(stderr, "ERROR: Can't open database: %s\n", sqlite3_errmsg(db));
    return 1;
  }
  printf("LOG: Opened database successfully.\n");

  init_tables(db, &err_msg);

  int new_socket;
  struct sockaddr_in address;
  int addrlen = sizeof(address);

  signal(SIGINT, handle_sigint);

  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == 0) {
    perror("ERROR: Socket failed");
    return 1;
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("ERROR: Bind failed");
    close(server_fd);
    return 1;
  }

  if (listen(server_fd, 3) < 0) {
    perror("ERROR: Listen failed");
    close(server_fd);
    return 1;
  }

  printf("HTTP server is running on port %d\n", PORT);

  while (1) {
    new_socket =
        accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
    if (new_socket < 0) {
      perror("ERROR: Accept failed");
      continue;
    }

    handle_request(db, &err_msg, new_socket);

    close(new_socket);
  }

  close(server_fd);
  return 0;
}
