#include "requests.h"
#include "cJSON.h"
#include "db.h"
#include "http.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

cJSON *get_required_field(cJSON *json, const char *field_name, char **response,
                          int socket)
{
  cJSON *field = cJSON_GetObjectItem(json, field_name);
  if (!field) {
    char error_message[256];
    sprintf(error_message, "{\"error\": \"Missing required field: %s.\"}",
            field_name);
    *response = construct_response(BAD_REQUEST, error_message);
    send(socket, *response, strlen(*response), 0);
    return NULL;
  }
  return field;
}

void append_update(const char *field, cJSON *item, char *sql, int *has_updates)
{
  if (item && cJSON_IsString(item)) {
    if (*has_updates) {
      strcat(sql, ", ");
    }
    strcat(sql, field);
    strcat(sql, " = '");
    strcat(sql, sqlite3_mprintf("%q", item->valuestring));
    strcat(sql, "'");
    (*has_updates)++;
  }
}

char *format_sql_query(const char *temp, ...)
{
  va_list args;
  va_start(args, temp);
  int query_size = vsnprintf(NULL, 0, temp, args) + 1;
  va_end(args);

  char *query = malloc(query_size);
  if (!query)
    return NULL;

  va_start(args, temp);
  vsnprintf(query, query_size, temp, args);
  va_end(args);

  return query;
}

void handle_error(const char *message, char **response, int socket)
{
  fprintf(stderr, "ERROR: %s\n", message);
  *response = construct_response(
      INTERNAL_SERVER_ERROR, "{\"error\": \"An internal error occurred.\"}");
  send(socket, *response, strlen(*response), 0);
}

void construct_json_response(cJSON *json, int code, char **response)
{
  char *json_string = cJSON_PrintUnformatted(json);
  if (json_string) {
    *response = construct_response(code, json_string);
    free(json_string);
  } else {
    *response = construct_response(
        INTERNAL_SERVER_ERROR, "{\"error\": \"Failed to serialize JSON.\"}");
  }
}

void request_get_games(sqlite3 *db, char **response, char **err_msg)
{
  const char *select_sql = "SELECT * FROM Games;";

  cJSON *json_array = cJSON_CreateArray();
  if (!json_array) {
    fprintf(stderr, "ERROR: Failed to create JSON array.\n");
    return;
  }

  db_request(db, select_sql, callback_array, json_array, err_msg,
             "Fetched all games");

  char *json_string = cJSON_PrintUnformatted(json_array);

  if (json_string) {
    *response = construct_response(SUCCESS, json_string);
    free(json_string);
  } else {
    *response = construct_response(
        INTERNAL_SERVER_ERROR, "{\"error\": \"Failed to serialize JSON.\"}");
  }

  cJSON_Delete(json_array);
}

void request_get_game_by_id(sqlite3 *db, char *id, char **response,
                            char **err_msg)
{
  char *select_sql =
      format_sql_query("SELECT * FROM Games WHERE game_id = %s;", id);

  cJSON *json = cJSON_CreateObject();
  if (!json) {
    fprintf(stderr, "ERROR: Failed to create JSON object.\n");
    return;
  }
  db_request(db, select_sql, callback_object, json, err_msg,
             "Fetched game by id");

  int response_code = SUCCESS;
  if (cJSON_GetArraySize(json) == 0) {
    response_code = NOT_FOUND;
    cJSON_AddStringToObject(json, "error", "Game not found.");
  }

  char *json_string = cJSON_PrintUnformatted(json);

  if (json_string) {
    *response = construct_response(response_code, json_string);
    free(json_string);
  } else {
    *response = construct_response(
        INTERNAL_SERVER_ERROR, "{\"error\": \"Failed to serialize JSON.\"}");
  }

  cJSON_Delete(json);
  free(select_sql);
}

void request_post_game(sqlite3 *db, char *body, char **response, char **err_msg,
                       int socket)
{
  cJSON *json = cJSON_Parse(body);
  if (!json) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Failed to parse JSON request body.\"}");
    return;
  }

  cJSON *added_by = get_required_field(json, "added_by", response, socket);
  cJSON *title = get_required_field(json, "title", response, socket);
  cJSON *description =
      get_required_field(json, "description", response, socket);
  cJSON *price = get_required_field(json, "price", response, socket);
  cJSON *genre = get_required_field(json, "genre", response, socket);
  cJSON *cover_image =
      get_required_field(json, "cover_image", response, socket);
  cJSON *icon_image = get_required_field(json, "icon_image", response, socket);
  cJSON *developer = get_required_field(json, "developer", response, socket);

  if (!added_by || !title || !genre || !cover_image || !icon_image ||
      !developer) {
    cJSON_Delete(json);
    return;
  }

  char *insert_sql = format_sql_query(
      "INSERT INTO Games (added_by, title, description, price, genre, "
      "cover_image, "
      "icon_image, developer) "
      "VALUES ('%d','%s', '%s', '%s', '%s', '%s', '%s', '%s');",
      added_by->valueint, title->valuestring, description->valuestring,
      price->valuestring, genre->valuestring, cover_image->valuestring,
      icon_image->valuestring, developer->valuestring);

  if (!insert_sql) {
    handle_error("Failed to format SQL query.", response, socket);
    cJSON_Delete(json);
    return;
  }

  db_request(db, insert_sql, 0, 0, err_msg, "Inserted game");

  *response = construct_response(SUCCESS, "{\"message\": \"Game inserted.\"}");

  free(insert_sql);
  cJSON_Delete(json);
}

void request_delete_game_by_id(sqlite3 *db, char *id, char **response,
                               char **err_msg)
{
  char *delete_sql =
      format_sql_query("DELETE FROM Games WHERE game_id = %s;", id);

  db_request(db, delete_sql, 0, 0, err_msg, "Deleted game by id");

  *response = construct_response(SUCCESS, "{\"message\": \"Game deleted.\"}");
  free(delete_sql);
}

void request_patch_game_by_id(sqlite3 *db, char *id, char *body,
                              char **response, char **err_msg)
{
  cJSON *json = cJSON_Parse(body);
  if (!json) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Failed to parse JSON request body.\"}");
    return;
  }

  const char *fields[] = {"title",      "genre",        "cover_image",
                          "icon_image", "release_date", "developer"};
  char *sql = malloc(1024);
  strcpy(sql, "UPDATE Games SET ");

  int has_updates = 0;
  for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
    append_update(fields[i], cJSON_GetObjectItem(json, fields[i]), sql,
                  &has_updates);
  }

  if (!has_updates) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"No fields provided to update.\"}");
    free(sql);
    cJSON_Delete(json);
    return;
  }

  strcat(sql, " WHERE game_id = ");
  strcat(sql, sqlite3_mprintf("%q", id));
  strcat(sql, ";");

  if (sqlite3_exec(db, sql, NULL, NULL, err_msg) != SQLITE_OK) {
    *response =
        construct_response(INTERNAL_SERVER_ERROR,
                           "{\"error\": \"Failed to execute SQL update.\"}");
  } else {
    *response = construct_response(SUCCESS, "{\"message\": \"Game updated.\"}");
  }

  free(sql);
  cJSON_Delete(json);
}

void request_post_register(sqlite3 *db, char *body, char **response,
                           char **err_msg, int socket)
{
  cJSON *json = cJSON_Parse(body);
  if (!json) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Failed to parse JSON request body.\"}");
    return;
  }

  cJSON *username = get_required_field(json, "username", response, socket);
  cJSON *email = get_required_field(json, "email", response, socket);
  cJSON *password = get_required_field(json, "password", response, socket);
  cJSON *profile_image =
      get_required_field(json, "profile_image", response, socket);

  if (!username || !email || !password || !profile_image) {
    cJSON_Delete(json);
    return;
  }

  char *hashed_password = crypt(password->valuestring, "salt");

  char *insert_sql = format_sql_query(
      "INSERT INTO Users (username, email, password, profile_image) "
      "VALUES ('%s', '%s', '%s', '%s');",
      username->valuestring, email->valuestring, hashed_password,
      profile_image->valuestring);

  if (!insert_sql) {
    handle_error("Failed to format SQL query.", response, socket);
    cJSON_Delete(json);
    return;
  }

  db_request(db, insert_sql, 0, 0, err_msg, "Inserted user");

  char *login_sql = format_sql_query(
      "SELECT * FROM Users WHERE username = '%s' AND password = '%s';",
      username->valuestring, hashed_password);

  if (!login_sql) {
    handle_error("Failed to format SQL query.", response, socket);
    cJSON_Delete(json);
    return;
  }

  cJSON *json_response = cJSON_CreateObject();
  if (!json_response) {
    fprintf(stderr, "ERROR: Failed to create JSON object.\n");
    return;
  }

  db_request(db, login_sql, callback_object, json_response, err_msg,
             "Fetched user by username and password");

  int response_code = SUCCESS;
  if (cJSON_GetArraySize(json_response) == 0) {
    response_code = INTERNAL_SERVER_ERROR;
    cJSON_AddStringToObject(json_response, "error", "Failed to create a user.");
  }

  cJSON_DeleteItemFromObject(json_response, "password");
  char *json_string = cJSON_PrintUnformatted(json_response);

  if (json_string) {
    *response = construct_response(response_code, json_string);
    // free(json_string);
  } else {
    *response = construct_response(
        INTERNAL_SERVER_ERROR, "{\"error\": \"Failed to serialize JSON.\"}");
  }

  cJSON_Delete(json_response);
  cJSON_Delete(json);
  // free(login_sql);
  free(insert_sql);
}

void request_post_login(sqlite3 *db, char *body, char **response,
                        char **err_msg, int socket)
{
  cJSON *json = cJSON_Parse(body);
  if (!json) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Failed to parse JSON request body.\"}");
    return;
  }

  cJSON *username = get_required_field(json, "username", response, socket);
  cJSON *password = get_required_field(json, "password", response, socket);

  if (!username || !password) {
    cJSON_Delete(json);
    return;
  }

  char *hashed_password = crypt(password->valuestring, "salt");

  char *login_sql = format_sql_query(
      "SELECT * FROM Users WHERE username = '%s' AND password = '%s';",
      username->valuestring, hashed_password);

  if (!login_sql) {
    handle_error("Failed to format SQL query.", response, socket);
    cJSON_Delete(json);
    return;
  }

  cJSON *json_response = cJSON_CreateObject();
  if (!json_response) {
    fprintf(stderr, "ERROR: Failed to create JSON object.\n");
    return;
  }

  db_request(db, login_sql, callback_object, json_response, err_msg,
             "Fetched user by username and password");

  int response_code = SUCCESS;
  if (cJSON_GetArraySize(json_response) == 0) {
    response_code = NOT_FOUND;
    cJSON_AddStringToObject(json_response, "error", "User not found.");
  }

  cJSON_DeleteItemFromObject(json_response, "password");

  char *json_string = cJSON_PrintUnformatted(json_response);

  if (json_string) {
    *response = construct_response(response_code, json_string);
    free(json_string);
  } else {
    *response = construct_response(
        INTERNAL_SERVER_ERROR, "{\"error\": \"Failed to serialize JSON.\"}");
  }

  cJSON_Delete(json);
  cJSON_Delete(json_response);
  free(login_sql);
}

void request_get_reviews_by_game_id(sqlite3 *db, char *id, char **response,
                                    char **err_msg)
{
  char *select_sql = format_sql_query(
      "SELECT Reviews.review_id, Reviews.game_id, Reviews.rating, "
      "Reviews.review_text, Reviews.created_at, Users.username "
      "FROM Reviews "
      "INNER JOIN Users ON Reviews.user_id = Users.user_id "
      "WHERE Reviews.game_id = %s;",
      id);

  cJSON *json = cJSON_CreateArray();
  if (!json) {
    fprintf(stderr, "ERROR: Failed to create JSON object.\n");
    return;
  }

  db_request(db, select_sql, callback_array, json, err_msg,
             "Fetched review by id");

  int response_code = SUCCESS;
  if (cJSON_GetArraySize(json) == 0) {
    response_code = NOT_FOUND;
    cJSON_AddStringToObject(json, "error", "Review not found.");
  }

  char *json_string = cJSON_PrintUnformatted(json);

  if (json_string) {
    *response = construct_response(response_code, json_string);
    free(json_string);
  } else {
    *response = construct_response(
        INTERNAL_SERVER_ERROR, "{\"error\": \"Failed to serialize JSON.\"}");
  }

  cJSON_Delete(json);
  free(select_sql);
}

void request_post_review(sqlite3 *db, char *id, char *body, char **response,
                         char **err_msg, int socket)
{
  cJSON *json = cJSON_Parse(body);
  if (!json) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Failed to parse JSON request body.\"}");
    return;
  }

  cJSON *user_id = get_required_field(json, "user_id", response, socket);
  cJSON *rating = get_required_field(json, "rating", response, socket);
  cJSON *review_text =
      get_required_field(json, "review_text", response, socket);

  char *insert_sql = format_sql_query("INSERT INTO Reviews (user_id, game_id, "
                                      "rating, review_text) "
                                      "VALUES ('%d', '%s', '%d', '%s');",
                                      user_id->valueint, id, rating->valueint,
                                      review_text->valuestring);

  if (!insert_sql) {
    handle_error("Failed to format SQL query.", response, socket);
    cJSON_Delete(json);
    return;
  }

  db_request(db, insert_sql, 0, 0, err_msg, "Inserted review");

  *response =
      construct_response(SUCCESS, "{\"message\": \"Review inserted.\"}");

  cJSON_Delete(json);
  free(insert_sql);
}

void request_get_my_games(sqlite3 *db, QueryParams *query, char **response,
                          char **err_msg, int socket)

{

  char *user_id = query->values[0];
  if (!user_id || strcmp(query->keys[0], "user_id") != 0) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Missing required query: user_id.\"}");
    send(socket, response, strlen(*response), 0);
    return;
  }

  char *select_sql =
      format_sql_query("SELECT Games.* "
                       "FROM Libraries "
                       "INNER JOIN Games ON Libraries.game_id = Games.game_id "
                       "WHERE Libraries.user_id = %s;",
                       user_id);

  if (!select_sql) {
    handle_error("Failed to format SQL query.", response, socket);
    return;
  }

  cJSON *json_array = cJSON_CreateArray();
  if (!json_array) {
    fprintf(stderr, "ERROR: Failed to create JSON array.\n");
    return;
  }

  db_request(db, select_sql, callback_array, json_array, err_msg,
             "Fetched library games");

  char *json_string = cJSON_PrintUnformatted(json_array);

  if (json_string) {
    *response = construct_response(SUCCESS, json_string);
    free(json_string);
  } else {
    *response = construct_response(
        INTERNAL_SERVER_ERROR, "{\"error\": \"Failed to serialize JSON.\"}");
  }

  cJSON_Delete(json_array);
  free(select_sql);
}

void request_post_my_game(sqlite3 *db, char *body, char **response,
                          char **err_msg, int socket)

{
  cJSON *json = cJSON_Parse(body);
  if (!json) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Failed to parse JSON request body.\"}");
    return;
  }

  cJSON *user_id = get_required_field(json, "user_id", response, socket);
  cJSON *game_id = get_required_field(json, "game_id", response, socket);

  if (!user_id || !game_id) {
    cJSON_Delete(json);
    return;
  }

  char *insert_sql =
      format_sql_query("INSERT INTO Libraries (user_id, game_id) "
                       "VALUES ('%d', '%d');",
                       user_id->valueint, game_id->valueint);

  if (!insert_sql) {
    handle_error("Failed to format SQL query.", response, socket);
    cJSON_Delete(json);
    return;
  }

  db_request(db, insert_sql, callback_array, 0, err_msg,
             "Inserted game into library");

  *response = construct_response(
      SUCCESS, "{\"message\": \"Game inserted to library.\"}");

  cJSON_Delete(json);
  free(insert_sql);
}

void request_delete_my_game(sqlite3 *db, char *id, QueryParams *query,
                            char **response, char **err_msg, int socket)
{
  char *user_id = query->values[0];
  if (!user_id || strcmp(query->keys[0], "user_id") != 0) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Missing required query: user_id.\"}");
    send(socket, response, strlen(*response), 0);
    return;
  }

  char *delete_sql = format_sql_query(
      "DELETE FROM Libraries WHERE game_id = %s AND user_id = %s;", id,
      user_id);

  if (!delete_sql) {
    handle_error("Failed to format SQL query.", response, socket);
    return;
  }

  db_request(db, delete_sql, callback_array, 0, err_msg,
             "Deleted game from library");

  *response = construct_response(
      SUCCESS, "{\"message\": \"Game deleted from library.\"}");

  free(delete_sql);
}

void request_get_achievement_by_id(sqlite3 *db, char *id, char **response,
                                   char **err_msg)
{
  char *select_sql = format_sql_query(
      "SELECT * FROM Achievements WHERE achievement_id = %s;", id);

  cJSON *json = cJSON_CreateObject();
  if (!json) {
    fprintf(stderr, "ERROR: Failed to create JSON object.\n");
    return;
  }

  db_request(db, select_sql, callback_object, json, err_msg,
             "Fetched achievement by id");

  int response_code = SUCCESS;
  if (cJSON_GetArraySize(json) == 0) {
    response_code = NOT_FOUND;
    cJSON_AddStringToObject(json, "error", "Achievement not found.");
  }

  char *json_string = cJSON_PrintUnformatted(json);

  if (json_string) {
    *response = construct_response(response_code, json_string);
    free(json_string);
  } else {
    *response = construct_response(
        INTERNAL_SERVER_ERROR, "{\"error\": \"Failed to serialize JSON.\"}");
  }

  cJSON_Delete(json);
  free(select_sql);
}

void request_get_my_posted_games(sqlite3 *db, QueryParams *query,
                                 char **response, char **err_msg, int socket)
{
  char *user_id = query->values[0];
  if (!user_id || strcmp(query->keys[0], "user_id") != 0) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Missing required query: user_id.\"}");
    send(socket, response, strlen(*response), 0);
    return;
  }

  char *select_sql = format_sql_query("SELECT Games.* "
                                      "FROM Games "
                                      "WHERE Games.added_by = %s;",
                                      user_id);

  if (!select_sql) {
    handle_error("Failed to format SQL query.", response, socket);
    return;
  }

  cJSON *json_array = cJSON_CreateArray();
  if (!json_array) {
    fprintf(stderr, "ERROR: Failed to create JSON array.\n");
    return;
  }

  db_request(db, select_sql, callback_array, json_array, err_msg,
             "Fetched posted games");

  char *json_string = cJSON_PrintUnformatted(json_array);

  if (json_string) {
    *response = construct_response(SUCCESS, json_string);
    free(json_string);
  } else {
    *response = construct_response(
        INTERNAL_SERVER_ERROR, "{\"error\": \"Failed to serialize JSON.\"}");
  }

  cJSON_Delete(json_array);
  free(select_sql);
}

void request_post_achievement(sqlite3 *db, char *body, char **response,
                              char **err_msg, int socket)
{
  cJSON *json = cJSON_Parse(body);
  if (!json) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Failed to parse JSON request body.\"}");
    return;
  }

  cJSON *game_id = get_required_field(json, "game_id", response, socket);
  cJSON *name = get_required_field(json, "name", response, socket);
  cJSON *description =
      get_required_field(json, "description", response, socket);
  cJSON *points = get_required_field(json, "points", response, socket);

  if (!game_id || !name || !description || !points) {
    cJSON_Delete(json);
    return;
  }

  char *insert_sql =
      format_sql_query("INSERT INTO Achievements (game_id, "
                       "name, description, points) "
                       "VALUES ('%d', '%s', '%s', '%d');",
                       game_id->valueint, name->valuestring,
                       description->valuestring, points->valueint);

  if (!insert_sql) {
    handle_error("Failed to format SQL query.", response, socket);
    cJSON_Delete(json);
    return;
  }

  db_request(db, insert_sql, 0, 0, err_msg, "Inserted achievement");

  *response =
      construct_response(SUCCESS, "{\"message\": \"Achievement inserted.\"}");

  cJSON_Delete(json);
  free(insert_sql);
}

void request_patch_achievement_by_id(sqlite3 *db, char *id, char *body,
                                     char **response, char **err_msg)
{
  cJSON *json = cJSON_Parse(body);
  if (!json) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Failed to parse JSON request body.\"}");
    return;
  }

  const char *fields[] = {"name", "description", "points"};
  char *sql = malloc(1024);
  strcpy(sql, "UPDATE Achievements SET ");

  int has_updates = 0;
  for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
    append_update(fields[i], cJSON_GetObjectItem(json, fields[i]), sql,
                  &has_updates);
  }

  if (!has_updates) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"No fields provided to update.\"}");
    free(sql);
    cJSON_Delete(json);
    return;
  }

  strcat(sql, " WHERE achievement_id = ");
  strcat(sql, sqlite3_mprintf("%q", id));
  strcat(sql, ";");

  if (sqlite3_exec(db, sql, NULL, NULL, err_msg) != SQLITE_OK) {
    *response =
        construct_response(INTERNAL_SERVER_ERROR,
                           "{\"error\": \"Failed to execute SQL update.\"}");
  } else {
    *response = construct_response(SUCCESS, "{\"message\": \"Game updated.\"}");
  }

  free(sql);
  cJSON_Delete(json);
}

void request_delete_achievement_by_id(sqlite3 *db, char *id, char **response,
                                      char **err_msg)
{
  char *delete_sql = format_sql_query(
      "DELETE FROM Achievements WHERE achievement_id = %s;", id);

  db_request(db, delete_sql, 0, 0, err_msg, "Deleted achievement by id");

  *response =
      construct_response(SUCCESS, "{\"message\": \"Achievement deleted.\"}");

  free(delete_sql);
}

void request_get_achievements_by_game_id(sqlite3 *db, char *id, char **response,
                                         char **err_msg)
{
  char *select_sql =
      format_sql_query("SELECT * FROM Achievements WHERE game_id = %s;", id);

  cJSON *json = cJSON_CreateArray();
  if (!json) {
    fprintf(stderr, "ERROR: Failed to create JSON object.\n");
    return;
  }

  db_request(db, select_sql, callback_array, json, err_msg,
             "Fetched achievements by game id");

  int response_code = SUCCESS;
  if (cJSON_GetArraySize(json) == 0) {
    response_code = NOT_FOUND;
    cJSON_AddStringToObject(json, "error", "Achievements not found.");
  }

  char *json_string = cJSON_PrintUnformatted(json);

  if (json_string) {
    *response = construct_response(response_code, json_string);
    free(json_string);
  } else {
    *response = construct_response(
        INTERNAL_SERVER_ERROR, "{\"error\": \"Failed to serialize JSON.\"}");
  }

  cJSON_Delete(json);
  free(select_sql);
}

void request_get_user_achievements(sqlite3 *db, QueryParams *query,
                                   char **response, char **err_msg, int socket)
{
  char *user_id = query->values[0];
  if (!user_id || strcmp(query->keys[0], "user_id") != 0) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Missing required query: user_id.\"}");
    send(socket, response, strlen(*response), 0);
    return;
  }

  char *select_sql = format_sql_query(
      "SELECT Achievements.* "
      "FROM Achievements "
      "INNER JOIN User_Achievements ON Achievements.achievement_id = "
      "User_Achievements.achievement_id "
      "WHERE User_Achievements.user_id = %d;",
      user_id);

  if (!select_sql) {
    handle_error("Failed to format SQL query.", response, socket);
    return;
  }

  cJSON *json = cJSON_CreateArray();
  if (!json) {
    fprintf(stderr, "ERROR: Failed to create JSON object.\n");
    return;
  }
  db_request(db, select_sql, callback_array, json, err_msg,
             "Fetched user achievements");

  int response_code = SUCCESS;
  if (cJSON_GetArraySize(json) == 0) {
    response_code = NOT_FOUND;
    cJSON_AddStringToObject(json, "error", "Achievements not found.");
  }

  char *json_string = cJSON_PrintUnformatted(json);

  if (json_string) {
    *response = construct_response(response_code, json_string);
    free(json_string);
  } else {
    *response = construct_response(
        INTERNAL_SERVER_ERROR, "{\"error\": \"Failed to serialize JSON.\"}");
  }

  cJSON_Delete(json);
  free(select_sql);
}

void request_post_user_achievement(sqlite3 *db, char *body, char **response,
                                   char **err_msg, int socket)
{
  cJSON *json = cJSON_Parse(body);
  if (!json) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Failed to parse JSON request body.\"}");
    return;
  }

  cJSON *user_id = get_required_field(json, "user_id", response, socket);
  cJSON *achievement_id =
      get_required_field(json, "achievement_id", response, socket);

  if (!user_id || !achievement_id) {
    cJSON_Delete(json);
    return;
  }

  char *insert_sql =
      format_sql_query("INSERT INTO User_Achievements (user_id, "
                       "achievement_id) "
                       "VALUES ('%d', '%d');",
                       user_id->valueint, achievement_id->valueint);

  if (!insert_sql) {
    handle_error("Failed to format SQL query.", response, socket);
    cJSON_Delete(json);
    return;
  }

  db_request(db, insert_sql, 0, 0, err_msg, "Inserted user achievement");

  *response = construct_response(
      SUCCESS, "{\"message\": \"User achievement inserted.\"}");

  cJSON_Delete(json);
  free(insert_sql);
}

void request_patch_user(sqlite3 *db, QueryParams *query, char *body,
                        char **response, char **err_msg, int socket)
{
  cJSON *json = cJSON_Parse(body);
  if (!json) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Failed to parse JSON request body.\"}");
    return;
  }

  char *user_id = query->values[0];
  if (!user_id || strcmp(query->keys[0], "user_id") != 0) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Missing required query: user_id.\"}");
    send(socket, response, strlen(*response), 0);
    return;
  }

  const char *fields[] = {"username", "email", "profile_image"};
  char *sql = malloc(1024);
  strcpy(sql, "UPDATE Users SET ");

  int has_updates = 0;
  for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
    append_update(fields[i], cJSON_GetObjectItem(json, fields[i]), sql,
                  &has_updates);
  }

  if (!has_updates) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"No fields provided to update.\"}");
    free(sql);
    cJSON_Delete(json);
    return;
  }

  strcat(sql, " WHERE user_id = ");
  strcat(sql, sqlite3_mprintf("%q", user_id));
  strcat(sql, ";");

  printf("SQL: %s\n", sql);

  if (sqlite3_exec(db, sql, NULL, NULL, err_msg) != SQLITE_OK) {
    *response =
        construct_response(INTERNAL_SERVER_ERROR,
                           "{\"error\": \"Failed to execute SQL update.\"}");
  } else {
    // return user after patch
    char *select_sql =
        format_sql_query("SELECT * FROM Users WHERE user_id = %s;", user_id);

    if (!select_sql) {
      handle_error("Failed to format SQL query.", response, socket);
      cJSON_Delete(json);
      return;
    }

    cJSON *json_response = cJSON_CreateObject();
    if (!json_response) {
      fprintf(stderr, "ERROR: Failed to create JSON object.\n");
      return;
    }

    db_request(db, select_sql, callback_object, json_response, err_msg,
               "Fetched user by user_id");

    int response_code = SUCCESS;
    if (cJSON_GetArraySize(json_response) == 0) {
      response_code = INTERNAL_SERVER_ERROR;
      cJSON_AddStringToObject(json_response, "error",
                              "Failed to fetch a user.");
    }

    cJSON_DeleteItemFromObject(json_response, "password");
    char *json_string = cJSON_PrintUnformatted(json_response);

    if (json_string) {
      *response = construct_response(response_code, json_string);
      free(json_string);
    } else {
      *response = construct_response(
          INTERNAL_SERVER_ERROR, "{\"error\": \"Failed to serialize JSON.\"}");
    }
  }

  free(sql);
  cJSON_Delete(json);
}

void request_get_user_achievements_by_game_id(sqlite3 *db, char *id,
                                              QueryParams *query,
                                              char **response, char **err_msg,
                                              int socket)
{
  char *user_id = query->values[0];
  if (!user_id || strcmp(query->keys[0], "user_id") != 0) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Missing required query: user_id.\"}");
    send(socket, response, strlen(*response), 0);
    return;
  }

  char *select_sql = format_sql_query(
      "SELECT Achievements.* "
      "FROM Achievements "
      "INNER JOIN User_Achievements ON Achievements.achievement_id = "
      "User_Achievements.achievement_id "
      "WHERE Achievements.game_id = %s AND User_Achievements.user_id = %s;",
      id, user_id);

  if (!select_sql) {
    handle_error("Failed to format SQL query.", response, socket);
    return;
  }

  cJSON *json = cJSON_CreateArray();
  if (!json) {
    fprintf(stderr, "ERROR: Failed to create JSON object.\n");
    return;
  }

  db_request(db, select_sql, callback_array, json, err_msg,
             "Fetched user achievements by game id");

  int response_code = SUCCESS;
  if (cJSON_GetArraySize(json) == 0) {
    response_code = NOT_FOUND;
    cJSON_AddStringToObject(json, "error", "Achievements not found.");
  }

  char *json_string = cJSON_PrintUnformatted(json);

  if (json_string) {
    *response = construct_response(response_code, json_string);
    free(json_string);
  } else {
    *response = construct_response(
        INTERNAL_SERVER_ERROR, "{\"error\": \"Failed to serialize JSON.\"}");
  }

  cJSON_Delete(json);
  free(select_sql);
}
