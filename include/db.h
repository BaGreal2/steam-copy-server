#pragma once

#include <sqlite3.h>

void db_request(sqlite3 *db, const char *sql,
                int (*callback)(void *, int, char **, char **), void *data,
                char **err_msg, void *description);

int callback_object(void *buffer, int argc, char **argv, char **colName);
int callback_array(void *buffer, int argc, char **argv, char **colName);

void init_tables(sqlite3 *db, char **err_msg);
