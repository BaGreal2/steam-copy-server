#include "routes.h"
#include "db.h"
#include "http.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void request_get_games(sqlite3 *db, char **response, char **err_msg)
{
  const char *all_games_sql = "SELECT * FROM Games;";

  cJSON *json_array = cJSON_CreateArray();
  if (!json_array) {
    fprintf(stderr, "ERROR: Failed to create JSON array.\n");
    return;
  }
  db_request(db, all_games_sql, callback_array, json_array, err_msg,
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
  const char *game_by_id_sql = "SELECT * FROM Games WHERE game_id = %s;";
  char *game_by_id_format_sql = malloc(strlen(game_by_id_sql) + strlen(id) + 1);
  sprintf(game_by_id_format_sql, game_by_id_sql, id);

  cJSON *json = cJSON_CreateObject();
  if (!json) {
    fprintf(stderr, "ERROR: Failed to create JSON object.\n");
    return;
  }
  db_request(db, game_by_id_format_sql, callback_object, json, err_msg,
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
    *response =
        construct_response(INTERNAL_SERVER_ERROR, "{\"error\": \"Failed to serialize JSON.\"}");
  }

  cJSON_Delete(json);
  free(game_by_id_format_sql);
}

void request_post_game(sqlite3 *db, char *body, char **response, char **err_msg,
                       int socket)
{
  const char *insert_game_sql = "INSERT INTO Games (title, genre, "
                                "cover_image, icon_image, developer) "
                                "VALUES ('%s', '%s', '%s', '%s', '%s');";
  char *insert_game_format_sql = malloc(strlen(insert_game_sql) + SUCCESS);

  cJSON *json = cJSON_Parse(body);
  if (!json) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Failed to parse JSON request body.\"}");
  } else {
    cJSON *title = cJSON_GetObjectItem(json, "title");
    if (!title) {
      *response = construct_response(
          BAD_REQUEST, "{\"error\": \"Missing required field: title.\"}");
      send(socket, response, strlen(*response), 0);
      return;
    }
    cJSON *genre = cJSON_GetObjectItem(json, "genre");
    if (!genre) {
      *response = construct_response(
          BAD_REQUEST, "{\"error\": \"Missing required field: genre.\"}");
      send(socket, response, strlen(*response), 0);
      return;
    }
    cJSON *cover_image = cJSON_GetObjectItem(json, "cover_image");
    if (!cover_image) {
      *response = construct_response(
          BAD_REQUEST, "{\"error\": \"Missing required field: cover_image.\"}");
      send(socket, response, strlen(*response), 0);
      return;
    }
    cJSON *icon_image = cJSON_GetObjectItem(json, "icon_image");
    if (!icon_image) {
      *response = construct_response(
          BAD_REQUEST, "{\"error\": \"Missing required field: icon_image.\"}");
      send(socket, response, strlen(*response), 0);
      return;
    }
    cJSON *developer = cJSON_GetObjectItem(json, "developer");
    if (!developer) {
      *response = construct_response(
          BAD_REQUEST, "{\"error\": \"Missing required field: developer.\"}");
      send(socket, response, strlen(*response), 0);
      return;
    }

    sprintf(insert_game_format_sql, insert_game_sql, title->valuestring,
            genre->valuestring, cover_image->valuestring,
            icon_image->valuestring, developer->valuestring);

    db_request(db, insert_game_format_sql, 0, 0, err_msg, "Inserted game");

    *response =
        construct_response(SUCCESS, "{\"message\": \"Game inserted.\"}");
  }

  cJSON_Delete(json);
  free(insert_game_format_sql);
}

void request_delete_game_by_id(sqlite3 *db, char *id, char **response,
                               char **err_msg)
{
  const char *delete_game_sql = "DELETE FROM Games WHERE game_id = %s;";
  char *delete_game_format_sql =
      malloc(strlen(delete_game_sql) + strlen(id) + 1);
  sprintf(delete_game_format_sql, delete_game_sql, id);

  db_request(db, delete_game_format_sql, 0, 0, err_msg, "Deleted game by id");

  *response = construct_response(SUCCESS, "{\"message\": \"Game deleted.\"}");
  free(delete_game_format_sql);
}

void request_patch_game_by_id(sqlite3 *db, char *id, char *body,
                              char **response, char **err_msg)
{
  char *update_game_sql = malloc(1024);
  sprintf(update_game_sql, "UPDATE Games SET ");
  int has_updates = 0;

  cJSON *json = cJSON_Parse(body);
  if (!json) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Failed to parse JSON request body.\"}");
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
      strcat(update_game_sql, id);
      strcat(update_game_sql, ";");

      // Execute the SQL query
      db_request(db, update_game_sql, 0, 0, err_msg, "Updated game by id");

      *response =
          construct_response(SUCCESS, "{\"message\": \"Game updated.\"}");
    } else {
      *response = construct_response(
          BAD_REQUEST, "{\"error\": \"No fields provided to update.\"}");
    }
  }

  free(update_game_sql);
  cJSON_Delete(json);
}

void request_post_register(sqlite3 *db, char *body, char **response,
                           char **err_msg, int socket)
{
  char *create_user_sql =
      "INSERT INTO Users (username, email, password, profile_image) "
      "VALUES ('%s', '%s', '%s', '%s');";
  char *create_user_format_sql = malloc(strlen(create_user_sql) + 200);

  cJSON *json = cJSON_Parse(body);

  if (!json) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Failed to parse JSON request body.\"}");
  } else {
    cJSON *username = cJSON_GetObjectItem(json, "username");
    if (!username) {
      *response = construct_response(
          BAD_REQUEST, "{\"error\": \"Missing required field: username.\"}");
      send(socket, response, strlen(*response), 0);
      return;
    }
    cJSON *email = cJSON_GetObjectItem(json, "email");
    if (!email) {
      *response = construct_response(
          BAD_REQUEST, "{\"error\": \"Missing required field: email.\"}");
      send(socket, response, strlen(*response), 0);
      return;
    }
    cJSON *password = cJSON_GetObjectItem(json, "password");
    if (!password) {
      *response = construct_response(
          BAD_REQUEST, "{\"error\": \"Missing required field: password.\"}");
      send(socket, response, strlen(*response), 0);
      return;
    }
    cJSON *profile_image = cJSON_GetObjectItem(json, "profile_image");
    if (!profile_image) {
      *response = construct_response(
          BAD_REQUEST, "{\"error\": \"Missing required field: profile_image.\"}");
      send(socket, response, strlen(*response), 0);
      return;
    }

    char *hashed_password = crypt(password->valuestring, "salt");

    sprintf(create_user_format_sql, create_user_sql, username->valuestring,
            email->valuestring, hashed_password, profile_image->valuestring);

    db_request(db, create_user_format_sql, 0, 0, err_msg, "Inserted user");

    *response = construct_response(200, "{\"message\": \"User registered.\"}");
  }
}

void request_post_login(sqlite3 *db, char *body, char **response,
                        char **err_msg, int socket)
{
  cJSON *json = cJSON_Parse(body);
  if (!json) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Failed to parse JSON request body.\"}");
  } else {
    cJSON *email = cJSON_GetObjectItem(json, "email");
    if (!email) {
      *response = construct_response(
          BAD_REQUEST, "{\"error\": \"Missing required field: email.\"}");
      send(socket, response, strlen(*response), 0);
      return;
    }
    cJSON *password = cJSON_GetObjectItem(json, "password");
    if (!password) {
      *response = construct_response(
          BAD_REQUEST, "{\"error\": \"Missing required field: password.\"}");
      send(socket, response, strlen(*response), 0);
      return;
    }

    char *hashed_password = crypt(password->valuestring, "salt");

    const char *login_sql =
        "SELECT * FROM Users WHERE email = '%s' AND password = '%s';";
    char *login_format_sql = malloc(strlen(login_sql) + 200);
    sprintf(login_format_sql, login_sql, email->valuestring, hashed_password);

    cJSON *json = cJSON_CreateObject();
    if (!json) {
      fprintf(stderr, "ERROR: Failed to create JSON object.\n");
      return;
    }
    db_request(db, login_format_sql, callback_object, json, err_msg,
               "Fetched user by email and password");

    int response_code = SUCCESS;
    if (cJSON_GetArraySize(json) == 0) {
      response_code = NOT_FOUND;
      cJSON_AddStringToObject(json, "error", "User not found.");
    }

    char *json_string = cJSON_PrintUnformatted(json);

    if (json_string) {
      *response = construct_response(response_code, json_string);
      free(json_string);
    } else {
      *response =
          construct_response(INTERNAL_SERVER_ERROR, "{\"error\": \"Failed to serialize JSON.\"}");
    }

    cJSON_Delete(json);
    free(login_format_sql);
  }
}
