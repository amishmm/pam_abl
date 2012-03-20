#include "test.h"
#include "pam_able.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define TEST_DIR "/tmp/pam-able_test-dir"

static void testPamAbleDbEnv() {
    //first start off wit a non existing dir, we expect it to fail
    abl_args args;
    memset(&args, 0, sizeof(abl_args));
    args.db_home = "/tmp/blaat/non-existing";
    args.host_db = "/tmpt/blaat/non-existing/hosts.db";
    args.user_db = "/tmpt/blaat/non-existing/users.db";

    PamAbleDbEnv *dummy = openPamAbleDbEnvironment(&args, NULL);
    if (dummy) {
        printf("   The db could be opened on a non existing environment.\n");
    }

    //next start it using an existing dir, it should succeed
    removeDir(TEST_DIR);
    makeDir(TEST_DIR);
    args.db_home = TEST_DIR;
    args.host_db = TEST_DIR"/hosts.db";
    args.user_db = TEST_DIR"/users.db";
    dummy = openPamAbleDbEnvironment(&args, NULL);
    if (dummy) {
        destroyPamAbleDbEnvironment(dummy);
    } else {
        printf("   The db could be opened on a non existing environment.\n");
    }
    removeDir(TEST_DIR);
//    void destroyPamAbleDbEnvironment(PamAbleDbEnv *env);
}

static int setupTestEnvironment(PamAbleDbEnv **dbEnv) {
    removeDir(TEST_DIR);
    makeDir(TEST_DIR);
    char *userClearBuffer = malloc(100);
    memset(userClearBuffer, 0, 100);
    char *hostClearBuffer = malloc(100);
    memset(hostClearBuffer, 0, 100);
    char *userBlockedBuffer = malloc(100);
    memset(userBlockedBuffer, 0, 100);
    char *hostBlockedBuffer = malloc(100);
    memset(hostBlockedBuffer, 0, 100);
    abl_args args;
    memset(&args, 0, sizeof(abl_args));
    args.db_home = TEST_DIR;
    args.host_db = TEST_DIR"/hosts.db";
    args.user_db = TEST_DIR"/users.db";
    *dbEnv = openPamAbleDbEnvironment(&args, NULL);
    if (!*dbEnv) {
        printf("   The db environment could not be opened.\n");
        return 1;
    }

    int i = 0;
    for (; i < 20; ++i) {
        AuthState *userClearState = NULL;
        AuthState *userBlockedState = NULL;
        if (createEmptyState(CLEAR, &userClearState) || createEmptyState(BLOCKED, &userBlockedState)) {
            printf("   Could not create an empty state.\n");
            return 1;
        }
        AuthState *hostClearState = NULL;
        AuthState *hostBlockedState = NULL;
        if (createEmptyState(CLEAR, &hostClearState) || createEmptyState(BLOCKED, &hostBlockedState)) {
            printf("   Could not create an empty state.\n");
            return 1;
        }
        snprintf(userClearBuffer, 100, "cu_%d", i);
        snprintf(hostClearBuffer, 100, "ch_%d", i);
        snprintf(userBlockedBuffer, 100, "bu_%d", i);
        snprintf(hostBlockedBuffer, 100, "bh_%d", i);
        time_t tm = time(NULL);
        int x = 50;
        for (; x >= 0; --x) {
            time_t logTime = tm - x*10;
            if (addAttempt(userClearState, USER_BLOCKED, logTime, "host", "Service", 0, 0))
                printf("   Could not add an attempt for user %s.\n", userClearBuffer);
            if (addAttempt(userBlockedState, USER_BLOCKED, logTime, "host", "Service", 0, 0))
                printf("   Could not add an attempt for user %s.\n", userBlockedBuffer);
            if (addAttempt(hostClearState, USER_BLOCKED, logTime, "user", "Service", 0, 0))
                printf("   Could not add an attempt for host %s.\n", hostClearBuffer);
            if (addAttempt(hostBlockedState, USER_BLOCKED, logTime, "user", "Service", 0, 0))
                printf("   Could not add an attempt for host %s.\n", hostBlockedBuffer);
        }
        if (saveInfo((*dbEnv)->m_userDb, userClearBuffer, userClearState))
            printf("   Could not save state for user %s.\n", userClearBuffer);
        if (saveInfo((*dbEnv)->m_userDb, userBlockedBuffer, userBlockedState))
            printf("   Could not save state for user %s.\n", userBlockedBuffer);
        if (saveInfo((*dbEnv)->m_hostDb, hostClearBuffer, hostClearState))
            printf("   Could not save state for host %s.\n", hostClearBuffer);
        if (saveInfo((*dbEnv)->m_hostDb, hostBlockedBuffer, hostBlockedState))
            printf("   Could not save state for host %s.\n", hostBlockedBuffer);
        destroyAuthState(userClearState);
        destroyAuthState(userBlockedState);
        destroyAuthState(hostClearState);
        destroyAuthState(hostBlockedState);
    }
    free(userClearBuffer);
    free(hostClearBuffer);
    free(userBlockedBuffer);
    free(hostBlockedBuffer);
    return 0;
}

static void checkAttempt(const char *user, const char *userRule, BlockState newUserState,
                         const char *host, const char *hostRule, BlockState newHostState,
                         const char *service, BlockState expectedBlockState, BlockReason bReason, const PamAbleDbEnv *dbEnv) {
    abl_args args;
    memset(&args, 0, sizeof(abl_args));
    args.host_rule = hostRule;
    args.user_rule = userRule;
    //TODO, check if the commands have been run
//    const char      *host_blk_cmd;
//    const char      *host_clr_cmd;

    abl_info info;
    memset(&info, 0, sizeof(abl_info));
    info.user = user;
    info.host = host;
    info.service = service;
    BlockState newState = check_attempt(dbEnv, &args, &info, NULL);
    if (newState != expectedBlockState) {
        printf("   Expected attempt to have as result %d, yet %d was returned.\n", (int)expectedBlockState, (int)newState);
    }
    if (info.blockReason != bReason) {
        printf("   Expected the reason to be %d, yet %d was returned.\n", (int)bReason, (int)info.blockReason);
    }
    startTransaction(dbEnv->m_environment);
    AuthState *userState = NULL;
    if (getUserOrHostInfo(dbEnv->m_userDb, user, &userState))
        printf("   Could not retrieve the current state of the user.\n");
    if (userState) {
        BlockState retrievedState = getState(userState);
        if (retrievedState != newUserState)
            printf("   Expected attempt to have as user blockstate %d, yet %d was returned.\n", (int)newUserState, (int)retrievedState);
        destroyAuthState(userState);
    } else {
        printf("   Does the host not exist in the db?.\n");
    }

    AuthState *hostState = NULL;
    if (getUserOrHostInfo(dbEnv->m_hostDb, host, &hostState))
        printf("   Could not retrieve the current state of the host.\n");
    if (hostState) {
        BlockState retrievedState = getState(hostState);
        if (retrievedState != newHostState)
            printf("   Expected attempt to have as host blockstate %d, yet %d was returned.\n", (int)newHostState, (int)retrievedState);
        destroyAuthState(hostState);
    } else {
        printf("   Does the host not exist in the db?.\n");
    }
    abortTransaction(dbEnv->m_environment);
}

static void testCheckAttempt() {
    removeDir(TEST_DIR);

    PamAbleDbEnv *dbEnv = NULL;
    if (setupTestEnvironment(&dbEnv) || !dbEnv) {
        printf("   Could not create our test environment.\n");
        return;
    }

    //we have 20 user/hosts with a CLEAR/BLOCKED state, all with 50 attempts
    const char *clearRule = "*:30/10s";
    const char *blockRule = "*:1/1h";
    const char *service = "Service";

    //user clear, host clear, no blocking
    checkAttempt("cu_0", clearRule, CLEAR, "ch_0", clearRule, CLEAR, service, CLEAR, AUTH_FAILED, dbEnv);
    //user clear, host clear, user blocked
    checkAttempt("cu_1", blockRule, BLOCKED, "ch_1", clearRule, CLEAR, service, BLOCKED, USER_BLOCKED, dbEnv);
    //user clear, host clear, host blocked
    checkAttempt("cu_2", clearRule, CLEAR, "ch_2", blockRule, BLOCKED, service, BLOCKED, HOST_BLOCKED, dbEnv);
    //user clear, host clear, both blocked
    checkAttempt("cu_3", blockRule, BLOCKED, "ch_3", blockRule, BLOCKED, service, BLOCKED, BOTH_BLOCKED, dbEnv);

    //user blocked, host clear, no blocking
    checkAttempt("bu_4", clearRule, CLEAR, "ch_4", clearRule, CLEAR, service, CLEAR, AUTH_FAILED, dbEnv);
    //user blocked, host clear, user blocked
    checkAttempt("bu_5", blockRule, BLOCKED, "ch_5", clearRule, CLEAR, service, BLOCKED, USER_BLOCKED, dbEnv);
    //user blocked, host clear, host blocked
    checkAttempt("bu_6", clearRule, CLEAR, "ch_6", blockRule, BLOCKED, service, BLOCKED, HOST_BLOCKED, dbEnv);
    //user blocked, host clear, both blocked
    checkAttempt("bu_7", blockRule, BLOCKED, "ch_7", blockRule, BLOCKED, service, BLOCKED, BOTH_BLOCKED, dbEnv);

    //user clear, host blocked, no blocking
    checkAttempt("cu_8", clearRule, CLEAR, "bh_8", clearRule, CLEAR, service, CLEAR, AUTH_FAILED, dbEnv);
    //user clear, host blocked, user blocked
    checkAttempt("cu_9", blockRule, BLOCKED, "bh_9", clearRule, CLEAR, service, BLOCKED, USER_BLOCKED, dbEnv);
    //user clear, host blocked, host blocked
    checkAttempt("cu_10", clearRule, CLEAR, "bh_10", blockRule, BLOCKED, service, BLOCKED, HOST_BLOCKED, dbEnv);
    //user clear, host blocked, both blocked
    checkAttempt("cu_11", blockRule, BLOCKED, "bh_11", blockRule, BLOCKED, service, BLOCKED, BOTH_BLOCKED, dbEnv);

    //user blocked, host blocked, no blocking
    checkAttempt("bu_12", clearRule, CLEAR, "bh_12", clearRule, CLEAR, service, CLEAR, AUTH_FAILED, dbEnv);
    //user blocked, host blocked, user blocked
    checkAttempt("bu_13", blockRule, BLOCKED, "bh_13", clearRule, CLEAR, service, BLOCKED, USER_BLOCKED, dbEnv);
    //user blocked, host blocked, host blocked
    checkAttempt("bu_14", clearRule, CLEAR, "bh_14", blockRule, BLOCKED, service, BLOCKED, HOST_BLOCKED, dbEnv);
    //user blocked, host blocked, both blocked
    checkAttempt("bu_15", blockRule, BLOCKED, "bh_15", blockRule, BLOCKED, service, BLOCKED, BOTH_BLOCKED, dbEnv);

    destroyPamAbleDbEnvironment(dbEnv);
    removeDir(TEST_DIR);
}

static void testRecordAttempt() {
    removeDir(TEST_DIR);
    char userBuffer[100];
    char hostBuffer[100];
    char serviceBuffer[100];
    time_t currentTime = time(NULL);

    abl_args args;
    memset(&args, 0, sizeof(abl_args));
    args.host_purge = 60*60*24; //1 day
    args.user_purge = 60*60*24; //1 day

    abl_info info;
    memset(&info, 0, sizeof(abl_info));
    info.blockReason = USER_BLOCKED;
    info.user = &userBuffer[0];
    info.host = &hostBuffer[0];
    info.service = &serviceBuffer[0];

    PamAbleDbEnv *dbEnv = NULL;
    if (setupTestEnvironment(&dbEnv) || !dbEnv) {
        printf("   Could not create our test environment.\n");
        return;
    }

    int x = 0;
    int y = 0;
    for (x = 0; x < 5; ++x) {
        for (y = 0; y < 10; ++y) {
            snprintf(&userBuffer[0], 100, "user_%d", y);
            snprintf(&hostBuffer[0], 100, "host_%d", y);
            snprintf(&serviceBuffer[0], 100, "service_%d", y);
            if (record_attempt(dbEnv, &args, &info, NULL))
                printf("   Could not add an attempt.\n");
        }
    }

    startTransaction(dbEnv->m_environment);
    for (y = 0; y < 10; ++y) {
        snprintf(&userBuffer[0], 100, "user_%d", y);
        snprintf(&hostBuffer[0], 100, "host_%d", y);
        snprintf(&serviceBuffer[0], 100, "service_%d", y);
        AuthState *userState = NULL;
        AuthState *hostState = NULL;
        if (getUserOrHostInfo(dbEnv->m_userDb, &userBuffer[0], &userState))
            printf("   Could not retrieve info for user %s.\n", &userBuffer[0]);
        if (getUserOrHostInfo(dbEnv->m_hostDb, &hostBuffer[0], &hostState))
            printf("   Could not retrieve info for host %s.\n", &hostBuffer[0]);
        if (userState && hostState) {
            if (getNofAttempts(userState) != 5 || getNofAttempts(hostState) != 5) {
                printf("   We expected to find five attempts.\n");
            } else {
                AuthAttempt attempt;
                while (nextAttempt(userState, &attempt) == 0) {
                    if (strcmp(&hostBuffer[0], attempt.m_userOrHost) != 0)
                        printf("   Expected host %s, but recieved %s.\n", &hostBuffer[0], attempt.m_userOrHost);
                    if (strcmp(&serviceBuffer[0], attempt.m_service) != 0)
                        printf("   Expected service %s, but recieved %s.\n", &serviceBuffer[0], attempt.m_service);
                    if (attempt.m_time < currentTime)
                        printf("   The attempt took place in the past.\n");
                }

                while (nextAttempt(hostState, &attempt) == 0) {
                    if (strcmp(&userBuffer[0], attempt.m_userOrHost) != 0)
                        printf("   Expected user %s, but recieved %s.\n", &userBuffer[0], attempt.m_userOrHost);
                    if (strcmp(&serviceBuffer[0], attempt.m_service) != 0)
                        printf("   Expected service %s, but recieved %s.\n", &serviceBuffer[0], attempt.m_service);
                    if (attempt.m_time < currentTime)
                        printf("   The attempt took place in the past.\n");
                    if (attempt.m_reason != USER_BLOCKED)
                        printf("   Exptected the reason to be %d, yet %d was returned.\n", (int)USER_BLOCKED, (int)attempt.m_reason);
                }
            }
        } else {
            if (!userState)
                printf("   Could not retrieve the user state.\n");
            if (!hostState)
                printf("   Could not retrieve the host state.\n");
        }
        if (userState)
            destroyAuthState(userState);
        if (hostState)
            destroyAuthState(hostState);
    }
    commitTransaction(dbEnv->m_environment);

    destroyPamAbleDbEnvironment(dbEnv);
    removeDir(TEST_DIR);
}

static void testRecordAttemptWhitelistHost() {
    removeDir(TEST_DIR);
    char userBuffer[100];
    char serviceBuffer[100];
    char hostBuffer[100];

    abl_args args;
    memset(&args, 0, sizeof(abl_args));
    args.host_purge = 60*60*24; //1 day
    args.user_purge = 60*60*24; //1 day
    args.host_whitelist = "1.1.1.1;2.2.2.2;127.0.0.1";
    args.user_whitelist = "blaat1;username;blaat3";

    abl_info info;
    PamAbleDbEnv *dbEnv = NULL;
    if (setupTestEnvironment(&dbEnv) || !dbEnv) {
        printf("   Could not create our test environment.\n");
        return;
    }

    int x = 0;
    int y = 0;
    for (x = 0; x < 5; ++x) {
        for (y = 0; y < 10; ++y) {
            memset(&info, 0, sizeof(abl_info));
            info.blockReason = USER_BLOCKED;
            info.user = &userBuffer[0];
            info.service = &serviceBuffer[0];

            //
            // Add some host checking attempts
            //
            snprintf(&userBuffer[0], 100, "user_%d", y);
            snprintf(&serviceBuffer[0], 100, "service_%d", y);
            info.host = "127.0.0.1";
            if (record_attempt(dbEnv, &args, &info, NULL))
                printf("   Could not add an attempt.\n");

            snprintf(&userBuffer[0], 100, "user__%d", y);
            snprintf(&serviceBuffer[0], 100, "service__%d", y);
            info.host = "";
            if (record_attempt(dbEnv, &args, &info, NULL))
                printf("   Could not add an attempt.\n");

            //
            // Add some user checking attempts
            //
            info.host = &hostBuffer[0];
            snprintf(&hostBuffer[0], 100, "host_%d", y);
            snprintf(&serviceBuffer[0], 100, "service_%d", y);

            info.user = "username";
            if (record_attempt(dbEnv, &args, &info, NULL))
                printf("   Could not add an attempt.\n");

            info.user = "";
            if (record_attempt(dbEnv, &args, &info, NULL))
                printf("   Could not add an attempt.\n");
        }
    }

    startTransaction(dbEnv->m_environment);
    for (y = 0; y < 10; ++y) {
        snprintf(&userBuffer[0], 100, "user_%d", y);
        snprintf(&serviceBuffer[0], 100, "service_%d", y);
        AuthState *userState = NULL;
        if (getUserOrHostInfo(dbEnv->m_userDb, &userBuffer[0], &userState))
            printf("   Could not retrieve info for user %s.\n", &userBuffer[0]);
        if (userState) {
            if (getNofAttempts(userState) != 5) {
                printf("   We expected to find five attempts %d.\n", (int)(getNofAttempts(userState)));
            }
            destroyAuthState(userState);
        } else {
            printf("   Could not retrieve the user state.\n");
        }
    }

    AuthState *hostState = NULL;
    if (getUserOrHostInfo(dbEnv->m_hostDb, "127.0.0.1", &hostState))
        printf("   Could not retrieve info for host 127.0.0.1.\n");
    if (hostState)
        printf("   We expected an empty state for host 127.0.0.1\n");

    hostState = NULL;
    if (getUserOrHostInfo(dbEnv->m_hostDb, "", &hostState))
        printf("   Could not retrieve info for the empty host.\n");
    if (hostState)
        printf("   We expected an empty state for the empty host\n");


    AuthState *userState = NULL;
    if (getUserOrHostInfo(dbEnv->m_userDb, "", &userState))
        printf("   Could not retrieve info for the empty user.\n");
    if (userState)
        printf("   We expected an empty state for the empty user\n");

    userState = NULL;
    if (getUserOrHostInfo(dbEnv->m_userDb, "username", &userState))
        printf("   Could not retrieve info for the empty user.\n");
    if (userState)
        printf("   We expected an empty state for the empty user\n");

    commitTransaction(dbEnv->m_environment);
    destroyPamAbleDbEnvironment(dbEnv);
    removeDir(TEST_DIR);
}

static void testRecordAttemptPurge() {
    removeDir(TEST_DIR);

    abl_args args;
    memset(&args, 0, sizeof(abl_args));
    args.host_purge = 25;
    args.user_purge = 15;

    abl_info info;
    memset(&info, 0, sizeof(abl_info));
    info.blockReason = USER_BLOCKED;
    info.user = "cu_0";
    info.host = "ch_0";
    info.service = "Cool_Service";

    PamAbleDbEnv *dbEnv = NULL;
    if (setupTestEnvironment(&dbEnv) || !dbEnv) {
        printf("   Could not create our test environment.\n");
        return;
    }

    if (record_attempt(dbEnv, &args, &info, NULL))
        printf("   Could not add an attempt.\n");

    //we know in the db every 10 seconds an attempt was made, lets's see if it is purged enough
    //time_t logTime = tm - x*10;
    startTransaction(dbEnv->m_environment);
    AuthState *userState = NULL;
    AuthState *hostState = NULL;
    if (getUserOrHostInfo(dbEnv->m_userDb, info.user, &userState))
        printf("   Could not retrieve info for user %s.\n", info.user);
    if (getUserOrHostInfo(dbEnv->m_hostDb, info.host, &hostState))
        printf("   Could not retrieve info for host %s.\n", info.host);
    if (userState && hostState) {
        //for the user we purged every attempt older then 15 seconds, so we expect to see: now, now - 10 and our just logged time
        if (getNofAttempts(userState) != 3) {
            printf("   The current user state holds %d entries.\n", getNofAttempts(userState));
        }
        if (getNofAttempts(hostState) != 4) {
            printf("   The current host state holds %d entries.\n", getNofAttempts(hostState));
        }
    }
    commitTransaction(dbEnv->m_environment);
    destroyPamAbleDbEnvironment(dbEnv);
    if (userState)
        destroyAuthState(userState);
    if (hostState)
        destroyAuthState(hostState);
}

static void testOpenOnlyHostDb() {
    removeDir(TEST_DIR);
    makeDir(TEST_DIR);
    abl_args args;
    memset(&args, 0, sizeof(abl_args));
    args.db_home = TEST_DIR;
    args.host_db = TEST_DIR"/hosts.db";
    args.user_db = NULL;
    PamAbleDbEnv *dbEnv = openPamAbleDbEnvironment(&args, NULL);
    if (!dbEnv) {
        printf("   The db environment could not be opened.\n");
        return;
    }
    if (!dbEnv->m_environment)
        printf("   The db environment was not filled in.\n");
    if (dbEnv->m_userDb)
        printf("   The user db was filled in.\n");
    if (!dbEnv->m_hostDb)
        printf("   The host db was not filled in.\n");
    destroyPamAbleDbEnvironment(dbEnv);
    removeDir(TEST_DIR);
}

static void testOpenOnlyUserDb() {
    removeDir(TEST_DIR);
    makeDir(TEST_DIR);
    abl_args args;
    memset(&args, 0, sizeof(abl_args));
    args.db_home = TEST_DIR;
    args.host_db = NULL;
    args.user_db = TEST_DIR"/users.db";
    PamAbleDbEnv *dbEnv = openPamAbleDbEnvironment(&args, NULL);
    if (!dbEnv) {
        printf("   The db environment could not be opened.\n");
        return;
    }
    if (!dbEnv->m_environment)
        printf("   The db environment was not filled in.\n");
    if (!dbEnv->m_userDb)
        printf("   The user db was not filled in.\n");
    if (dbEnv->m_hostDb)
        printf("   The host db was filled in.\n");
    destroyPamAbleDbEnvironment(dbEnv);
    removeDir(TEST_DIR);
}


void testAble() {
    printf("Able test start.\n");
    printf(" Starting testPamAbleDbEnv.\n");
    testPamAbleDbEnv();
    printf(" Starting testCheckAttempt.\n");
    testCheckAttempt();
    printf(" Starting testRecordAttempt.\n");
    testRecordAttempt();
    printf(" Starting testRecordAttemptWhitelistHost.\n");
    testRecordAttemptWhitelistHost();
    printf(" Starting testRecordAttemptPurge.\n");
    testRecordAttemptPurge();
    printf(" Starting testOpenOnlyHostDb.\n");
    testOpenOnlyHostDb();
    printf(" Starting testOpenOnlyUserDb.\n");
    testOpenOnlyUserDb();
    printf("Able test end.\n");
}
