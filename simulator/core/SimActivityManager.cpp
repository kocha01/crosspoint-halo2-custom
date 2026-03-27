// Desktop ActivityManager — override the FreeRTOS-dependent parts
// The base ActivityManager.cpp is NOT compiled. This file provides the full implementation
// with single-threaded rendering (no tasks, no semaphores needed for actual locking).

#include "activities/ActivityManager.h"
#include "activities/Activity.h"
#include "activities/RenderLock.h"
#include "activities/home/HomeActivity.h"
#include "activities/home/FileBrowserActivity.h"
#include "activities/home/RecentBooksActivity.h"
#include "activities/settings/SettingsActivity.h"
#include "activities/util/FullScreenMessageActivity.h"
#include "activities/reader/ReaderActivity.h"
#include "components/UITheme.h"
#include <HalPowerManager.h>
#include <Logging.h>

// We need the activityManager global
ActivityManager* simActivityManagerPtr = nullptr;

void ActivityManager::begin() {
    // No FreeRTOS task creation — we call render directly from loop
    renderTaskHandle = reinterpret_cast<void*>(0x2);  // Non-null dummy
    simActivityManagerPtr = this;
}

void ActivityManager::renderTaskTrampoline(void*) {}

void ActivityManager::renderTaskLoop() {
    // Not used in simulator — rendering is called directly
}

void ActivityManager::loop() {
    if (currentActivity) {
        currentActivity->loop();
    }

    while (pendingAction != PendingAction::None) {
        if (pendingAction == PendingAction::Pop) {
            RenderLock lock;

            if (!currentActivity) {
                pendingAction = PendingAction::None;
                continue;
            }

            ActivityResult pendingResult = std::move(currentActivity->result);
            exitActivity(lock);
            pendingAction = PendingAction::None;

            if (stackActivities.empty()) {
                lock.unlock();
                goHome();
                continue;
            } else {
                currentActivity = std::move(stackActivities.back());
                stackActivities.pop_back();
                if (currentActivity->resultHandler) {
                    auto handler = std::move(currentActivity->resultHandler);
                    currentActivity->resultHandler = nullptr;
                    lock.unlock();
                    handler(pendingResult);
                }
                if (pendingAction == PendingAction::None) {
                    requestUpdate();
                }
                continue;
            }

        } else if (pendingActivity) {
            RenderLock lock;

            if (pendingAction == PendingAction::Replace) {
                exitActivity(lock);
                while (!stackActivities.empty()) {
                    stackActivities.back()->onExit();
                    stackActivities.pop_back();
                }
            } else if (pendingAction == PendingAction::Push) {
                stackActivities.push_back(std::move(currentActivity));
            }
            pendingAction = PendingAction::None;
            currentActivity = std::move(pendingActivity);

            lock.unlock();
            currentActivity->onEnter();
            continue;
        }
    }

    // Single-threaded render: if update was requested, render immediately
    if (requestedUpdate) {
        requestedUpdate = false;
        if (currentActivity) {
            RenderLock lock;
            currentActivity->render(std::move(lock));
        }
    }
}

void ActivityManager::exitActivity(const RenderLock&) {
    if (currentActivity) {
        currentActivity->onExit();
        currentActivity.reset();
    }
}

void ActivityManager::replaceActivity(std::unique_ptr<Activity>&& newActivity) {
    if (currentActivity) {
        pendingActivity = std::move(newActivity);
        pendingAction = PendingAction::Replace;
    } else {
        currentActivity = std::move(newActivity);
        currentActivity->onEnter();
    }
}

// Navigation methods — real activities where possible, popups for unsupported
void ActivityManager::goHome() { replaceActivity(std::make_unique<HomeActivity>(renderer, mappedInput)); }

void ActivityManager::goToFileTransfer() {
    LOG_INF("SIM", "goToFileTransfer — not available in simulator");
    goToFullScreenMessage("File Transfer (not in sim)");
}
void ActivityManager::goToSettings() {
    LOG_INF("SIM", "goToSettings");
    replaceActivity(std::make_unique<SettingsActivity>(renderer, mappedInput));
}
void ActivityManager::goToFileBrowser(std::string path) {
    LOG_INF("SIM", "goToFileBrowser: %s", path.c_str());
    replaceActivity(std::make_unique<FileBrowserActivity>(renderer, mappedInput, std::move(path)));
}
void ActivityManager::goToRecentBooks() {
    LOG_INF("SIM", "goToRecentBooks");
    replaceActivity(std::make_unique<RecentBooksActivity>(renderer, mappedInput));
}
void ActivityManager::goToBrowser() {
    LOG_INF("SIM", "goToBrowser — not available in simulator");
    goToFullScreenMessage("OPDS Browser (not in sim)");
}
void ActivityManager::goToReader(std::string path) {
    LOG_INF("SIM", "goToReader: %s", path.c_str());
    replaceActivity(std::make_unique<ReaderActivity>(renderer, mappedInput, std::move(path)));
}
void ActivityManager::goToSleep() { LOG_INF("SIM", "goToSleep — not implemented in simulator"); }
void ActivityManager::goToBoot() { LOG_INF("SIM", "goToBoot — not implemented in simulator"); }
void ActivityManager::goToFullScreenMessage(std::string msg, EpdFontFamily::Style style) {
    LOG_INF("SIM", "FullScreenMessage: %s", msg.c_str());
    replaceActivity(std::make_unique<FullScreenMessageActivity>(renderer, mappedInput, std::move(msg), style));
}

void ActivityManager::pushActivity(std::unique_ptr<Activity>&& activity) {
    if (pendingActivity) pendingActivity.reset();
    pendingActivity = std::move(activity);
    pendingAction = PendingAction::Push;
}

void ActivityManager::popActivity() {
    if (pendingActivity) pendingActivity.reset();
    pendingAction = PendingAction::Pop;
}

bool ActivityManager::preventAutoSleep() const { return currentActivity && currentActivity->preventAutoSleep(); }
bool ActivityManager::isReaderActivity() const { return currentActivity && currentActivity->isReaderActivity(); }
bool ActivityManager::skipLoopDelay() const { return currentActivity && currentActivity->skipLoopDelay(); }

void ActivityManager::requestUpdate(bool immediate) {
    if (immediate && currentActivity) {
        RenderLock lock;
        currentActivity->render(std::move(lock));
    } else {
        requestedUpdate = true;
    }
}

void ActivityManager::requestUpdateAndWait() {
    if (currentActivity) {
        RenderLock lock;
        currentActivity->render(std::move(lock));
    }
}

// ── RenderLock (no-op in single-threaded simulator) ──
RenderLock::RenderLock() : isLocked(true) {}
RenderLock::RenderLock(Activity&) : isLocked(true) {}
RenderLock::~RenderLock() { isLocked = false; }
void RenderLock::unlock() { isLocked = false; }
bool RenderLock::peek() { return false; }
