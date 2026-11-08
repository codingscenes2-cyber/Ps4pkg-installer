#include "pkgmaster/core/state_transition.hpp"
namespace pkgmaster {
bool can_transition(JobState from,JobState to) noexcept {
 if(from==to)return true;
 switch(from){
 case JobState::Queued:return to==JobState::Resolving||to==JobState::Cancelled||to==JobState::Failed;
 case JobState::Resolving:return to==JobState::Downloading||to==JobState::Paused||to==JobState::Cancelled||to==JobState::Failed;
 case JobState::Downloading:return to==JobState::Verifying||to==JobState::Paused||to==JobState::Cancelled||to==JobState::Failed;
 case JobState::Paused:return to==JobState::Resolving||to==JobState::Downloading||to==JobState::Cancelled||to==JobState::Failed;
 case JobState::Verifying:return to==JobState::ReadyToInstall||to==JobState::Failed||to==JobState::Cancelled;
 case JobState::ReadyToInstall:return to==JobState::Installing||to==JobState::Cancelled;
 case JobState::Installing:return to==JobState::Completed||to==JobState::Failed;
 case JobState::Completed:case JobState::Failed:case JobState::Cancelled:return false;
 } return false;
}
}