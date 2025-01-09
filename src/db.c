#include "db.h"
#include "cJSON.h"
#include <stdio.h>

void db_request(sqlite3 *db, const char *sql,
                int (*callback)(void *, int, char **, char **), void *data,
                char **err_msg, void *description)
{
  int rc = sqlite3_exec(db, sql, callback, data, err_msg);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "ERROR: Failed to execute SQL: %s\n", *err_msg);
    sqlite3_free(*err_msg);
  } else {
    if (description) {
      printf("LOG: %s\n", (char *)description);
    } else {
      printf("LOG: SQL query executed successfully.\n");
    }
  }
}

int callback_object(void *buffer, int argc, char **argv, char **colName)
{
  cJSON *json_object = (cJSON *)buffer;

  for (int i = 0; i < argc; i++) {
    const char *key = colName[i];
    const char *value = argv[i] ? argv[i] : NULL;

    if (value) {
      cJSON_AddStringToObject(json_object, key, value);
    } else {
      cJSON_AddNullToObject(json_object, key);
    }
  }

  return 0;
}

int callback_array(void *buffer, int argc, char **argv, char **colName)
{
  cJSON *json_array = (cJSON *)buffer;

  cJSON *json_row = cJSON_CreateObject();
  if (!json_row) {
    fprintf(stderr, "ERROR: Failed to create JSON object.\n");
    return 1;
  }

  for (int i = 0; i < argc; i++) {
    const char *key = colName[i];
    const char *value = argv[i] ? argv[i] : NULL;

    if (value) {
      cJSON_AddStringToObject(json_row, key, value);
    } else {
      cJSON_AddNullToObject(json_row, key);
    }
  }

  cJSON_AddItemToArray(json_array, json_row);

  return 0;
}

void init_tables(sqlite3 *db, char **err_msg)
{
  const char *create_users_table_sql =
      "CREATE TABLE IF NOT EXISTS Users("
      "user_id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "username TEXT NOT NULL UNIQUE, "
      "email TEXT NOT NULL UNIQUE, "
      "password TEXT NOT NULL, "
      "profile_image TEXT, "
      "created_at DATETIME DEFAULT CURRENT_TIMESTAMP);";
  const char *create_games_table_sql =
      "CREATE TABLE IF NOT EXISTS Games("
      "game_id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "title TEXT NOT NULL, "
      "genre TEXT NOT NULL, "
      "cover_image TEXT NOT NULL, "
      "icon_image TEXT NOT NULL, "
      "release_date DATETIME DEFAULT CURRENT_TIMESTAMP, "
      "developer TEXT);";
  const char *create_libraries_table_sql =
      "CREATE TABLE IF NOT EXISTS Libraries("
      "library_id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "user_id INTEGER NOT NULL, "
      "game_id INTEGER NOT NULL, "
      "purchased_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
      "FOREIGN KEY (user_id) REFERENCES Users(user_id) ON DELETE CASCADE, "
      "FOREIGN KEY (game_id) REFERENCES Games(game_id) ON DELETE CASCADE, "
      "UNIQUE(user_id, game_id));";
  const char *create_reviews_table_sql =
      "CREATE TABLE IF NOT EXISTS Reviews("
      "review_id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "user_id INTEGER NOT NULL, "
      "game_id INTEGER NOT NULL, "
      "rating INTEGER NOT NULL CHECK (rating >= 1 AND rating <=5), "
      "review_text TEXT, "
      "created_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
      "FOREIGN KEY (user_id) REFERENCES Users(user_id) ON DELETE CASCADE, "
      "FOREIGN KEY (game_id) REFERENCES Games(game_id) ON DELETE CASCADE);";
  const char *create_achievements_table_sql =
      "CREATE TABLE IF NOT EXISTS Achievements("
      "achievement_id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "game_id INTEGER NOT NULL, "
      "name TEXT NOT NULL, "
      "description TEXT NOT NULL, "
      "points INTEGER NOT NULL, "
      "FOREIGN KEY (game_id) REFERENCES Games(game_id) ON DELETE CASCADE);";
  const char *create_user_achievements_table_sql =
      "CREATE TABLE IF NOT EXISTS User_Achievements("
      "user_achievement_id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "user_id INTEGER NOT NULL, "
      "achievement_id INTEGER NOT NULL, "
      "unlocked_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
      "FOREIGN KEY (user_id) REFERENCES Users(user_id) ON DELETE CASCADE, "
      "FOREIGN KEY (achievement_id) REFERENCES Achievements(achievement_id) ON "
      "DELETE CASCADE);";

  db_request(db, create_users_table_sql, 0, 0, err_msg, "Users table created.");
  db_request(db, create_games_table_sql, 0, 0, err_msg, "Games table created.");
  db_request(db, create_libraries_table_sql, 0, 0, err_msg,
             "Libraries table created.");
  db_request(db, create_reviews_table_sql, 0, 0, err_msg,
             "Reviews table created.");
  db_request(db, create_achievements_table_sql, 0, 0, err_msg,
             "Achievements table created.");
  db_request(db, create_user_achievements_table_sql, 0, 0, err_msg,
             "User_Achievements table created.");
}
