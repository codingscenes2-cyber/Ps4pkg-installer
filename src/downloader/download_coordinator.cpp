#include "pkgmaster/downloader/download_coordinator.hpp"
#include "pkgmaster/downloader/http_probe.hpp"
#include <algorithm>

namespace pkgmaster {
namespace {
bool update_state(Queue& queue,std::uint64_t id,JobState state) {
    auto current=queue.get(id);if(!current)return false;
    if(current->state==state)return true;
    Job next=*current;next.state=state;return queue.update(next);
}
}
CoordinatedDownloadResult DownloadCoordinator::run(std::uint64_t id,
        HttpDownloadOptions options,unsigned connections,std::uint64_t minimum_part_bytes) {
    CoordinatedDownloadResult out;
    auto initial=queue_.get(id);
    if(!initial){out.error="Queue job was not found.";return out;}
    if(initial->state==JobState::Queued) {
        if(!update_state(queue_,id,JobState::Resolving)){out.error="Could not start queued job.";return out;}
    } else if(initial->state!=JobState::Resolving&&initial->state!=JobState::Downloading) {
        out.error="Job must be queued or resumed before it can run.";return out;
    }
    auto stop=std::make_shared<std::atomic_bool>(false);
    {
        std::lock_guard<std::mutex> lock(active_mutex_);
        if(active_.count(id)){out.error="This queue job is already active.";return out;}
        active_.emplace(id,ActiveJob{stop,false});
    }
    auto cleanup=[&](){std::lock_guard<std::mutex> lock(active_mutex_);active_.erase(id);};
    options.url=initial->source_url;
    options.destination_path=initial->local_path;
    options.cancel=stop.get();
    const auto probe=probe_http_resource(options.url,options.connect_timeout_ms);
    const bool parallel=probe.reachable&&probe.range_supported&&probe.resumable;
    {
        std::lock_guard<std::mutex> lock(active_mutex_);
        auto it=active_.find(id);if(it!=active_.end())it->second.resumable=parallel;
    }
    if(stop->load()) {out.status=DownloadStatus::Cancelled;out.error="Job stopped before transfer.";cleanup();return out;}
    if(probe.reachable&&probe.content_length) {
        auto job=queue_.get(id);
        if(job&&!stop->load()){job->expected_bytes=probe.content_length;queue_.update(*job);}
    }

    const auto user_progress=options.progress;
    const auto user_metrics=options.metrics;
    options.progress=[&,user_progress](std::uint64_t received,std::uint64_t total) {
        if(stop->load())return;
        auto job=queue_.get(id);
        if(job&&job->state!=JobState::Paused&&job->state!=JobState::Cancelled) {
            if(job->state==JobState::Resolving)job->state=JobState::Downloading;
            job->expected_bytes=total?total:job->expected_bytes;
            job->received_bytes=std::max(job->received_bytes,received);
            queue_.update(*job);
        }
        if(user_progress)user_progress(received,total);
    };
    options.metrics=[user_metrics](const DownloadMetrics& metrics) {
        if(user_metrics)user_metrics(metrics);
    };

    std::uint64_t received=0,expected=0;
    if(parallel) {
        ParallelRangeOptions range_options;
        range_options.transfer=options;
        range_options.connections=connections;
        range_options.minimum_part_bytes=minimum_part_bytes;
        const auto result=download_http_ranges(range_options);
        out.status=result.status;out.error=result.error;
        received=result.received_bytes;expected=result.expected_bytes;
        out.used_parallel=true;
    } else {
        const auto result=download_http_file(options);
        out.status=result.status;out.error=result.error;
        received=result.received_bytes;expected=result.expected_bytes;
    }
    if(out.status==DownloadStatus::Completed) {
        auto job=queue_.get(id);
        if(job&&job->state!=JobState::Cancelled&&job->state!=JobState::Paused) {
            if(job->state==JobState::Resolving)job->state=JobState::Downloading;
            job->received_bytes=std::max(job->received_bytes,received);
            if(expected)job->expected_bytes=expected;
            queue_.update(*job);
            update_state(queue_,id,JobState::Verifying);
            update_state(queue_,id,JobState::ReadyToInstall);
        }
    } else if(out.status==DownloadStatus::Failed||out.status==DownloadStatus::Unsupported) {
        auto job=queue_.get(id);
        if(job&&job->state!=JobState::Cancelled&&job->state!=JobState::Paused) {
            job->state=JobState::Failed;job->error=out.error;queue_.update(*job);
        }
    }
    cleanup();
    return out;
}

bool DownloadCoordinator::pause(std::uint64_t id) {
    std::shared_ptr<std::atomic_bool> stop;
    {
        std::lock_guard<std::mutex> lock(active_mutex_);
        auto it=active_.find(id);
        if(it==active_.end()||!it->second.resumable)return false;
        stop=it->second.stop;
    }
    stop->store(true);
    if(!queue_.pause(id)){stop->store(false);return false;}
    return true;
}
bool DownloadCoordinator::cancel(std::uint64_t id) {
    std::shared_ptr<std::atomic_bool> stop;
    {
        std::lock_guard<std::mutex> lock(active_mutex_);
        auto it=active_.find(id);if(it==active_.end())return queue_.cancel(id);
        stop=it->second.stop;
    }
    stop->store(true);
    if(!queue_.cancel(id)){stop->store(false);return false;}
    return true;
}
bool DownloadCoordinator::resume(std::uint64_t id) {
    return queue_.resume(id);
}
}
