#pragma once
#include "pkgmaster/core/job.hpp"
namespace pkgmaster { bool can_transition(JobState from, JobState to) noexcept; }