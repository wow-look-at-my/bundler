#include "watcher.hpp"
#include "glob.hpp"
#include <sys/inotify.h>
#include <unistd.h>
#include <poll.h>
#include <cstring>
#include <iostream>

namespace fs = std::filesystem;

namespace ts0 {

FileWatcher::FileWatcher() : inotify_fd_(-1), running_(false) {
    inotify_fd_ = inotify_init1(IN_NONBLOCK);
    if (inotify_fd_ == -1) {
        throw std::runtime_error("Failed to initialize inotify");
    }
}

FileWatcher::~FileWatcher() {
    stop();
    if (inotify_fd_ != -1) {
        for (const auto& [wd, path] : watch_descriptors_) {
            inotify_rm_watch(inotify_fd_, wd);
        }
        close(inotify_fd_);
    }
}

void FileWatcher::watch(const fs::path& path, const WatchCallback& callback) {
    fs::path abs_path = fs::absolute(path);

    uint32_t mask = IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM;

    if (fs::is_directory(abs_path)) {
        // Watch the directory
        int wd = inotify_add_watch(inotify_fd_, abs_path.c_str(), mask);
        if (wd == -1) {
            throw std::runtime_error("Failed to add watch for: " + abs_path.string());
        }
        watch_descriptors_[wd] = abs_path;
        callbacks_[abs_path] = callback;

        // Also watch subdirectories
        std::error_code ec;
        for (const auto& entry : fs::recursive_directory_iterator(abs_path, fs::directory_options::skip_permission_denied, ec)) {
            if (entry.is_directory()) {
                int sub_wd = inotify_add_watch(inotify_fd_, entry.path().c_str(), mask);
                if (sub_wd != -1) {
                    watch_descriptors_[sub_wd] = entry.path();
                }
            }
        }
    } else {
        // Watch the parent directory for changes to this file
        fs::path parent = abs_path.parent_path();
        int wd = inotify_add_watch(inotify_fd_, parent.c_str(), mask);
        if (wd == -1) {
            throw std::runtime_error("Failed to add watch for: " + parent.string());
        }
        watch_descriptors_[wd] = parent;
        callbacks_[abs_path] = callback;
    }
}

void FileWatcher::watch_glob(const std::string& pattern, const fs::path& base_dir, const WatchCallback& callback) {
    glob_pattern_ = pattern;
    glob_base_ = fs::absolute(base_dir);
    glob_callback_ = callback;

    // Watch the base directory
    uint32_t mask = IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM;
    int wd = inotify_add_watch(inotify_fd_, glob_base_.c_str(), mask);
    if (wd != -1) {
        watch_descriptors_[wd] = glob_base_;
    }

    // Watch all subdirectories
    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(glob_base_, fs::directory_options::skip_permission_denied, ec)) {
        if (entry.is_directory()) {
            int sub_wd = inotify_add_watch(inotify_fd_, entry.path().c_str(), mask);
            if (sub_wd != -1) {
                watch_descriptors_[sub_wd] = entry.path();
            }
        }
    }
}

void FileWatcher::start() {
    if (running_.load()) return;

    running_.store(true);
    watch_thread_ = std::thread([this]() { run_loop(); });
}

void FileWatcher::stop() {
    running_.store(false);
    if (watch_thread_.joinable()) {
        watch_thread_.join();
    }
}

void FileWatcher::run_loop() {
    constexpr size_t EVENT_SIZE = sizeof(struct inotify_event);
    constexpr size_t BUF_LEN = 1024 * (EVENT_SIZE + 16);
    char buffer[BUF_LEN];

    while (running_.load()) {
        struct pollfd pfd = {inotify_fd_, POLLIN, 0};
        int poll_result = poll(&pfd, 1, 100); // 100ms timeout

        if (poll_result < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (poll_result == 0) continue;

        ssize_t length = read(inotify_fd_, buffer, BUF_LEN);
        if (length < 0) {
            if (errno == EAGAIN) continue;
            break;
        }

        ssize_t i = 0;
        while (i < length) {
            struct inotify_event* event = reinterpret_cast<struct inotify_event*>(&buffer[i]);

            if (event->len > 0) {
                auto it = watch_descriptors_.find(event->wd);
                if (it != watch_descriptors_.end()) {
                    fs::path file_path = it->second / event->name;

                    WatchEventType event_type;
                    if (event->mask & (IN_CREATE | IN_MOVED_TO)) {
                        event_type = WatchEventType::Created;
                    } else if (event->mask & (IN_DELETE | IN_MOVED_FROM)) {
                        event_type = WatchEventType::Deleted;
                    } else {
                        event_type = WatchEventType::Modified;
                    }

                    WatchEvent watch_event{event_type, file_path};

                    // Check if this file matches a glob pattern
                    if (glob_callback_) {
                        std::error_code ec;
                        fs::path relative = fs::relative(file_path, glob_base_, ec);
                        if (!ec) {
                            std::string rel_str = relative.string();
                            std::replace(rel_str.begin(), rel_str.end(), '\\', '/');
                            GlobMatcher matcher(glob_pattern_);
                            if (matcher.matches(rel_str)) {
                                glob_callback_(watch_event);
                            }
                        }
                    }

                    // Check specific file callbacks
                    auto cb_it = callbacks_.find(file_path);
                    if (cb_it != callbacks_.end()) {
                        cb_it->second(watch_event);
                    }

                    // Check directory callbacks
                    cb_it = callbacks_.find(it->second);
                    if (cb_it != callbacks_.end()) {
                        cb_it->second(watch_event);
                    }

                    // If a new directory was created, watch it too
                    if ((event->mask & IN_CREATE) && (event->mask & IN_ISDIR)) {
                        uint32_t mask = IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM;
                        int new_wd = inotify_add_watch(inotify_fd_, file_path.c_str(), mask);
                        if (new_wd != -1) {
                            watch_descriptors_[new_wd] = file_path;
                        }
                    }
                }
            }

            i += EVENT_SIZE + event->len;
        }
    }
}

} // namespace ts0
