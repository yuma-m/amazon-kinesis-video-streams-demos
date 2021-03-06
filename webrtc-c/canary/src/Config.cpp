#include "Include.h"

namespace Canary {

STATUS mustenv(CHAR const* pKey, Config::Value<const CHAR*>* pResult)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pResult != NULL, STATUS_NULL_ARG);

    CHK_ERR((pResult->value = getenv(pKey)) != NULL, STATUS_INVALID_OPERATION, "%s must be set", pKey);
    pResult->initialized = TRUE;

CleanUp:

    return retStatus;
}

STATUS optenv(CHAR const* pKey, Config::Value<const CHAR*>* pResult, const CHAR* pDefault)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pResult != NULL, STATUS_NULL_ARG);

    if (NULL == (pResult->value = getenv(pKey))) {
        pResult->value = pDefault;
    }
    pResult->initialized = TRUE;

CleanUp:

    return retStatus;
}

STATUS mustenvBool(CHAR const* pKey, Config::Value<BOOL>* pResult)
{
    STATUS retStatus = STATUS_SUCCESS;
    Config::Value<const CHAR*> raw;

    CHK_STATUS(mustenv(pKey, &raw));
    if (STRCMPI(raw.value, "on") == 0 || STRCMPI(raw.value, "true") == 0) {
        pResult->value = TRUE;
    } else {
        pResult->value = FALSE;
    }
    pResult->initialized = TRUE;

CleanUp:

    return retStatus;
}

STATUS optenvBool(CHAR const* pKey, Config::Value<BOOL>* pResult, BOOL defVal)
{
    STATUS retStatus = STATUS_SUCCESS;
    Config::Value<const CHAR*> raw;

    CHK_STATUS(optenv(pKey, &raw, NULL));
    if (raw.value != NULL) {
        if (STRCMPI(raw.value, "on") == 0 || STRCMPI(raw.value, "true") == 0) {
            pResult->value = TRUE;
        } else {
            pResult->value = FALSE;
        }
    } else {
        pResult->value = defVal;
    }
    pResult->initialized = TRUE;

CleanUp:

    return retStatus;
}

STATUS mustenvUint64(CHAR const* pKey, Config::Value<UINT64>* pResult)
{
    STATUS retStatus = STATUS_SUCCESS;
    Config::Value<const CHAR*> raw;

    CHK_STATUS(mustenv(pKey, &raw));
    STRTOUI64((PCHAR) raw.value, NULL, 10, &pResult->value);
    pResult->initialized = TRUE;

CleanUp:

    return retStatus;
}

STATUS optenvUint64(CHAR const* pKey, Config::Value<UINT64>* pResult, UINT64 defVal)
{
    STATUS retStatus = STATUS_SUCCESS;
    Config::Value<const CHAR*> raw;

    CHK_STATUS(optenv(pKey, &raw, NULL));
    if (raw.value != NULL) {
        STRTOUI64((PCHAR) raw.value, NULL, 10, &pResult->value);
    } else {
        pResult->value = defVal;
    }
    pResult->initialized = TRUE;

CleanUp:

    return retStatus;
}

VOID Config::print()
{
    DLOGD("\n\n"
          "\tChannel Name  : %s\n"
          "\tRegion        : %s\n"
          "\tClient ID     : %s\n"
          "\tRole          : %s\n"
          "\tTrickle ICE   : %s\n"
          "\tUse TURN      : %s\n"
          "\tLog Level     : %u\n"
          "\tLog Group     : %s\n"
          "\tLog Stream    : %s\n"
          "\tDuration      : %lu seconds\n"
          "\tIteration     : %lu seconds\n"
          "\n",
          this->channelName.value, this->region.value, this->clientId.value, this->isMaster.value ? "Master" : "Viewer",
          this->trickleIce.value ? "True" : "False", this->useTurn.value ? "True" : "False", this->logLevel.value, this->logGroupName.value,
          this->logStreamName.value, this->duration.value / HUNDREDS_OF_NANOS_IN_A_SECOND,
          this->iterationDuration.value / HUNDREDS_OF_NANOS_IN_A_SECOND);
}

STATUS Config::init(INT32 argc, PCHAR argv[])
{
    // TODO: Probably also support command line args to fill the config
    // TODO: Probably also support JSON format to fill the config to allow more scalable option
    UNUSED_PARAM(argc);
    UNUSED_PARAM(argv);

    STATUS retStatus = STATUS_SUCCESS;
    Config::Value<UINT64> logLevel64;
    PCHAR pLogStreamName;
    Config::Value<const CHAR*> logGroupName;

    CHK(argv != NULL, STATUS_NULL_ARG);

    MEMSET(this, 0, SIZEOF(Config));

    /* This is ignored for master. Master can extract the info from offer. Viewer has to know if peer can trickle or
     * not ahead of time. */
    CHK_STATUS(optenvBool(CANARY_TRICKLE_ICE_ENV_VAR, &trickleIce, FALSE));
    CHK_STATUS(optenvBool(CANARY_USE_TURN_ENV_VAR, &useTurn, TRUE));
    CHK_STATUS(optenvBool(CANARY_FORCE_TURN_ENV_VAR, &forceTurn, FALSE));

    CHK_STATUS(mustenv(ACCESS_KEY_ENV_VAR, &accessKey));
    CHK_STATUS(mustenv(SECRET_KEY_ENV_VAR, &secretKey));
    CHK_STATUS(optenv(SESSION_TOKEN_ENV_VAR, &sessionToken, NULL));
    CHK_STATUS(optenv(DEFAULT_REGION_ENV_VAR, &region, DEFAULT_AWS_REGION));

    // Set the logger log level
    CHK_STATUS(optenvUint64(DEBUG_LOG_LEVEL_ENV_VAR, &logLevel64, LOG_LEVEL_WARN));
    logLevel.value = (UINT32) logLevel64.value;
    logLevel.initialized = TRUE;

    CHK_STATUS(optenv(CANARY_CHANNEL_NAME_ENV_VAR, &channelName, CANARY_DEFAULT_CHANNEL_NAME));
    CHK_STATUS(optenv(CANARY_CLIENT_ID_ENV_VAR, &clientId, CANARY_DEFAULT_CLIENT_ID));
    CHK_STATUS(optenvBool(CANARY_IS_MASTER_ENV_VAR, &isMaster, TRUE));

    CHK_STATUS(optenv(CANARY_LOG_GROUP_NAME_ENV_VAR, &logGroupName, CANARY_DEFAULT_LOG_GROUP_NAME));
    STRNCPY(this->logGroupName.value, logGroupName.value, ARRAY_SIZE(this->logGroupName.value) - 1);
    this->logGroupName.initialized = TRUE;

    pLogStreamName = getenv(CANARY_LOG_STREAM_NAME_ENV_VAR);
    if (pLogStreamName != NULL) {
        STRNCPY(this->logStreamName.value, pLogStreamName, ARRAY_SIZE(this->logStreamName.value) - 1);
    } else {
        SNPRINTF(this->logStreamName.value, ARRAY_SIZE(this->logStreamName.value) - 1, "%s-%s-%llu", channelName.value,
                 isMaster.value ? "master" : "viewer", GETTIME() / HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    }

    CHK_STATUS(optenvUint64(CANARY_DURATION_IN_SECONDS_ENV_VAR, &duration, 0));
    duration.value *= HUNDREDS_OF_NANOS_IN_A_SECOND;
    // Need to impose a min duration
    if (duration.value != 0 && duration.value < CANARY_MIN_DURATION) {
        DLOGW("Canary duration should be at least %u seconds. Overriding with minimal duration.",
              CANARY_MIN_DURATION / HUNDREDS_OF_NANOS_IN_A_SECOND);
        duration.value = CANARY_MIN_DURATION;
    }

    // Iteration duration is an optional param
    CHK_STATUS(optenvUint64(CANARY_ITERATION_IN_SECONDS_ENV_VAR, &iterationDuration, CANARY_DEFAULT_ITERATION_DURATION_IN_SECONDS));
    iterationDuration.value *= HUNDREDS_OF_NANOS_IN_A_SECOND;

    // Need to impose a min iteration duration
    if (iterationDuration.value < CANARY_MIN_ITERATION_DURATION) {
        DLOGW("Canary iterations duration should be at least %u seconds. Overriding with minimal iterations duration.",
              CANARY_MIN_ITERATION_DURATION / HUNDREDS_OF_NANOS_IN_A_SECOND);
        iterationDuration.value = CANARY_MIN_ITERATION_DURATION;
    }

    CHK_STATUS(optenvUint64(CANARY_BIT_RATE_ENV_VAR, &bitRate, CANARY_DEFAULT_BITRATE));
    CHK_STATUS(optenvUint64(CANARY_FRAME_RATE_ENV_VAR, &frameRate, CANARY_DEFAULT_FRAMERATE));

CleanUp:

    return retStatus;
}

} // namespace Canary
