#include "file_browser.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return value;
}

std::string homeDir() {
    const char* home = std::getenv("HOME");
    return home && *home ? home : "/home/pi";
}

std::string trim(const std::string& value) {
    size_t begin = 0;
    while (begin < value.size() &&
           std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }
    size_t end = value.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(begin, end - begin);
}

}  // namespace

bool FileBrowser::init(const std::string& preferred_dir,
                       const std::string& config_path, std::string& error) {
    config_path_ = config_path;

    std::string dir = preferred_dir;
    std::error_code ec;
    if (!dir.empty() && !fs::is_directory(dir, ec)) dir.clear();

    if (dir.empty()) {
        dir = readLastDir();
        if (!dir.empty() && !fs::is_directory(dir, ec)) dir.clear();
    }

    if (dir.empty()) dir = homeDir();

    const bool ok = setDirectory(dir, true);
    error = error_;
    return ok;
}

bool FileBrowser::setDirectory(const std::string& path, bool remember) {
    std::error_code ec;
    fs::path dir = fs::path(path).lexically_normal();

    if (!fs::is_directory(dir, ec)) {
        error_ = "NOT A DIRECTORY: " + path;
        return false;
    }

    std::string keep = last_selection_path_;
    if (keep.empty()) {
        if (const FileEntry* entry = selected()) keep = entry->path;
    }

    std::vector<FileEntry> found;
    const fs::path parent = dir.parent_path();
    if (!parent.empty() && parent != dir) {
        found.push_back({"..", parent.string(), true, 0});
    }

    fs::directory_iterator iterator(dir, ec);
    if (ec) {
        directory_ = dir.string();
        entries_.clear();
        selection_ = 0;
        scroll_ = 0;
        error_ = "CANNOT READ " + dir.string() + ": " + ec.message();
        return false;
    }

    for (const fs::directory_entry& entry : iterator) {
        const std::string name = entry.path().filename().string();
        if (name.empty() || name[0] == '.') continue;

        std::error_code type_error;
        const bool is_dir = entry.is_directory(type_error);
        if (type_error) continue;

        if (is_dir) {
            found.push_back({name, entry.path().string(), true, 0});
            continue;
        }

        if (lower(entry.path().extension().string()) != ".stl") continue;

        std::error_code size_error;
        const uintmax_t size = entry.file_size(size_error);
        found.push_back(
            {name, entry.path().string(), false, size_error ? 0 : size});
    }

    // Keep ".." first, then directories, then files, case-insensitive.
    std::sort(found.begin(), found.end(),
              [](const FileEntry& a, const FileEntry& b) {
                  if (a.name == "..") return true;
                  if (b.name == "..") return false;
                  if (a.is_directory != b.is_directory) return a.is_directory;
                  return lower(a.name) < lower(b.name);
              });

    directory_ = dir.string();
    entries_ = std::move(found);
    error_.clear();

    selection_ = 0;
    if (!keep.empty()) {
        for (size_t i = 0; i < entries_.size(); ++i) {
            if (entries_[i].path == keep) {
                selection_ = static_cast<int>(i);
                break;
            }
        }
    }
    if (selection_ == 0 && !entries_.empty() && entries_.front().name == "..") {
        // Prefer a file over the parent entry on first entry into a folder.
        for (size_t i = 1; i < entries_.size(); ++i) {
            if (!entries_[i].is_directory) {
                selection_ = static_cast<int>(i);
                break;
            }
        }
    }

    ensureVisible();
    if (remember) saveLastDir();
    return true;
}

void FileBrowser::reload() {
    std::string keep = last_selection_path_;
    if (const FileEntry* entry = selected()) keep = entry->path;
    last_selection_path_ = keep;

    std::string dir = directory_;
    std::string previous_error = error_;
    if (!setDirectory(dir, false)) {
        if (error_.empty()) error_ = previous_error;
    }
}

void FileBrowser::moveSelection(int delta) {
    if (entries_.empty()) return;
    selection_ = std::max(0, std::min(static_cast<int>(entries_.size()) - 1,
                                    selection_ + delta));
    ensureVisible();
}

void FileBrowser::pageSelection(int direction) {
    moveSelection(direction * (kBrowserRows - 1));
}

bool FileBrowser::activateSelected(std::string& chosen_file) {
    const FileEntry* entry = selected();
    if (entry == nullptr) return false;

    if (entry->is_directory) {
        last_selection_path_ = entry->path;
        setDirectory(entry->path, true);
        return false;
    }

    last_selection_path_ = entry->path;
    chosen_file = entry->path;
    saveLastDir();
    return true;
}

bool FileBrowser::goParent() {
    const fs::path current(directory_);
    const fs::path parent = current.parent_path();
    if (parent.empty() || parent == current) return false;
    last_selection_path_ = directory_;
    return setDirectory(parent.string(), true);
}

bool FileBrowser::selectPath(const std::string& path) {
    for (size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].path == path) {
            selection_ = static_cast<int>(i);
            last_selection_path_ = path;
            ensureVisible();
            return true;
        }
    }
    return false;
}

const FileEntry* FileBrowser::selected() const {
    if (selection_ < 0 ||
        selection_ >= static_cast<int>(entries_.size())) {
        return nullptr;
    }
    return &entries_[selection_];
}

void FileBrowser::ensureVisible() {
    const int count = static_cast<int>(entries_.size());
    if (count == 0) {
        scroll_ = 0;
        return;
    }
    if (selection_ < scroll_) scroll_ = selection_;
    if (selection_ >= scroll_ + kBrowserRows) {
        scroll_ = selection_ - kBrowserRows + 1;
    }
    const int max_scroll = std::max(0, count - kBrowserRows);
    scroll_ = std::max(0, std::min(scroll_, max_scroll));
}

void FileBrowser::saveLastDir() const {
    if (config_path_.empty() || directory_.empty()) return;

    std::error_code ec;
    const fs::path config(config_path_);
    if (!config.parent_path().empty()) {
        fs::create_directories(config.parent_path(), ec);
    }
    std::ofstream out(config_path_);
    if (out) out << directory_ << "\n";
}

std::string FileBrowser::readLastDir() const {
    if (config_path_.empty()) return {};
    std::ifstream in(config_path_);
    std::string line;
    if (!std::getline(in, line)) return {};
    return trim(line);
}
