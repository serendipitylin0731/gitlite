#include "../include/SomeObj.h"
#include "../include/Utils.h"
#include "../include/Repository.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <set>
#include <map>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <ctime>
#include <iomanip>

namespace fs = std::filesystem;

class SomeObj::Impl {
private:
    std::string gitliteDir = ".gitlite";
    std::string headPath;
    std::string objectsDir;
    std::string stagingPath;
    
    std::string currentBranch = "master";
    std::map<std::string, std::string> stagedFiles;  // filename -> blobHash
    std::set<std::string> removedFiles;
    
    // 辅助方法
    std::string getHeadCommitHash() const;
    void saveHead();
    void loadHead();
    void saveStaging();
    void loadStaging();
    std::string formatTimestamp(const std::string& utcTimestamp) const;
    std::vector<std::string> getAllCommitHashes() const;
    std::string expandCommitId(const std::string& shortId) const;
    void restoreFileFromCommit(const std::string& commitHash, const std::string& filename) const;
    std::string getCommitMessage(const std::string& commitHash) const;
    void printCommitInfo(const std::string& commitHash, bool includeMergeInfo = true) const;
    std::pair<std::string, std::string> getCommitParents(const std::string& commitHash) const;
    
public:
    Impl();
    
    void init();
    void add(const std::string& filename);
    void commit(const std::string& message);
    void rm(const std::string& filename);
    void status();
    void log();
    void globalLog();
    void find(const std::string& commitMessage);
    void checkoutFile(const std::string& filename);
    void checkoutFileInCommit(const std::string& commitId, const std::string& filename);
    
    // 其他方法保持空实现
    void checkoutBranch(const std::string&);
    void branch(const std::string&);
    void rmBranch(const std::string&);
    void reset(const std::string&);
    void merge(const std::string&) {}
    void addRemote(const std::string&, const std::string&) {}
    void rmRemote(const std::string&) {}
    void push(const std::string&, const std::string&) {}
    void fetch(const std::string&, const std::string&) {}
    void pull(const std::string&, const std::string&) {}
};

// ==================== 构造函数和基础方法 ====================

SomeObj::Impl::Impl() {
    headPath = gitliteDir + "/HEAD";
    objectsDir = gitliteDir + "/objects";
    stagingPath = gitliteDir + "/STAGING";
    
    if (Utils::exists(gitliteDir)) {
        loadHead();
        loadStaging();
    }
}

std::string SomeObj::Impl::getHeadCommitHash() const {
    std::string branchPath = gitliteDir + "/refs/heads/" + currentBranch;
    if (Utils::exists(branchPath)) {
        std::string content = Utils::readContentsAsString(branchPath);
        // 清理换行符
        if (!content.empty()) {
            // 移除末尾的换行符
            size_t lastChar = content.find_last_not_of("\n\r");
            if (lastChar != std::string::npos) {
                content = content.substr(0, lastChar + 1);
            } else {
                content.clear();
            }
        }
        return content;
    }
    return "";  // 空字符串表示没有提交
}

void SomeObj::Impl::saveHead() {
    Utils::createDirectories(gitliteDir);
    std::string content = "ref: refs/heads/" + currentBranch + "\n";
    Utils::writeContents(headPath, content);
}

void SomeObj::Impl::loadHead() {
    if (Utils::exists(headPath)) {
        std::string content = Utils::readContentsAsString(headPath);
        if (content.find("ref: refs/heads/") == 0) {
            size_t pos = content.find("ref: refs/heads/") + 16;
            size_t end = content.find('\n', pos);
            if (end != std::string::npos) {
                currentBranch = content.substr(pos, end - pos);
            }
        }
    }
}

void SomeObj::Impl::saveStaging() {
    std::stringstream ss;
    ss << stagedFiles.size() << "\n";
    for (const auto& [filename, hash] : stagedFiles) {
        ss << filename << "\n" << hash << "\n";
    }
    ss << removedFiles.size() << "\n";
    for (const auto& file : removedFiles) {
        ss << file << "\n";
    }
    Utils::writeContents(stagingPath, ss.str());
}

void SomeObj::Impl::loadStaging() {
    if (!Utils::exists(stagingPath)) return;
    
    std::string content = Utils::readContentsAsString(stagingPath);
    std::stringstream ss(content);
    
    stagedFiles.clear();
    removedFiles.clear();
    
    size_t stagedCount, removedCount;
    ss >> stagedCount;
    
    for (size_t i = 0; i < stagedCount; ++i) {
        std::string filename, hash;
        ss >> filename >> hash;
        stagedFiles[filename] = hash;
    }
    
    ss >> removedCount;
    for (size_t i = 0; i < removedCount; ++i) {
        std::string filename;
        ss >> filename;
        removedFiles.insert(filename);
    }
}

// ==================== Subtask1 方法 ====================

void SomeObj::Impl::init() {
    if (Utils::exists(gitliteDir)) {
        Utils::exitWithMessage("A Gitlite version-control system already exists in the current directory.");
    }
    
    Repository repo;
    repo.init();
    
    currentBranch = "master";
    saveHead();
    
    stagedFiles.clear();
    removedFiles.clear();
    saveStaging();
}

void SomeObj::Impl::add(const std::string& filename) {
    // 1. 检查文件是否存在
    if (!Utils::exists(filename)) {
        Utils::exitWithMessage("File does not exist.");
    }
    
    // 2. 读取文件内容并计算哈希
    std::string content = Utils::readContentsAsString(filename);
    std::string hash = Utils::sha1(content);
    
    // 3. 保存blob
    std::string blobPath = objectsDir + "/" + hash;
    if (!Utils::exists(blobPath)) {
        Utils::writeContents(blobPath, content);
    }
    
    // 4. 检查是否与当前提交中的版本相同
    bool isSameAsCurrentCommit = false;
    std::string currentCommitHash = getHeadCommitHash();
    
    if (!currentCommitHash.empty() && currentCommitHash != "0") {
        // 读取当前提交
        std::string commitPath = objectsDir + "/" + currentCommitHash;
        if (Utils::exists(commitPath)) {
            std::string commitContent = Utils::readContentsAsString(commitPath);
            std::istringstream commitStream(commitContent);
            
            // 跳过消息、父提交、时间戳
            std::string line;
            std::getline(commitStream, line);  // 消息
            std::getline(commitStream, line);  // 父提交
            std::getline(commitStream, line);  // 时间戳
            
            int blobCount;
            commitStream >> blobCount;
            
            // 读取所有blob
            for (int i = 0; i < blobCount; ++i) {
                std::string blobHash, blobFile;
                commitStream >> blobHash >> blobFile;
                if (blobFile == filename && blobHash == hash) {
                    isSameAsCurrentCommit = true;
                    break;
                }
            }
        }
    }
    
    // 5. 根据要求处理暂存区
    if (isSameAsCurrentCommit) {
        // 与当前提交相同：从暂存区移除（如果存在）
        stagedFiles.erase(filename);
    } else {
        // 与当前提交不同：添加到暂存区（覆盖之前的）
        stagedFiles[filename] = hash;
    }
    
    // 6. 如果文件被标记为待删除，移除标记
    removedFiles.erase(filename);
    
    // 7. 保存暂存区状态
    saveStaging();
}

void SomeObj::Impl::commit(const std::string& message) {
    if (message.empty()) {
        Utils::exitWithMessage("Please enter a commit message.");
    }
    
    // 检查是否有文件被暂存
    if (stagedFiles.empty() && removedFiles.empty()) {
        Utils::exitWithMessage("No changes added to the commit.");
    }
    
    std::string parentHash = getHeadCommitHash();
    
    std::stringstream commitData;
    commitData << message << "\n";
    
    // 处理父提交：如果为空或不存在，使用"0"
    if (parentHash.empty() || !Utils::exists(objectsDir + "/" + parentHash)) {
        commitData << "0\n";
    } else {
        commitData << parentHash << "\n";
    }
    
    // 时间戳
    std::time_t now = std::time(nullptr);
    std::tm* gmt = std::gmtime(&now);
    char timeBuffer[100];
    std::strftime(timeBuffer, sizeof(timeBuffer), "%a %b %d %H:%M:%S %Y +0000", gmt);
    commitData << timeBuffer << "\n";
    
    // 收集blob信息
    std::map<std::string, std::string> blobs;
    
    // 从父提交继承blob（如果存在且不是初始提交）
    if (!parentHash.empty() && parentHash != "0") {
        std::string commitPath = objectsDir + "/" + parentHash;
        if (Utils::exists(commitPath)) {
            std::string commitContent = Utils::readContentsAsString(commitPath);
            std::stringstream ss(commitContent);
            
            // 跳过前三行：消息、父提交、时间戳
            std::string line;
            std::getline(ss, line);  // 消息
            std::getline(ss, line);  // 父提交
            std::getline(ss, line);  // 时间戳
            
            int blobCount;
            ss >> blobCount;
            
            for (int i = 0; i < blobCount; ++i) {
                std::string blobHash, blobFile;
                ss >> blobHash >> blobFile;
                blobs[blobFile] = blobHash;
            }
        }
    }
    
    // 添加暂存的文件
    for (const auto& [filename, hash] : stagedFiles) {
        blobs[filename] = hash;
    }
    
    // 移除标记为删除的文件
    for (const auto& filename : removedFiles) {
        blobs.erase(filename);
    }
    
    // 写入blob数量和信息
    commitData << blobs.size() << "\n";
    for (const auto& [filename, hash] : blobs) {
        commitData << hash << " " << filename << "\n";
    }
    
    // 保存提交
    std::string commitContent = commitData.str();
    std::string commitHash = Utils::sha1(commitContent);
    std::string commitPath = objectsDir + "/" + commitHash;
    Utils::writeContents(commitPath, commitContent);
    
    // 更新分支引用
    std::string branchPath = gitliteDir + "/refs/heads/" + currentBranch;
    Utils::writeContents(branchPath, commitHash + "\n");
    
    // 清空暂存区
    stagedFiles.clear();
    removedFiles.clear();
    saveStaging();
}

void SomeObj::Impl::rm(const std::string& filename) {
    bool isStaged = (stagedFiles.find(filename) != stagedFiles.end());
    bool isTracked = false;
    
    std::string currentCommitHash = getHeadCommitHash();
    if (!currentCommitHash.empty() && currentCommitHash != "0") {
        std::string commitPath = objectsDir + "/" + currentCommitHash;
        if (Utils::exists(commitPath)) {
            std::string commitContent = Utils::readContentsAsString(commitPath);
            std::stringstream ss(commitContent);
            std::string line;
            
            for (int i = 0; i < 3; ++i) std::getline(ss, line);
            
            int blobCount;
            ss >> blobCount;
            
            for (int i = 0; i < blobCount; ++i) {
                std::string blobHash, blobFile;
                ss >> blobHash >> blobFile;
                if (blobFile == filename) {
                    isTracked = true;
                    break;
                }
            }
        }
    }
    
    if (!isStaged && !isTracked) {
        Utils::exitWithMessage("No reason to remove the file.");
    }
    
    if (isStaged) {
        stagedFiles.erase(filename);
    }
    
    if (isTracked) {
        removedFiles.insert(filename);
        // 删除工作目录中的文件
        if (Utils::exists(filename)) {
            Utils::restrictedDelete(filename);
        }
    }
    
    saveStaging();
}

// ==================== Subtask2 辅助方法 ====================

std::string SomeObj::Impl::formatTimestamp(const std::string& utcTimestamp) const {
    // 初始提交的特殊处理
    if (utcTimestamp == "Thu Jan 01 00:00:00 1970 +0000") {
        return "Wed Jan 01 08:00:00 1970 +0800";
    }
    
    // 尝试解析UTC时间
    std::tm tm = {};
    std::stringstream ss(utcTimestamp);
    ss >> std::get_time(&tm, "%a %b %d %H:%M:%S %Y %z");
    
    if (ss.fail()) {
        return utcTimestamp;
    }
    
    // 转换为本地时间
    std::time_t utcTime = std::mktime(&tm);
    std::tm* localTm = std::localtime(&utcTime);
    
    char buffer[100];
    std::strftime(buffer, sizeof(buffer), "%a %b %d %H:%M:%S %Y %z", localTm);
    
    return std::string(buffer);
}

std::vector<std::string> SomeObj::Impl::getAllCommitHashes() const {
    std::vector<std::string> commits;
    if (!Utils::exists(objectsDir)) return commits;
    
    for (const auto& entry : fs::directory_iterator(objectsDir)) {
        std::string filename = entry.path().filename().string();
        if (filename.length() == 40) {
            commits.push_back(filename);
        }
    }
    return commits;
}

std::string SomeObj::Impl::expandCommitId(const std::string& shortId) const {
    if (shortId.length() == 40) return shortId;
    
    std::vector<std::string> allCommits = getAllCommitHashes();
    for (const auto& commitHash : allCommits) {
        if (commitHash.find(shortId) == 0) {
            return commitHash;
        }
    }
    return "";
}

std::string SomeObj::Impl::getCommitMessage(const std::string& commitHash) const {
    std::string commitPath = objectsDir + "/" + commitHash;
    if (!Utils::exists(commitPath)) return "";
    
    std::string content = Utils::readContentsAsString(commitPath);
    std::stringstream ss(content);
    std::string message;
    std::getline(ss, message);
    return message;
}

std::pair<std::string, std::string> SomeObj::Impl::getCommitParents(const std::string& commitHash) const {
    std::pair<std::string, std::string> parents("", "");
    
    std::string commitPath = objectsDir + "/" + commitHash;
    if (!Utils::exists(commitPath)) return parents;
    
    std::string content = Utils::readContentsAsString(commitPath);
    std::stringstream ss(content);
    
    std::string line;
    std::getline(ss, line); // 跳过消息
    
    // 读取第一个父提交
    std::getline(ss, line);
    if (!line.empty() && line != "0") {
        parents.first = line;
    }
    
    // 检查是否有第二个父提交
    std::getline(ss, line);
    if (line.find(":") == std::string::npos) { // 不是时间戳
        if (!line.empty() && line != "0") {
            parents.second = line;
        }
    }
    
    return parents;
}

void SomeObj::Impl::printCommitInfo(const std::string& commitHash, bool includeMergeInfo) const {
    std::string commitPath = objectsDir + "/" + commitHash;
    if (!Utils::exists(commitPath)) return;
    
    std::string content = Utils::readContentsAsString(commitPath);
    std::stringstream ss(content);
    
    std::string message, parent1, line;
    std::getline(ss, message);
    std::getline(ss, parent1);
    std::getline(ss, line);
    
    bool isMerge = false;
    std::string parent2, timestamp;
    
    // 检查是否是合并提交
    if (line.find(":") == std::string::npos) {
        isMerge = true;
        parent2 = line;
        std::getline(ss, timestamp);
    } else {
        timestamp = line;
    }
    
    std::cout << "===" << std::endl;
    std::cout << "commit " << commitHash << std::endl;
    
    if (isMerge && includeMergeInfo) {
        std::string parent1Short = (parent1.length() >= 7) ? parent1.substr(0, 7) : parent1;
        std::string parent2Short = (parent2.length() >= 7) ? parent2.substr(0, 7) : parent2;
        std::cout << "Merge: " << parent1Short << " " << parent2Short << std::endl;
    }
    
    std::string formattedTimestamp = formatTimestamp(timestamp);
    std::cout << "Date: " << formattedTimestamp << std::endl;
    std::cout << message << std::endl;
    std::cout << std::endl;
}

void SomeObj::Impl::restoreFileFromCommit(const std::string& commitHash, const std::string& filename) const {
    std::string commitPath = objectsDir + "/" + commitHash;
    if (!Utils::exists(commitPath)) {
        Utils::exitWithMessage("No commit with that id exists.");
    }
    
    std::string content = Utils::readContentsAsString(commitPath);
    std::stringstream ss(content);
    
    std::string line;
    for (int i = 0; i < 3; ++i) std::getline(ss, line);
    
    int blobCount;
    ss >> blobCount;
    
    std::string blobHash;
    bool found = false;
    
    for (int i = 0; i < blobCount; ++i) {
        std::string currentHash, currentFile;
        ss >> currentHash >> currentFile;
        if (currentFile == filename) {
            blobHash = currentHash;
            found = true;
            break;
        }
    }
    
    if (!found) {
        Utils::exitWithMessage("File does not exist in that commit.");
    }
    
    std::string blobPath = objectsDir + "/" + blobHash;
    if (!Utils::exists(blobPath)) {
        Utils::exitWithMessage("Blob not found.");
    }
    
    std::string fileContent = Utils::readContentsAsString(blobPath);
    Utils::writeContents(filename, fileContent);
}

// ==================== Subtask2 主要方法 ====================

void SomeObj::Impl::status() {
    std::cout << "=== Branches ===" << std::endl;
    std::cout << "*" << currentBranch << std::endl;
    
    std::string branchesDir = gitliteDir + "/refs/heads";
    if (Utils::exists(branchesDir)) {
        for (const auto& entry : fs::directory_iterator(branchesDir)) {
            std::string branchName = entry.path().filename().string();
            if (branchName != currentBranch) {
                std::cout << branchName << std::endl;
            }
        }
    }
    
    std::cout << std::endl << "=== Staged Files ===" << std::endl;
    for (const auto& [filename, hash] : stagedFiles) {
        std::cout << filename << std::endl;
    }
    
    std::cout << std::endl << "=== Removed Files ===" << std::endl;
    for (const auto& filename : removedFiles) {
        std::cout << filename << std::endl;
    }
    
    std::cout << std::endl << "=== Modifications Not Staged For Commit ===" << std::endl;
    // 留空，按题目要求
    
    std::cout << std::endl << "=== Untracked Files ===" << std::endl;
    // 留空，按题目要求
}

void SomeObj::Impl::log() {
    std::string commitHash = getHeadCommitHash();
    
    while (!commitHash.empty() && commitHash != "0") {
        printCommitInfo(commitHash, true);
        
        auto parents = getCommitParents(commitHash);
        commitHash = parents.first; // 只跟随第一个父提交
    }
}

void SomeObj::Impl::globalLog() {
    std::vector<std::string> allCommits = getAllCommitHashes();
    for (const auto& commitHash : allCommits) {
        printCommitInfo(commitHash, true);
    }
}

void SomeObj::Impl::find(const std::string& commitMessage) {
    std::vector<std::string> allCommits = getAllCommitHashes();
    std::vector<std::string> matchingCommits;
    
    for (const auto& commitHash : allCommits) {
        std::string message = getCommitMessage(commitHash);
        if (message == commitMessage) {
            matchingCommits.push_back(commitHash);
        }
    }
    
    if (matchingCommits.empty()) {
        Utils::exitWithMessage("Found no commit with that message.");
    }
    
    for (const auto& commitHash : matchingCommits) {
        std::cout << commitHash << std::endl;
    }
}

void SomeObj::Impl::checkoutFile(const std::string& filename) {
    std::string commitHash = getHeadCommitHash();
    if (commitHash.empty()) {
        Utils::exitWithMessage("No commits yet.");
    }
    restoreFileFromCommit(commitHash, filename);
}

void SomeObj::Impl::checkoutFileInCommit(const std::string& commitId, const std::string& filename) {
    std::string fullCommitId = expandCommitId(commitId);
    if (fullCommitId.empty()) {
        Utils::exitWithMessage("No commit with that id exists.");
    }
    restoreFileFromCommit(fullCommitId, filename);
}

// ==================== Subtask3 方法 (checkout branch) ====================

void SomeObj::Impl::checkoutBranch(const std::string& branchName) {
    // 1. 检查分支是否存在
    std::string branchPath = gitliteDir + "/refs/heads/" + branchName;
    if (!Utils::exists(branchPath)) {
        Utils::exitWithMessage("No such branch exists.");
    }
    
    // 2. 检查是否已经是当前分支
    if (branchName == currentBranch) {
        Utils::exitWithMessage("No need to checkout the current branch.");
    }
    
    // 3. 获取目标分支的提交哈希
    std::string targetCommitHash = Utils::readContentsAsString(branchPath);
    if (!targetCommitHash.empty() && targetCommitHash.back() == '\n') {
        targetCommitHash.pop_back();
    }
    
    // 4. 获取当前分支的提交哈希
    std::string currentCommitHash = getHeadCommitHash();
    
    // 5. 获取目标提交的文件列表
    std::set<std::string> targetFiles;
    if (!targetCommitHash.empty() && targetCommitHash != "0") {
        std::string commitPath = objectsDir + "/" + targetCommitHash;
        if (Utils::exists(commitPath)) {
            std::string commitContent = Utils::readContentsAsString(commitPath);
            std::stringstream ss(commitContent);
            
            // 跳过前三行
            std::string line;
            std::getline(ss, line);  // 消息
            std::getline(ss, line);  // 父提交
            std::getline(ss, line);  // 时间戳
            
            int blobCount;
            ss >> blobCount;
            
            for (int i = 0; i < blobCount; ++i) {
                std::string blobHash, blobFile;
                ss >> blobHash >> blobFile;
                targetFiles.insert(blobFile);
            }
        }
    }
    
    // 6. 检查是否有未跟踪文件会被覆盖
    std::set<std::string> currentFiles;
    if (!currentCommitHash.empty() && currentCommitHash != "0") {
        std::string commitPath = objectsDir + "/" + currentCommitHash;
        if (Utils::exists(commitPath)) {
            std::string commitContent = Utils::readContentsAsString(commitPath);
            std::stringstream ss(commitContent);
            
            // 跳过前三行
            std::string line;
            std::getline(ss, line);  // 消息
            std::getline(ss, line);  // 父提交
            std::getline(ss, line);  // 时间戳
            
            int blobCount;
            ss >> blobCount;
            
            for (int i = 0; i < blobCount; ++i) {
                std::string blobHash, blobFile;
                ss >> blobHash >> blobFile;
                currentFiles.insert(blobFile);
            }
        }
    }
    
    // 检查未跟踪文件
    for (const auto& targetFile : targetFiles) {
        // 如果文件在目标提交中，但不在当前提交中，并且工作目录中存在
        if (currentFiles.find(targetFile) == currentFiles.end() && Utils::exists(targetFile)) {
            // 检查文件是否未被跟踪（不在暂存区）
            if (stagedFiles.find(targetFile) == stagedFiles.end()) {
                Utils::exitWithMessage("There is an untracked file in the way; delete it, or add and commit it first.");
            }
        }
    }
    
    // 7. 恢复目标提交的所有文件
    for (const auto& filename : targetFiles) {
        restoreFileFromCommit(targetCommitHash, filename);
    }
    
    // 8. 删除在当前分支中存在但在目标分支中不存在的文件
    for (const auto& filename : currentFiles) {
        if (targetFiles.find(filename) == targetFiles.end()) {
            if (Utils::exists(filename)) {
                Utils::restrictedDelete(filename);
            }
        }
    }
    
    // 9. 更新当前分支
    currentBranch = branchName;
    saveHead();
    
    // 10. 清空暂存区
    stagedFiles.clear();
    removedFiles.clear();
    saveStaging();
}

// ==================== Subtask4 方法 ====================

void SomeObj::Impl::branch(const std::string& branchName) {
    // 1. 检查分支是否已存在
    std::string branchPath = gitliteDir + "/refs/heads/" + branchName;
    if (Utils::exists(branchPath)) {
        Utils::exitWithMessage("A branch with that name already exists.");
    }
    
    // 2. 获取当前提交哈希
    std::string currentCommitHash = getHeadCommitHash();
    
    // 3. 创建新分支指向当前提交
    Utils::writeContents(branchPath, currentCommitHash + "\n");
}

void SomeObj::Impl::rmBranch(const std::string& branchName) {
    // 1. 检查分支是否存在
    std::string branchPath = gitliteDir + "/refs/heads/" + branchName;
    if (!Utils::exists(branchPath)) {
        Utils::exitWithMessage("A branch with that name does not exist.");
    }
    
    // 2. 检查是否是当前分支
    if (branchName == currentBranch) {
        Utils::exitWithMessage("Cannot remove the current branch.");
    }
    
    // 3. 删除分支
    Utils::restrictedDelete(branchPath);
}

void SomeObj::Impl::reset(const std::string& commitId) {
    // 1. 展开提交ID（如果提供的是短ID）
    std::string fullCommitId = expandCommitId(commitId);
    if (fullCommitId.empty()) {
        Utils::exitWithMessage("No commit with that id exists.");
    }
    
    // 2. 验证提交存在
    std::string commitPath = objectsDir + "/" + fullCommitId;
    if (!Utils::exists(commitPath)) {
        Utils::exitWithMessage("No commit with that id exists.");
    }
    
    // 3. 获取当前提交哈希
    std::string currentCommitHash = getHeadCommitHash();
    
    // 4. 获取目标提交的文件列表
    std::set<std::string> targetFiles;
    std::string commitContent = Utils::readContentsAsString(commitPath);
    std::stringstream ss(commitContent);
    
    // 跳过前三行
    std::string line;
    std::getline(ss, line);  // 消息
    std::getline(ss, line);  // 父提交
    std::getline(ss, line);  // 时间戳
    
    int blobCount;
    ss >> blobCount;
    
    for (int i = 0; i < blobCount; ++i) {
        std::string blobHash, blobFile;
        ss >> blobHash >> blobFile;
        targetFiles.insert(blobFile);
    }
    
    // 5. 获取当前提交的文件列表
    std::set<std::string> currentFiles;
    if (!currentCommitHash.empty() && currentCommitHash != "0") {
        std::string currentCommitPath = objectsDir + "/" + currentCommitHash;
        if (Utils::exists(currentCommitPath)) {
            std::string currentCommitContent = Utils::readContentsAsString(currentCommitPath);
            std::stringstream ssCurrent(currentCommitContent);
            
            // 跳过前三行
            std::getline(ssCurrent, line);  // 消息
            std::getline(ssCurrent, line);  // 父提交
            std::getline(ssCurrent, line);  // 时间戳
            
            int currentBlobCount;
            ssCurrent >> currentBlobCount;
            
            for (int i = 0; i < currentBlobCount; ++i) {
                std::string blobHash, blobFile;
                ssCurrent >> blobHash >> blobFile;
                currentFiles.insert(blobFile);
            }
        }
    }
    
    // 6. 检查是否有未跟踪文件会被覆盖
    for (const auto& targetFile : targetFiles) {
        // 如果文件在目标提交中，但不在当前提交中，并且工作目录中存在
        if (currentFiles.find(targetFile) == currentFiles.end() && Utils::exists(targetFile)) {
            // 检查文件是否未被跟踪（不在暂存区）
            if (stagedFiles.find(targetFile) == stagedFiles.end()) {
                Utils::exitWithMessage("There is an untracked file in the way; delete it, or add and commit it first.");
            }
        }
    }
    
    // 7. 恢复目标提交的所有文件
    for (const auto& filename : targetFiles) {
        restoreFileFromCommit(fullCommitId, filename);
    }
    
    // 8. 删除在当前分支中存在但在目标提交中不存在的文件
    for (const auto& filename : currentFiles) {
        if (targetFiles.find(filename) == targetFiles.end()) {
            if (Utils::exists(filename)) {
                Utils::restrictedDelete(filename);
            }
        }
    }
    
    // 9. 更新当前分支指向目标提交
    std::string branchPath = gitliteDir + "/refs/heads/" + currentBranch;
    Utils::writeContents(branchPath, fullCommitId + "\n");
    
    // 10. 清空暂存区
    stagedFiles.clear();
    removedFiles.clear();
    saveStaging();
}

// ==================== SomeObj 公共接口 ====================

SomeObj::SomeObj() : pImpl(std::make_unique<Impl>()) {}
SomeObj::~SomeObj() = default;

void SomeObj::init() { pImpl->init(); }
void SomeObj::add(const std::string& filename) { pImpl->add(filename); }
void SomeObj::commit(const std::string& message) { pImpl->commit(message); }
void SomeObj::rm(const std::string& filename) { pImpl->rm(filename); }
void SomeObj::status() { pImpl->status(); }
void SomeObj::log() { pImpl->log(); }
void SomeObj::globalLog() { pImpl->globalLog(); }
void SomeObj::find(const std::string& commitMessage) { pImpl->find(commitMessage); }
void SomeObj::checkoutFile(const std::string& filename) { pImpl->checkoutFile(filename); }
void SomeObj::checkoutFileInCommit(const std::string& commitId, const std::string& filename) { 
    pImpl->checkoutFileInCommit(commitId, filename); 
}
void SomeObj::checkoutBranch(const std::string& branchName) { pImpl->checkoutBranch(branchName); }
void SomeObj::branch(const std::string& branchName) { pImpl->branch(branchName); }
void SomeObj::rmBranch(const std::string& branchName) { pImpl->rmBranch(branchName); }
void SomeObj::reset(const std::string& commitId) { pImpl->reset(commitId); }
void SomeObj::merge(const std::string& branchName) { pImpl->merge(branchName); }
void SomeObj::addRemote(const std::string& remoteName, const std::string& directory) { pImpl->addRemote(remoteName, directory); }
void SomeObj::rmRemote(const std::string& remoteName) { pImpl->rmRemote(remoteName); }
void SomeObj::push(const std::string& remoteName, const std::string& branchName) { pImpl->push(remoteName, branchName); }
void SomeObj::fetch(const std::string& remoteName, const std::string& branchName) { pImpl->fetch(remoteName, branchName); }
void SomeObj::pull(const std::string& remoteName, const std::string& branchName) { pImpl->pull(remoteName, branchName); }