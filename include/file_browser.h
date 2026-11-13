#ifndef KANSTL_FILE_BROWSER_H
#define KANSTL_FILE_BROWSER_H

#include <cstdint>
#include <string>
#include <vector>

struct FileEntry {
    std::string name;
    std::string path;
    bool is_directory = false;
    uint64_t size = 0;
};

constexpr int kBrowserRows = 13;

class FileBrowser {
public:
    // Loads `preferred_dir` if valid, otherwise the last directory from
    // `config_path`, otherwise $HOME.
    bool init(const std::string& preferred_dir, const std::string& config_path,
              std::string& error);

    void reload();

    void moveSelection(int delta);
    void pageSelection(int direction);

    // Enters the selected directory, or reports the selected file path and
    // returns true when a file should be opened.
    bool activateSelected(std::string& chosen_file);

    bool goParent();
    bool selectPath(const std::string& path);

    const std::vector<FileEntry>& entries() const { return entries_; }
    const std::string& directory() const { return directory_; }
    const std::string& error() const { return error_; }
    void setError(const std::string& error) { error_ = error; }
    void clearError() { error_.clear(); }

    const FileEntry* selected() const;
    int selection() const { return selection_; }
    int scroll() const { return scroll_; }

private:
    bool setDirectory(const std::string& path, bool remember);
    void ensureVisible();
    void saveLastDir() const;
    std::string readLastDir() const;

    std::string directory_;
    std::vector<FileEntry> entries_;
    std::string config_path_;
    std::string last_selection_path_;
    int selection_ = 0;
    int scroll_ = 0;
    std::string error_;
};

#endif  // KANSTL_FILE_BROWSER_H
