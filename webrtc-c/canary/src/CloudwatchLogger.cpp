#include "Include_i.h"

namespace Canary {

CloudwatchLogger::CloudwatchLogger(PConfig pConfig, ClientConfiguration* pClientConfig) : pConfig(pConfig), client(*pClientConfig)
{
}

STATUS CloudwatchLogger::init()
{
    STATUS retStatus = STATUS_SUCCESS;
    CreateLogGroupRequest createLogGroupRequest;
    Aws::CloudWatchLogs::Model::CreateLogStreamOutcome createLogStreamOutcome;
    CreateLogStreamRequest createLogStreamRequest;

    createLogGroupRequest.SetLogGroupName(pConfig->pLogGroupName);
    // ignore error since if this operation fails, CreateLogStream should fail as well.
    // There might be some errors that can lead to successfull CreateLogStream, e.g. log group already exists.
    this->client.CreateLogGroup(createLogGroupRequest);

    createLogStreamRequest.SetLogGroupName(pConfig->pLogGroupName);
    createLogStreamRequest.SetLogStreamName(pConfig->pLogStreamName);
    createLogStreamOutcome = this->client.CreateLogStream(createLogStreamRequest);

    if (!createLogStreamOutcome.IsSuccess()) {
        CHK_ERR(FALSE, STATUS_INTERNAL_ERROR, "Failed to create \"%s\" log stream: %s", pConfig->pLogStreamName,
                createLogStreamOutcome.GetError().GetMessage().c_str());
    }

CleanUp:

    return retStatus;
}

VOID CloudwatchLogger::deinit()
{
    this->flush(TRUE);
}

VOID CloudwatchLogger::push(string log)
{
    std::lock_guard<std::recursive_mutex> lock(this->sync.mutex);
    Aws::String awsCwString(log.c_str(), log.size());
    auto logEvent =
        Aws::CloudWatchLogs::Model::InputLogEvent().WithMessage(awsCwString).WithTimestamp(GETTIME() / HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    this->logs.push_back(logEvent);

    if (this->logs.size() >= MAX_CLOUDWATCH_LOG_COUNT) {
        this->flush();
    }
}

VOID CloudwatchLogger::flush(BOOL sync)
{
    std::unique_lock<std::recursive_mutex> lock(this->sync.mutex);
    if (this->logs.size() == 0) {
        return;
    }

    auto pendingLogs = this->logs;
    this->logs.clear();

    // wait until previous logs have been flushed entirely
    auto waitUntilFlushed = [this] { return !this->sync.pending.load(); };
    this->sync.await.wait(lock, waitUntilFlushed);

    auto request = Aws::CloudWatchLogs::Model::PutLogEventsRequest()
                       .WithLogGroupName(this->pConfig->pLogGroupName)
                       .WithLogStreamName(this->pConfig->pLogStreamName)
                       .WithLogEvents(pendingLogs);

    if (this->token != "") {
        request.SetSequenceToken(this->token);
    }

    if (!sync) {
        auto asyncHandler = [this](const Aws::CloudWatchLogs::CloudWatchLogsClient* cwClientLog,
                                   const Aws::CloudWatchLogs::Model::PutLogEventsRequest& request,
                                   const Aws::CloudWatchLogs::Model::PutLogEventsOutcome& outcome,
                                   const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context) {
            UNUSED_PARAM(cwClientLog);
            UNUSED_PARAM(request);
            UNUSED_PARAM(context);

            if (!outcome.IsSuccess()) {
                DLOGE("Failed to push logs: %s", outcome.GetError().GetMessage().c_str());
            } else {
                DLOGS("Successfully pushed logs to cloudwatch");
                this->token = outcome.GetResult().GetNextSequenceToken();
            }

            this->sync.pending = TRUE;
            this->sync.await.notify_one();
        };

        this->sync.pending = FALSE;
        this->client.PutLogEventsAsync(request, asyncHandler);
    } else {
        auto outcome = this->client.PutLogEvents(request);
        if (!outcome.IsSuccess()) {
            DLOGE("Failed to push logs: %s", outcome.GetError().GetMessage().c_str());
        } else {
            DLOGS("Successfully pushed logs to cloudwatch");
            this->token = outcome.GetResult().GetNextSequenceToken();
        }
    }
}

} // namespace Canary
