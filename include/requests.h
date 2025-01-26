#pragma once

#include "cJSON.h"
#include "http.h"
#include <sqlite3.h>

cJSON *get_required_field(cJSON *json, const char *field_name, char **response, int socket);
void append_update(const char *field, cJSON *item, char *sql, int *has_updates);
char *format_sql_query(const char *temp, ...);
void handle_error(const char *message, char **response, int socket);
void construct_json_response(cJSON *json, int code, char **response);

void request_get_games(sqlite3 *db, char **response, char **err_msg);
void request_get_game_by_id(sqlite3 *db, char *id, char **response, char **err_msg);
void request_post_game(sqlite3 *db, char *body, char **response, char **err_msg, int socket);
void request_delete_game_by_id(sqlite3 *db, char *id, char **response, char **err_msg);
void request_patch_game_by_id(sqlite3 *db, char *id, char *body, char **response, char **err_msg);

void request_get_reviews_by_game_id(sqlite3 *db, char *id, char **response, char **err_msg);
void request_post_review(sqlite3 *db, char *id, char *body, char **response, char **err_msg, int socket);

void request_post_register(sqlite3 *db, char *body, char **response, char **err_msg, int socket);
void request_post_login(sqlite3 *db, char *body, char **response, char **err_msg, int socket);

void request_patch_user(sqlite3 *db, QueryParams *query, char *body, char **response, char **err_msg, int socket);

void request_get_my_games(sqlite3 *db, QueryParams *query, char **response, char **err_msg, int socket);
void request_post_my_game(sqlite3 *db, char *body, char **response, char **err_msg, int socket);
void request_delete_my_game(sqlite3 *db, char *id, QueryParams *query, char **response, char **err_msg, int socket);

void request_get_my_posted_games(sqlite3 *db, QueryParams *query, char **response, char **err_msg, int socket);

void request_get_achievement_by_id(sqlite3 *db, char *id, char **response, char **err_msg);
void request_post_achievement(sqlite3 *db, char *body, char **response, char **err_msg, int socket);
void request_patch_achievement_by_id(sqlite3 *db, char *id, char *body, char **response, char **err_msg);
void request_delete_achievement_by_id(sqlite3 *db, char *id, char **response, char **err_msg);
void request_get_achievements_by_game_id(sqlite3 *db, char *id, char **response, char **err_msg);

void request_get_user_achievements(sqlite3 *db, QueryParams *query, char **response, char **err_msg, int socket);
void request_post_user_achievement(sqlite3 *db, char *body, char **response, char **err_msg, int socket);
void request_get_user_achievements_by_game_id(sqlite3 *db, char *id, QueryParams *query, char **response, char **err_msg, int socket);
