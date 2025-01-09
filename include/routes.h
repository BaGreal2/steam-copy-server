#pragma once

#include "cJSON.h"
#include <sqlite3.h>

void request_get_games(sqlite3 *db, char **response, char **err_msg);
void request_get_game_by_id(sqlite3 *db, char *id, char **response, char **err_msg);
void request_post_game(sqlite3 *db, char *body, char **response, char **err_msg, int socket);
void request_delete_game_by_id(sqlite3 *db, char *id, char **response, char **err_msg);
void request_patch_game_by_id(sqlite3 *db, char *id, char *body, char **response, char **err_msg);

void request_post_register(sqlite3 *db, char *body, char **response, char **err_msg, int socket);
void request_post_login(sqlite3 *db, char *body, char **response, char **err_msg, int socket);
