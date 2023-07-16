#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstring>

#include <sqlite3.h>

static int callback(void *NotUsed, int argc, char **argv, char **azColName)
{
    int i;
    for(i=0; i<argc; i++) {
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    printf("\n");
    return 0;
}

int main(int argc, char **argv)
{
    sqlite3* db;
    int rc;

    rc = sqlite3_open("datapoints.sqlite", &db);
    if(rc != SQLITE_OK)
    {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(EXIT_FAILURE);
    }

    {
        static char stmt[512];
        const char *stmt_tail;
        sprintf(stmt, "CREATE TABLE data(channel Text, time Long, value Single)\n"); 
        sqlite3_stmt* s;
        rc = sqlite3_prepare_v2(db, stmt, -1, &s, &stmt_tail);
        if(rc != SQLITE_OK)
        {
            fprintf(stderr, "Didn't prepare CREATE TABLE statement: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
            exit(EXIT_FAILURE);
        }
        assert(s != NULL);
        rc = sqlite3_step(s);
        if(rc != SQLITE_DONE)
        {
            fprintf(stderr, "Didn't step CREATE TABLE statement: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
            exit(EXIT_FAILURE);
        }
    }

    for(int i = 0; i < 1000; i++)
    {
        int t = random() % 1000000;
        float v = (random() % 1000000) / 100000.0;
        const char *channel = ((random() % 2) == 1) ? "battery voltage" : "inverter power";
        // printf("%8d %f\n", t, v);
        static char stmt[512];
        const char *stmt_tail;
        sprintf(stmt, "INSERT INTO data VALUES ('%s', %d, %f);\n", channel, t, v);
        if(1) {
            sqlite3_stmt* s = NULL;
            rc = sqlite3_prepare_v2(db, stmt, strlen(stmt) + 1, &s, NULL /*&stmt_tail*/);

            if(rc != SQLITE_OK) {
                fprintf(stderr, "Didn't prepare INSERT statement: %s\n", sqlite3_errmsg(db));
                sqlite3_close(db);
                exit(EXIT_FAILURE);
            }
            assert(s != NULL);

            rc = sqlite3_step(s);

            if(rc != SQLITE_DONE) {
                fprintf(stderr, "Didn't step INSERT statement: %s\n", sqlite3_errmsg(db));
                sqlite3_close(db);
                exit(EXIT_FAILURE);
            }

        } else {

            char *errmsg;
            sqlite3_exec(db, stmt, callback, 0, &errmsg);

            if(rc != SQLITE_OK)
            {
                fprintf(stderr, "Didn't exec INSERT statement: %s\n", errmsg);
                sqlite3_free(errmsg);
                sqlite3_close(db);
                exit(EXIT_FAILURE);
            }
        }
    }
    
    {
        static char stmt[512];
        sprintf(stmt, "SELECT * FROM data WHERE channel='battery voltage' AND time > 10000 AND time < 20000 ORDER BY time");
        sqlite3_stmt* s = NULL;
        rc = sqlite3_prepare_v2(db, stmt, strlen(stmt) + 1, &s, NULL);

        if(rc != SQLITE_OK) {
            fprintf(stderr, "Didn't prepare SELECT statement: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
            exit(EXIT_FAILURE);
        }

        assert(s != NULL);

        while(1) {
            rc = sqlite3_step(s);

            if(rc == SQLITE_ROW) {
                printf("%s: %d, %f\n", sqlite3_column_text(s, 0), sqlite3_column_int(s, 1), sqlite3_column_double(s, 2));
            } else if(rc == SQLITE_DONE) {

                break;

            } else {

                fprintf(stderr, "Didn't step SELECT statement: %s\n", sqlite3_errmsg(db));
                sqlite3_close(db);
                exit(EXIT_FAILURE);
            }
        }
    }

    sqlite3_close(db);
}
