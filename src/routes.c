#include "routes.h"
#include "cJSON.h"
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
    *response = construct_response(
        INTERNAL_SERVER_ERROR, "{\"error\": \"Failed to serialize JSON.\"}");
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
  char *insert_game_format_sql = malloc(strlen(insert_game_sql) + 200);

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
      update_game_sql[strlen(update_game_sql) - 2] = '\0';

      strcat(update_game_sql, " WHERE game_id = ");
      strcat(update_game_sql, id);
      strcat(update_game_sql, ";");

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
          BAD_REQUEST,
          "{\"error\": \"Missing required field: profile_image.\"}");
      send(socket, response, strlen(*response), 0);
      return;
    }

    char *hashed_password = crypt(password->valuestring, "salt");

    sprintf(create_user_format_sql, create_user_sql, username->valuestring,
            email->valuestring, hashed_password, profile_image->valuestring);

    db_request(db, create_user_format_sql, 0, 0, err_msg, "Inserted user");

    *response = construct_response(200, "{\"message\": \"User registered.\"}");
  }

  cJSON_Delete(json);
  free(create_user_format_sql);
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
      *response = construct_response(
          INTERNAL_SERVER_ERROR, "{\"error\": \"Failed to serialize JSON.\"}");
    }

    cJSON_Delete(json);
    free(login_format_sql);
  }
}

void request_get_reviews_by_game_id(sqlite3 *db, char *id, char **response,
                                    char **err_msg)
{
  const char *review_by_game_id_sql =
      "SELECT * FROM Reviews WHERE game_id = %s;";
  char *review_by_game_id_format_sql =
      malloc(strlen(review_by_game_id_sql) + strlen(id) + 1);
  sprintf(review_by_game_id_format_sql, review_by_game_id_sql, id);

  cJSON *json = cJSON_CreateArray();
  if (!json) {
    fprintf(stderr, "ERROR: Failed to create JSON object.\n");
    return;
  }
  db_request(db, review_by_game_id_format_sql, callback_array, json, err_msg,
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
  free(review_by_game_id_format_sql);
}

void request_post_review(sqlite3 *db, char *id, char *body, char **response,
                         char **err_msg, int socket)
{
  const char *insert_review_sql = "INSERT INTO Reviews (user_id, game_id, "
                                  "rating, review_text) "
                                  "VALUES ('%d', '%s', '%d', '%s');";
  char *insert_review_format_sql = malloc(strlen(insert_review_sql) + 200);

  cJSON *json = cJSON_Parse(body);
  if (!json) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Failed to parse JSON request body.\"}");
  } else {
    cJSON *user_id = cJSON_GetObjectItem(json, "user_id");
    if (!user_id) {
      *response = construct_response(
          BAD_REQUEST, "{\"error\": \"Missing required field: user_id.\"}");
      send(socket, response, strlen(*response), 0);
      return;
    }
    cJSON *rating = cJSON_GetObjectItem(json, "rating");
    if (!rating) {
      *response = construct_response(
          BAD_REQUEST, "{\"error\": \"Missing required field: rating.\"}");
      send(socket, response, strlen(*response), 0);
      return;
    }
    cJSON *review_text = cJSON_GetObjectItem(json, "review_text");
    if (!review_text) {
      *response = construct_response(
          BAD_REQUEST, "{\"error\": \"Missing required field: review_text.\"}");
      send(socket, response, strlen(*response), 0);
      return;
    }

    sprintf(insert_review_format_sql, insert_review_sql, user_id->valueint, id,
            rating->valueint, review_text->valuestring);

    db_request(db, insert_review_format_sql, 0, 0, err_msg, "Inserted review");

    *response =
        construct_response(SUCCESS, "{\"message\": \"Review inserted.\"}");
  }

  cJSON_Delete(json);
  free(insert_review_format_sql);
}

void request_get_my_games(sqlite3 *db, char *body, char **response,
                          char **err_msg, int socket)

{
  const char *my_games_sql =
      "SELECT Games.* "
      "FROM Libraries "
      "INNER JOIN Games ON Libraries.game_id = Games.game_id "
      "WHERE Libraries.user_id = %d;";
  char *my_games_format_sql = malloc(strlen(my_games_sql) + 200);

  cJSON *json = cJSON_Parse(body);
  if (!json) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Failed to parse JSON request body.\"}");
  } else {
    cJSON *user_id = cJSON_GetObjectItem(json, "user_id");
    if (!user_id) {
      *response = construct_response(
          BAD_REQUEST, "{\"error\": \"Missing required field: user_id.\"}");
      send(socket, response, strlen(*response), 0);
      return;
    }

    sprintf(my_games_format_sql, my_games_sql, user_id->valueint);

    cJSON *json_array = cJSON_CreateArray();
    if (!json_array) {
      fprintf(stderr, "ERROR: Failed to create JSON array.\n");
      return;
    }
    db_request(db, my_games_format_sql, callback_array, json_array, err_msg,
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
  }

  cJSON_Delete(json);
  free(my_games_format_sql);
}

void request_post_my_game(sqlite3 *db, char *body, char **response,
                          char **err_msg, int socket)

{
  const char *insert_my_game_sql = "INSERT INTO Libraries (user_id, game_id) "
                                   "VALUES ('%d', '%d');";

  char *insert_my_game_format_sql = malloc(strlen(insert_my_game_sql) + 200);

  cJSON *json = cJSON_Parse(body);
  if (!json) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Failed to parse JSON request body.\"}");
  } else {
    cJSON *user_id = cJSON_GetObjectItem(json, "user_id");
    if (!user_id) {
      *response = construct_response(
          BAD_REQUEST, "{\"error\": \"Missing required field: user_id.\"}");
      send(socket, response, strlen(*response), 0);
      return;
    }
    cJSON *game_id = cJSON_GetObjectItem(json, "game_id");
    if (!game_id) {
      *response = construct_response(
          BAD_REQUEST, "{\"error\": \"Missing required field: game_id.\"}");
      send(socket, response, strlen(*response), 0);
      return;
    }

    sprintf(insert_my_game_format_sql, insert_my_game_sql, user_id->valueint,
            game_id->valueint);

    db_request(db, insert_my_game_format_sql, callback_array, 0, err_msg,
               "Inserted game into library");

    *response = construct_response(
        SUCCESS, "{\"message\": \"Game inserted to library.\"}");
  }

  cJSON_Delete(json);
  free(insert_my_game_format_sql);
}

void request_delete_my_game(sqlite3 *db, char *id, char *body, char **response,
                            char **err_msg, int socket)
{
  const char *delete_my_game_sql =
      "DELETE FROM Libraries WHERE game_id = %s AND user_id = %d;";
  char *delete_my_game_format_sql = malloc(strlen(delete_my_game_sql) + 200);

  cJSON *json = cJSON_Parse(body);
  if (!json) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Failed to parse JSON request body.\"}");
  } else {
    cJSON *user_id = cJSON_GetObjectItem(json, "user_id");
    if (!user_id) {
      *response = construct_response(
          BAD_REQUEST, "{\"error\": \"Missing required field: user_id.\"}");
      send(socket, response, strlen(*response), 0);
      return;
    }

    sprintf(delete_my_game_format_sql, delete_my_game_sql, id,
            user_id->valueint);

    db_request(db, delete_my_game_format_sql, callback_array, 0, err_msg,
               "Deleted game from library");

    *response = construct_response(
        SUCCESS, "{\"message\": \"Game deleted from library.\"}");
  }

  cJSON_Delete(json);
  free(delete_my_game_format_sql);
}

void request_get_achievement_by_id(sqlite3 *db, char *id, char **response,
                                   char **err_msg)
{
  const char *achievement_by_id_sql =
      "SELECT * FROM Achievements WHERE achievement_id = %s;";
  char *achievement_by_id_format_sql =
      malloc(strlen(achievement_by_id_sql) + strlen(id) + 1);
  sprintf(achievement_by_id_format_sql, achievement_by_id_sql, id);

  cJSON *json = cJSON_CreateObject();
  if (!json) {
    fprintf(stderr, "ERROR: Failed to create JSON object.\n");
    return;
  }
  db_request(db, achievement_by_id_format_sql, callback_object, json, err_msg,
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
  free(achievement_by_id_format_sql);
}

void request_post_achievement(sqlite3 *db, char *body, char **response,
                              char **err_msg, int socket)
{
  const char *insert_achievement_sql = "INSERT INTO Achievements (game_id, "
                                       "name, description, points) "
                                       "VALUES ('%d', '%s', '%s', '%d');";
  char *insert_achievement_format_sql =
      malloc(strlen(insert_achievement_sql) + 200);

  cJSON *json = cJSON_Parse(body);
  if (!json) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Failed to parse JSON request body.\"}");
  } else {
    cJSON *game_id = cJSON_GetObjectItem(json, "game_id");
    if (!game_id) {
      *response = construct_response(
          BAD_REQUEST, "{\"error\": \"Missing required field: game_id.\"}");
      send(socket, response, strlen(*response), 0);
      return;
    }
    cJSON *name = cJSON_GetObjectItem(json, "name");
    if (!name) {
      *response = construct_response(
          BAD_REQUEST, "{\"error\": \"Missing required field: name.\"}");
      send(socket, response, strlen(*response), 0);
      return;
    }
    cJSON *description = cJSON_GetObjectItem(json, "description");
    if (!description) {
      *response = construct_response(
          BAD_REQUEST, "{\"error\": \"Missing required field: description.\"}");
      send(socket, response, strlen(*response), 0);
      return;
    }
    cJSON *points = cJSON_GetObjectItem(json, "points");
    if (!points) {
      *response = construct_response(
          BAD_REQUEST, "{\"error\": \"Missing required field: points.\"}");
      send(socket, response, strlen(*response), 0);
      return;
    }

    sprintf(insert_achievement_format_sql, insert_achievement_sql,
            game_id->valueint, name->valuestring, description->valuestring,
            points->valueint);

    db_request(db, insert_achievement_format_sql, 0, 0, err_msg,
               "Inserted achievement");

    *response =
        construct_response(SUCCESS, "{\"message\": \"Achievement inserted.\"}");
  }

  cJSON_Delete(json);
  free(insert_achievement_format_sql);
}

void request_patch_achievement_by_id(sqlite3 *db, char *id, char *body,
                                     char **response, char **err_msg)
{
  char *update_achievement_sql = malloc(1024);
  sprintf(update_achievement_sql, "UPDATE Achievements SET ");
  int has_updates = 0;

  cJSON *json = cJSON_Parse(body);
  if (!json) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Failed to parse JSON request body.\"}");
  } else {
    cJSON *name = cJSON_GetObjectItem(json, "name");
    cJSON *description = cJSON_GetObjectItem(json, "description");
    cJSON *points = cJSON_GetObjectItem(json, "points");

    if (name && cJSON_IsString(name)) {
      strcat(update_achievement_sql, "name = '");
      strcat(update_achievement_sql, name->valuestring);
      strcat(update_achievement_sql, "', ");
      has_updates = 1;
    }
    if (description && cJSON_IsString(description)) {
      strcat(update_achievement_sql, "description = '");
      strcat(update_achievement_sql, description->valuestring);
      strcat(update_achievement_sql, "', ");
      has_updates = 1;
    }
    if (points && cJSON_IsNumber(points)) {
      strcat(update_achievement_sql, "points = ");
      char points_str[10];
      sprintf(points_str, "%d", points->valueint);
      strcat(update_achievement_sql, points_str);
      strcat(update_achievement_sql, ", ");
      has_updates = 1;
    }

    if (has_updates) {
      update_achievement_sql[strlen(update_achievement_sql) - 2] = '\0';

      strcat(update_achievement_sql, " WHERE achievement_id = ");
      strcat(update_achievement_sql, id);
      strcat(update_achievement_sql, ";");

      db_request(db, update_achievement_sql, 0, 0, err_msg,
                 "Updated achievement by id");

      *response = construct_response(SUCCESS,
                                     "{\"message\": \"Achievement updated.\"}");
    } else {
      *response = construct_response(
          BAD_REQUEST, "{\"error\": \"No fields provided to update.\"}");
    }
  }

  free(update_achievement_sql);
  cJSON_Delete(json);
}

void request_delete_achievement_by_id(sqlite3 *db, char *id, char **response,
                                      char **err_msg)
{
  const char *delete_achievement_sql =
      "DELETE FROM Achievements WHERE achievement_id = %s;";
  char *delete_achievement_format_sql =
      malloc(strlen(delete_achievement_sql) + strlen(id) + 1);
  sprintf(delete_achievement_format_sql, delete_achievement_sql, id);

  db_request(db, delete_achievement_format_sql, 0, 0, err_msg,
             "Deleted achievement by id");

  *response =
      construct_response(SUCCESS, "{\"message\": \"Achievement deleted.\"}");
  free(delete_achievement_format_sql);
}

void request_get_achievements_by_game_id(sqlite3 *db, char *id, char **response,
                                         char **err_msg)
{
  const char *achievements_by_game_id_sql =
      "SELECT * FROM Achievements WHERE game_id = %s;";
  char *achievements_by_game_id_format_sql =
      malloc(strlen(achievements_by_game_id_sql) + strlen(id) + 1);
  sprintf(achievements_by_game_id_format_sql, achievements_by_game_id_sql, id);

  cJSON *json = cJSON_CreateArray();
  if (!json) {
    fprintf(stderr, "ERROR: Failed to create JSON object.\n");
    return;
  }
  db_request(db, achievements_by_game_id_format_sql, callback_array, json,
             err_msg, "Fetched achievements by game id");

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
  free(achievements_by_game_id_format_sql);
}

void request_get_user_achievements(sqlite3 *db, char *body, char **response,
                                   char **err_msg, int socket)
{
  const char *user_achievements_sql =
      "SELECT Achievements.* "
      "FROM Achievements "
      "INNER JOIN User_Achievements ON Achievements.achievement_id = "
      "User_Achievements.achievement_id "
      "WHERE User_Achievements.user_id = %d;";
  char *user_achievements_format_sql =
      malloc(strlen(user_achievements_sql) + 200);
  cJSON *json = cJSON_Parse(body);
  if (!json) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Failed to parse JSON request body.\"}");
  } else {
    cJSON *user_id = cJSON_GetObjectItem(json, "user_id");
    if (!user_id) {
      *response = construct_response(
          BAD_REQUEST, "{\"error\": \"Missing required field: user_id.\"}");
      send(socket, response, strlen(*response), 0);
      return;
    }

    sprintf(user_achievements_format_sql, user_achievements_sql,
            user_id->valueint);
    printf("user_achievements_format_sql: %s\n", user_achievements_format_sql);

    cJSON *json = cJSON_CreateArray();
    if (!json) {
      fprintf(stderr, "ERROR: Failed to create JSON object.\n");
      return;
    }
    db_request(db, user_achievements_format_sql, callback_array, json, err_msg,
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
  }
  cJSON_Delete(json);
  free(user_achievements_format_sql);
}

void request_post_user_achievement(sqlite3 *db, char *body, char **response,
                                   char **err_msg, int socket)
{
  const char *insert_user_achievement_sql =
      "INSERT INTO User_Achievements (user_id, "
      "achievement_id) "
      "VALUES ('%d', '%d');";
  char *insert_user_achievement_format_sql =
      malloc(strlen(insert_user_achievement_sql) + 200);

  cJSON *json = cJSON_Parse(body);
  if (!json) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Failed to parse JSON request body.\"}");
  } else {
    cJSON *user_id = cJSON_GetObjectItem(json, "user_id");
    if (!user_id) {
      *response = construct_response(
          BAD_REQUEST, "{\"error\": \"Missing required field: user_id.\"}");
      send(socket, response, strlen(*response), 0);
      return;
    }
    cJSON *achievement_id = cJSON_GetObjectItem(json, "achievement_id");
    if (!achievement_id) {
      *response = construct_response(
          BAD_REQUEST,
          "{\"error\": \"Missing required field: achievement_id.\"}");
      send(socket, response, strlen(*response), 0);
      return;
    }

    sprintf(insert_user_achievement_format_sql, insert_user_achievement_sql,
            user_id->valueint, achievement_id->valueint);

    db_request(db, insert_user_achievement_format_sql, 0, 0, err_msg,
               "Inserted user achievement");

    *response = construct_response(
        SUCCESS, "{\"message\": \"User achievement inserted.\"}");
  }
}

void request_get_user_achievements_by_game_id(sqlite3 *db, char *id, char *body,
                                              char **response, char **err_msg,
                                              int socket)
{
  const char *user_achievements_by_game_id_sql =
      "SELECT Achievements.* "
      "FROM Achievements "
      "INNER JOIN User_Achievements ON Achievements.achievement_id = "
      "User_Achievements.achievement_id "
      "WHERE Achievements.game_id = %s AND User_Achievements.user_id = %d;";
  char *user_achievements_by_game_id_format_sql =
      malloc(strlen(user_achievements_by_game_id_sql) + 200);

  cJSON *json = cJSON_Parse(body);
  if (!json) {
    *response = construct_response(
        BAD_REQUEST, "{\"error\": \"Failed to parse JSON request body.\"}");
  } else {
    cJSON *user_id = cJSON_GetObjectItem(json, "user_id");
    if (!user_id) {
      *response = construct_response(
          BAD_REQUEST, "{\"error\": \"Missing required field: user_id.\"}");
      send(socket, response, strlen(*response), 0);
      return;
    }

    sprintf(user_achievements_by_game_id_format_sql,
            user_achievements_by_game_id_sql, id, user_id->valueint);

    cJSON *json = cJSON_CreateArray();
    if (!json) {
      fprintf(stderr, "ERROR: Failed to create JSON object.\n");
      return;
    }
    db_request(db, user_achievements_by_game_id_format_sql, callback_array,
               json, err_msg, "Fetched user achievements by game id");

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
  }

  cJSON_Delete(json);
  free(user_achievements_by_game_id_format_sql);
}
