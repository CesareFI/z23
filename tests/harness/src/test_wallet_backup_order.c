/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * Wallet backup chronology remains ordered at integer timestamp boundaries.
 * Fixtures contain no wallet data and never start the backup service. */

#include "test/test_core.h"
#include "services/wallet_backup_service.h"
#include "platform/temp_directory.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

static const struct {
    const char *name;
    time_t modified;
} order_cases[] = {
    { "wallet_backup_100_000001.sqlite.enc", 900 },
    { "wallet_backup_9223372036854775807_000000.sqlite.enc", 901 },
    { "wallet_backup_-9223372036854775808_999999.sqlite", 902 },
    { "wallet_backup_9223372036854775808_000000.sqlite", 50 },
    { "wallet_backup_100_000002.sqlite", 903 },
    { "wallet_backup_9223372036854775807_999999.sqlite", 904 },
    { "wallet_backup_-9223372036854775808_000000.sqlite.enc", 905 },
    { "wallet_backup_-9223372036854775809_000000.sqlite", 70 },
    { "wallet_backup_100_1000000.sqlite.enc", 80 },
    { "wallet_backup_99999999999999999999999_000000.sqlite.enc", 60 },
};

enum { ORDER_CASE_COUNT = sizeof(order_cases) / sizeof(order_cases[0]) };

struct order_fixture {
    char directory[PLATFORM_TEMP_PATH_MAX];
    char paths[ORDER_CASE_COUNT][512];
    bool created[ORDER_CASE_COUNT];
};

/* The fixture owns every created file; callers always run cleanup, including
 * when setup fails. No assertion may bypass closing an opened stream. */
static bool order_create_file(struct order_fixture *fixture, size_t index)
{
    int size = snprintf(fixture->paths[index], sizeof(fixture->paths[index]),
                        "%s/%s", fixture->directory, order_cases[index].name);
    if (size < 0 || (size_t)size >= sizeof(fixture->paths[index]))
        return false;
    FILE *file = fopen(fixture->paths[index], "wx");
    if (!file)
        return false;
    fixture->created[index] = true;
    if (fclose(file) != 0)
        return false;
    struct utimbuf stamp = { .actime = order_cases[index].modified,
                             .modtime = order_cases[index].modified };
    return utime(fixture->paths[index], &stamp) == 0;
}

static bool order_fixture_create(struct order_fixture *fixture)
{
    if (!platform_temp_directory_create("wallet-backup-order-",
            fixture->directory, sizeof(fixture->directory)))
        return false;
    for (size_t i = 0; i < ORDER_CASE_COUNT; i++) {
        if (!order_create_file(fixture, i))
            return false;
    }
    return true;
}

static int order_fixture_cleanup(struct order_fixture *fixture)
{
    int failures = 0;
    for (size_t i = 0; i < ORDER_CASE_COUNT; i++) {
        if (!fixture->created[i])
            continue;
        if (unlink(fixture->paths[i]) != 0 && errno != ENOENT) {
            perror("wallet backup order fixture unlink");
            failures++;
        }
    }
    if (fixture->directory[0] && rmdir(fixture->directory) != 0) {
        perror("wallet backup order fixture rmdir");
        failures++;
    }
    return failures;
}

int test_wallet_backup_order(void);

int test_wallet_backup_order(void)
{
    struct order_fixture fixture = {0};
    int failures = 0;
    TEST("wallet backup: timestamp boundaries preserve listing and retention") {
        ASSERT(order_fixture_create(&fixture));
        static const size_t expected[] = { 5, 1, 4, 0, 8, 7, 9, 3, 2, 6 };
        char paths[ORDER_CASE_COUNT][512];
        ASSERT(wallet_backup_list(fixture.directory, paths,
                                  ORDER_CASE_COUNT) == ORDER_CASE_COUNT);
        for (size_t i = 0; i < ORDER_CASE_COUNT; i++)
            ASSERT(strcmp(paths[i], fixture.paths[expected[i]]) == 0);
        ASSERT(wallet_backup_rotate(fixture.directory, 4) ==
               ORDER_CASE_COUNT - 4);
        ASSERT(wallet_backup_list(fixture.directory, paths,
                                  ORDER_CASE_COUNT) == 4);
        for (size_t i = 0; i < 4; i++)
            ASSERT(strcmp(paths[i], fixture.paths[expected[i]]) == 0);
        PASS();
    } _test_next:;
    return failures + order_fixture_cleanup(&fixture);
}
