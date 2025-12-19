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
#include <queue>
#include <functional>

namespace fs = std::filesystem;

class SomeObj::Impl {
private:
    std::string gitliteDir = ".gitlite";
    std::string headPath;
    std::string objectsDir;
    std::string stagingPath;
    std::string remoteDir; // 远程仓库信息目录
    
    std::string currentBranch = "master";
    std::map<std::string, std::string> stagedFiles;  // filename -> blobHash
    std::set<std::string> removedFiles;
    std::map<std::string, std::string> remotes; // remoteName -> remotePath
    
    // 辅助方法
    std::string getHeadCommitHash() const;
    void saveHead();
    void loadHead();
    void saveStaging();
    void loadStaging();
    void saveRemotes();
    void loadRemotes();
    std::string formatTimestamp(const std::string& utcTimestamp) const;
    std::vector<std::string> getAllCommitHashes() const;
    std::string expandCommitId(const std::string& shortId) const;
    void restoreFileFromCommit(const std::string& commitHash, const std::string& filename) const;
    std::string getCommitMessage(const std::string& commitHash) const;
    void printCommitInfo(const std::string& commitHash, bool includeMergeInfo = true) const;
    std::pair<std::string, std::string> getCommitParents(const std::string& commitHash) const;

    std::string findSplitPoint(const std::string& commit1, const std::string& commit2) const;
    void checkUntrackedFilesForMerge(const std::string& currentCommit, 
                                     const std::string& givenCommit,
                                     const std::string& splitPoint) const;
    std::set<std::string> performMerge(const std::string& currentCommit,
                                       const std::string& givenCommit,
                                       const std::string& splitPoint,
                                       const std::string& branchName);
    std::map<std::string, std::string> getCommitFiles(const std::string& commitHash) const;
    bool filesEqual(const std::string& file1, const std::string& file2) const;
    
    // 远程相关辅助方法
    std::string getRemoteBranchHash(const std::string& remoteName, const std::string& branchName) const;
    void copyObjectIfNotExists(const std::string& objectHash, const std::string& remoteObjectsDir) const;
    void copyCommitAndBlobs(const std::string& commitHash, const std::string& remoteObjectsDir) const;
    bool isAncestor(const std::string& ancestor, const std::string& descendant) const;
    
public:
    Impl();
    
    void init();
    void add(const std::string& filename);
    void commit(const std::string& message, const std::string& secondParent = "");
    void rm(const std::string& filename);
    void status();
    void log();
    void globalLog();
    void find(const std::string& commitMessage);
    void checkoutFile(const std::string& filename);
    void checkoutFileInCommit(const std::string& commitId, const std::string& filename);
    void checkoutBranch(const std::string&);
    void branch(const std::string&);
    void rmBranch(const std::string&);
    void reset(const std::string&);
    void merge(const std::string&);

    // 远程方法
    void addRemote(const std::string& remoteName, const std::string& directory);
    void rmRemote(const std::string& remoteName);
    void push(const std::string& remoteName, const std::string& branchName);
    void fetch(const std::string& remoteName, const std::string& branchName);
    void pull(const std::string& remoteName, const std::string& branchName);
};

// ==================== 构造函数和基础方法 ====================

SomeObj::Impl::Impl() {
    headPath = gitliteDir + "/HEAD";
    objectsDir = gitliteDir + "/objects";
    stagingPath = gitliteDir + "/STAGING";
    remoteDir = gitliteDir + "/remotes";
    
    if (Utils::exists(gitliteDir)) {
        loadHead();
        loadStaging();
        loadRemotes();
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

void SomeObj::Impl::saveRemotes() {
    Utils::createDirectories(remoteDir);
    std::stringstream ss;
    ss << remotes.size() << "\n";
    for (const auto& [remoteName, remotePath] : remotes) {
        ss << remoteName << "\n" << remotePath << "\n";
    }
    Utils::writeContents(remoteDir + "/REMOTES", ss.str());
}

void SomeObj::Impl::loadRemotes() {
    std::string remotesPath = remoteDir + "/REMOTES";
    if (!Utils::exists(remotesPath)) return;
    
    std::string content = Utils::readContentsAsString(remotesPath);
    std::stringstream ss(content);
    
    remotes.clear();
    
    size_t remoteCount;
    ss >> remoteCount;
    
    for (size_t i = 0; i < remoteCount; ++i) {
        std::string remoteName, remotePath;
        ss >> remoteName >> remotePath;
        remotes[remoteName] = remotePath;
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
    
    remotes.clear();
    saveRemotes();
}

void SomeObj::Impl::add(const std::string& filename) {
    if (!Utils::exists(filename)) {
        Utils::exitWithMessage("File does not exist.");
    }

    // 读取文件内容并计算哈希
    std::string content = Utils::readContentsAsString(filename);
    std::string hash = Utils::sha1(content);

    // 保存 blob 对象
    std::string blobPath = objectsDir + "/" + hash;
    if (!Utils::exists(blobPath)) {
        Utils::writeContents(blobPath, content);
    }

    // 获取当前提交哈希
    std::string currentCommitHash = getHeadCommitHash();
    bool sameAsCommit = false;

    if (!currentCommitHash.empty() && currentCommitHash != "0") {
        std::string commitPath = objectsDir + "/" + currentCommitHash;
        if (Utils::exists(commitPath)) {
            std::string commitContent = Utils::readContentsAsString(commitPath);
            std::stringstream ss(commitContent);

            std::string line;
            std::getline(ss, line); // 消息
            std::getline(ss, line); // 父提交
            std::getline(ss, line); // 时间戳

            int blobCount = 0;
            if (ss >> blobCount) {
                ss.ignore(); // 跳过换行
                for (int i = 0; i < blobCount; ++i) {
                    std::string blobHash, blobFile;
                    if (!std::getline(ss, line)) break;
                    std::istringstream iss(line);
                    if (!(iss >> blobHash >> blobFile)) continue;
                    if (blobFile == filename) {
                        sameAsCommit = (blobHash == hash);
                        break;
                    }
                }
            }
        }
    }

    if (sameAsCommit) {
        stagedFiles.erase(filename);
    } else {
        stagedFiles[filename] = hash;
    }

    removedFiles.erase(filename);
    saveStaging();
}

void SomeObj::Impl::commit(const std::string& message, const std::string& secondParent) {
    if (message.empty()) {
        Utils::exitWithMessage("Please enter a commit message.");
    }
    
    // 检查是否有文件被暂存（合并提交时可能有例外）
    if (stagedFiles.empty() && removedFiles.empty() && secondParent.empty()) {
        Utils::exitWithMessage("No changes added to the commit.");
    }
    
    std::string parentHash = getHeadCommitHash();
    
    std::stringstream commitData;
    commitData << message << "\n";
    
    // 写入父提交（合并提交有两个父提交）
    if (parentHash.empty() || !Utils::exists(objectsDir + "/" + parentHash)) {
        commitData << "0\n";
    } else {
        commitData << parentHash << "\n";
    }
    
    // 如果是合并提交，添加第二个父提交
    if (!secondParent.empty()) {
        commitData << secondParent << "\n";
    }
    
    // 时间戳
    std::time_t now = std::time(nullptr);
    std::tm* gmt = std::gmtime(&now);
    char timeBuffer[100];
    std::strftime(timeBuffer, sizeof(timeBuffer), "%a %b %d %H:%M:%S %Y +0000", gmt);
    commitData << timeBuffer << "\n";
    
    // 收集blob信息
    std::map<std::string, std::string> blobs;
    
    // 从第一个父提交继承blob（如果存在且不是初始提交）
    if (!parentHash.empty() && parentHash != "0") {
        std::string commitPath = objectsDir + "/" + parentHash;
        if (Utils::exists(commitPath)) {
            std::string commitContent = Utils::readContentsAsString(commitPath);
            std::stringstream ss(commitContent);
            
            // 跳过消息行
            std::string line;
            std::getline(ss, line);  // 消息
            
            // 跳过父提交行（可能有两行）
            std::getline(ss, line);  // 第一个父提交
            // 检查是否有第二个父提交
            std::getline(ss, line);
            if (line.find(":") == std::string::npos) {
                // 这是第二个父提交，跳过时间戳行
                std::getline(ss, line);
            }
            // 现在line是时间戳
            
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
    // 直接返回UTC时间戳，不进行转换
    // 初始提交的时间戳已经是UTC时间
    return utcTimestamp;
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

// ==================== 改进的status方法 ====================

void SomeObj::Impl::status() {
    // === Branches ===
    std::cout << "=== Branches ===" << std::endl;
    std::cout << "*" << currentBranch << std::endl;
    
    std::string branchesDir = gitliteDir + "/refs/heads";
    if (Utils::exists(branchesDir)) {
        std::set<std::string> otherBranches;
        for (const auto& entry : fs::directory_iterator(branchesDir)) {
            std::string branchName = entry.path().filename().string();
            if (branchName != currentBranch) {
                otherBranches.insert(branchName);
            }
        }
        for (const auto& branchName : otherBranches) {
            std::cout << branchName << std::endl;
        }
    }
    
    // === Staged Files ===
    std::cout << std::endl << "=== Staged Files ===" << std::endl;
    std::set<std::string> stagedFileNames;
    for (const auto& [filename, hash] : stagedFiles) {
        stagedFileNames.insert(filename);
    }
    for (const auto& filename : stagedFileNames) {
        std::cout << filename << std::endl;
    }
    
    // === Removed Files ===
    std::cout << std::endl << "=== Removed Files ===" << std::endl;
    std::set<std::string> removedFileSet(removedFiles.begin(), removedFiles.end());
    for (const auto& filename : removedFileSet) {
        std::cout << filename << std::endl;
    }
    
    // === Modifications Not Staged For Commit ===
    std::cout << std::endl << "=== Modifications Not Staged For Commit ===" << std::endl;
    
    // 获取当前提交的文件
    std::string currentCommitHash = getHeadCommitHash();
    std::map<std::string, std::string> commitFiles;
    
    if (!currentCommitHash.empty() && currentCommitHash != "0") {
        commitFiles = getCommitFiles(currentCommitHash);
    }
    
    // 获取工作目录中所有普通文件
    std::set<std::string> workingDirFiles;
    try {
        for (const auto& entry : fs::directory_iterator(".")) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (filename != ".gitlite" && filename[0] != '.') {
                    workingDirFiles.insert(filename);
                }
            }
        }
    } catch (...) {
        // 忽略错误
    }
    
    std::set<std::string> modifications;
    
    // 1. 在当前提交中跟踪，在工作目录中更改，但未暂存
    for (const auto& [filename, commitHash] : commitFiles) {
        if (workingDirFiles.find(filename) != workingDirFiles.end()) {
            // 文件在工作目录中存在
            std::string workingContent = Utils::readContentsAsString(filename);
            std::string workingHash = Utils::sha1(workingContent);
            
            // 检查是否在暂存区
            bool isStaged = (stagedFiles.find(filename) != stagedFiles.end());
            
            if (!isStaged) {
                // 不在暂存区，且内容与提交不同
                if (workingHash != commitHash) {
                    modifications.insert(filename + " (modified)");
                }
            }
        }
    }
    
    // 2. 已保存在添加暂存区，但内容与工作目录不同
    for (const auto& [filename, stagedHash] : stagedFiles) {
        if (workingDirFiles.find(filename) != workingDirFiles.end()) {
            // 文件在工作目录中存在
            std::string workingContent = Utils::readContentsAsString(filename);
            std::string workingHash = Utils::sha1(workingContent);
            
            if (workingHash != stagedHash) {
                modifications.insert(filename + " (modified)");
            }
        }
    }
    
    // 3. 已保存在添加暂存区，但在工作目录中已删除
    for (const auto& [filename, stagedHash] : stagedFiles) {
        if (workingDirFiles.find(filename) == workingDirFiles.end()) {
            // 文件不在工作目录中
            modifications.insert(filename + " (deleted)");
        }
    }
    
    // 4. 未在删除暂存区，但在当前提交中被跟踪并已从工作目录中删除
    for (const auto& [filename, commitHash] : commitFiles) {
        if (workingDirFiles.find(filename) == workingDirFiles.end()) {
            // 文件不在工作目录中
            bool isStaged = (stagedFiles.find(filename) != stagedFiles.end());
            bool isRemoved = (removedFiles.find(filename) != removedFiles.end());
            
            if (!isStaged && !isRemoved) {
                modifications.insert(filename + " (deleted)");
            }
        }
    }
    
    // 输出修改
    for (const auto& modification : modifications) {
        std::cout << modification << std::endl;
    }
    
    // === Untracked Files ===
    std::cout << std::endl << "=== Untracked Files ===" << std::endl;
    
    std::set<std::string> untrackedFiles;
    
    for (const auto& filename : workingDirFiles) {
        // 检查是否在提交中
        bool inCommit = (commitFiles.find(filename) != commitFiles.end());
        
        // 检查是否在暂存区
        bool inStaged = (stagedFiles.find(filename) != stagedFiles.end());
        
        // 检查是否在删除暂存区
        bool inRemoved = (removedFiles.find(filename) != removedFiles.end());
        
        // 未跟踪文件：既不在提交中，也不在暂存区
        if (!inCommit && !inStaged) {
            untrackedFiles.insert(filename);
        }
        
        // 特殊情况：已暂存待删除但随后重新创建的文件
        if (inRemoved) {
            untrackedFiles.insert(filename);
        }
    }
    
    // 输出未跟踪文件
    for (const auto& filename : untrackedFiles) {
        std::cout << filename << std::endl;
    }
}

// ==================== Subtask2 主要方法 ====================

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
    // 检查是否是远程分支格式（remote/branch）
    bool isRemoteFormat = false;
    size_t slashPos = branchName.find('/');
    if (slashPos != std::string::npos) {
        isRemoteFormat = true;
    }
    
    // 检查分支是否存在（包括本地分支和远程分支引用）
    std::string branchPath = gitliteDir + "/refs/heads/" + branchName;
    if (!Utils::exists(branchPath)) {
        Utils::exitWithMessage("No such branch exists.");
    }
    
    // 检查是否已经是当前分支
    if (branchName == currentBranch) {
        Utils::exitWithMessage("No need to checkout the current branch.");
    }
    
    // 获取目标分支的提交哈希
    std::string targetCommitHash = Utils::readContentsAsString(branchPath);
    if (!targetCommitHash.empty() && targetCommitHash.back() == '\n') {
        targetCommitHash.pop_back();
    }
    
    // 获取当前分支的提交哈希
    std::string currentCommitHash = getHeadCommitHash();
    
    // 获取目标提交的文件列表
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
    
    // 获取当前提交的文件列表
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
    
    // 恢复目标提交的所有文件
    for (const auto& filename : targetFiles) {
        restoreFileFromCommit(targetCommitHash, filename);
    }
    
    // 删除在当前分支中存在但在目标分支中不存在的文件
    for (const auto& filename : currentFiles) {
        if (targetFiles.find(filename) == targetFiles.end()) {
            if (Utils::exists(filename)) {
                Utils::restrictedDelete(filename);
            }
        }
    }
    
    // 更新当前分支
    // 注意：对于远程分支格式（如 R1/master），我们也将其设置为当前分支
    currentBranch = branchName;
    saveHead();
    
    // 清空暂存区
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
    
    // 3. 检查是否是远程分支（格式为 remote/branch）
    size_t slashPos = branchName.find('/');
    if (slashPos != std::string::npos) {
        std::string remoteName = branchName.substr(0, slashPos);
        if (remotes.find(remoteName) != remotes.end()) {
            // 这是远程分支，不能直接删除
            Utils::exitWithMessage("Cannot remove a remote branch directly. Use rm-remote instead.");
        }
    }
    
    // 4. 删除分支文件
    // 直接删除文件，不调用Utils::restrictedDelete
    if (std::remove(branchPath.c_str()) != 0) {
        // 如果删除失败，可能是因为文件被锁定或其他原因
        // 但根据测试要求，我们只需要尝试删除
    }
}

void SomeObj::Impl::reset(const std::string& commitId) {
    //  展开提交ID（如果提供的是短ID）
    std::string fullCommitId = expandCommitId(commitId);
    if (fullCommitId.empty()) {
        Utils::exitWithMessage("No commit with that id exists.");
    }
    
    //  验证提交存在
    std::string commitPath = objectsDir + "/" + fullCommitId;
    if (!Utils::exists(commitPath)) {
        Utils::exitWithMessage("No commit with that id exists.");
    }
    
    //  获取当前提交哈希
    std::string currentCommitHash = getHeadCommitHash();
    
    //  获取目标提交的文件列表
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
    
    //  获取当前提交的文件列表
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
    
    //  检查是否有未跟踪文件会被覆盖
    for (const auto& targetFile : targetFiles) {
        // 如果文件在目标提交中，但不在当前提交中，并且工作目录中存在
        if (currentFiles.find(targetFile) == currentFiles.end() && Utils::exists(targetFile)) {
            // 检查文件是否未被跟踪（不在暂存区）
            if (stagedFiles.find(targetFile) == stagedFiles.end()) {
                Utils::exitWithMessage("There is an untracked file in the way; delete it, or add and commit it first.");
            }
        }
    }
    
    //  恢复目标提交的所有文件
    for (const auto& filename : targetFiles) {
        restoreFileFromCommit(fullCommitId, filename);
    }
    
    //  删除在当前分支中存在但在目标提交中不存在的文件
    for (const auto& filename : currentFiles) {
        if (targetFiles.find(filename) == targetFiles.end()) {
            if (Utils::exists(filename)) {
                Utils::restrictedDelete(filename);
            }
        }
    }
    
    //  更新当前分支指向目标提交
    std::string branchPath = gitliteDir + "/refs/heads/" + currentBranch;
    Utils::writeContents(branchPath, fullCommitId + "\n");
    
    //  清空暂存区
    stagedFiles.clear();
    removedFiles.clear();
    saveStaging();
}

// ==================== Subtask5 辅助方法 ====================
// 寻找两个提交的最低公共祖先
std::string SomeObj::Impl::findSplitPoint(const std::string& commit1, const std::string& commit2) const {
    // 使用深度优先搜索找到所有祖先
    std::function<void(const std::string&, std::set<std::string>&, int)> collectAncestors = 
        [&](const std::string& commit, std::set<std::string>& ancestors, int depth) {
            if (commit.empty() || commit == "0" || depth > 100) return;
            
            ancestors.insert(commit);
            
            std::string commitPath = objectsDir + "/" + commit;
            if (!Utils::exists(commitPath)) return;
            
            std::string content = Utils::readContentsAsString(commitPath);
            std::stringstream ss(content);
            
            // 跳过消息
            std::string line;
            std::getline(ss, line);
            
            // 读取父提交
            std::getline(ss, line);
            std::string parent1 = line;
            
            // 检查是否有第二个父提交
            std::string parent2 = "";
            std::getline(ss, line);
            if (line.find(":") == std::string::npos) {
                parent2 = line;
            }
            
            // 递归处理父提交
            if (!parent1.empty() && parent1 != "0") {
                collectAncestors(parent1, ancestors, depth + 1);
            }
            if (!parent2.empty() && parent2 != "0") {
                collectAncestors(parent2, ancestors, depth + 1);
            }
        };
    
    // 收集commit1的所有祖先
    std::set<std::string> ancestors1;
    collectAncestors(commit1, ancestors1, 0);
    
    // 使用BFS从commit2开始寻找第一个在ancestors1中的提交
    std::queue<std::string> queue;
    std::set<std::string> visited;
    
    queue.push(commit2);
    visited.insert(commit2);
    
    while (!queue.empty()) {
        std::string current = queue.front();
        queue.pop();
        
        if (ancestors1.find(current) != ancestors1.end()) {
            return current;
        }
        
        if (current.empty() || current == "0") continue;
        
        std::string commitPath = objectsDir + "/" + current;
        if (!Utils::exists(commitPath)) continue;
        
        std::string content = Utils::readContentsAsString(commitPath);
        std::stringstream ss(content);
        
        // 跳过消息
        std::string line;
        std::getline(ss, line);
        
        // 读取父提交
        std::getline(ss, line);
        std::string parent1 = line;
        
        // 检查是否有第二个父提交
        std::string parent2 = "";
        std::getline(ss, line);
        if (line.find(":") == std::string::npos) {
            parent2 = line;
        }
        
        // 添加父提交到队列
        if (!parent1.empty() && parent1 != "0" && visited.find(parent1) == visited.end()) {
            queue.push(parent1);
            visited.insert(parent1);
        }
        if (!parent2.empty() && parent2 != "0" && visited.find(parent2) == visited.end()) {
            queue.push(parent2);
            visited.insert(parent2);
        }
    }
    
    return "0"; // 返回初始提交
}

// 获取提交中的所有文件
std::map<std::string, std::string> SomeObj::Impl::getCommitFiles(const std::string& commitHash) const {
    std::map<std::string, std::string> files;
    
    if (commitHash.empty() || commitHash == "0") {
        return files;
    }
    
    std::string commitPath = objectsDir + "/" + commitHash;
    if (!Utils::exists(commitPath)) {
        return files;
    }
    
    std::string content = Utils::readContentsAsString(commitPath);
    std::stringstream ss(content);
    
    // 跳过消息
    std::string line;
    std::getline(ss, line);
    
    // 跳过父提交（可能有1个或2个）
    std::getline(ss, line); // 第一个父提交
    std::getline(ss, line); // 可能是第二个父提交或时间戳
    
    // 如果是第二个父提交，再读一行获取时间戳
    if (line.find(":") == std::string::npos) {
        std::getline(ss, line); // 时间戳
    }
    // 现在line是时间戳
    
    int blobCount;
    ss >> blobCount;
    
    for (int i = 0; i < blobCount; ++i) {
        std::string blobHash, filename;
        ss >> blobHash >> filename;
        files[filename] = blobHash;
    }
    
    return files;
}

// 检查两个文件是否相等
bool SomeObj::Impl::filesEqual(const std::string& file1, const std::string& file2) const {
    return file1 == file2;
}

// 检查未跟踪文件冲突
void SomeObj::Impl::checkUntrackedFilesForMerge(const std::string& currentCommit,
                                               const std::string& givenCommit,
                                               const std::string& splitPoint) const {
    auto splitFiles = getCommitFiles(splitPoint);
    auto currentFiles = getCommitFiles(currentCommit);
    auto givenFiles = getCommitFiles(givenCommit);
    
    // 检查所有在给定分支中存在但在分割点或当前分支中不存在的文件
    for (const auto& [filename, hash] : givenFiles) {
        bool inSplit = (splitFiles.find(filename) != splitFiles.end());
        bool inCurrent = (currentFiles.find(filename) != currentFiles.end());
        
        // 如果文件在给定分支中但不在分割点或当前分支中，并且工作目录中存在
        if ((!inSplit || !inCurrent) && Utils::exists(filename)) {
            // 检查是否未被跟踪（不在暂存区）
            if (stagedFiles.find(filename) == stagedFiles.end()) {
                Utils::exitWithMessage("There is an untracked file in the way; delete it, or add and commit it first.");
            }
        }
    }
}

// 执行合并操作
std::set<std::string> SomeObj::Impl::performMerge(const std::string& currentCommit,
                                                  const std::string& givenCommit,
                                                  const std::string& splitPoint,
                                                  const std::string& branchName) {
    std::set<std::string> conflictFiles;
    
    auto splitFiles = getCommitFiles(splitPoint);
    auto currentFiles = getCommitFiles(currentCommit);
    auto givenFiles = getCommitFiles(givenCommit);
    
    // 收集所有涉及的文件名
    std::set<std::string> allFiles;
    for (const auto& [filename, hash] : splitFiles) allFiles.insert(filename);
    for (const auto& [filename, hash] : currentFiles) allFiles.insert(filename);
    for (const auto& [filename, hash] : givenFiles) allFiles.insert(filename);
    
    // 遍历所有文件
    for (const auto& filename : allFiles) {
        bool inSplit = (splitFiles.find(filename) != splitFiles.end());
        bool inCurrent = (currentFiles.find(filename) != currentFiles.end());
        bool inGiven = (givenFiles.find(filename) != givenFiles.end());
        
        std::string splitHash = inSplit ? splitFiles[filename] : "";
        std::string currentHash = inCurrent ? currentFiles[filename] : "";
        std::string givenHash = inGiven ? givenFiles[filename] : "";
        
        // 情况1: 在给定分支中被修改，在当前分支中未修改
        if (inSplit && inCurrent && inGiven) {
            if (currentHash == splitHash && givenHash != splitHash) {
                // 从给定分支恢复文件
                restoreFileFromCommit(givenCommit, filename);
                add(filename); // 自动暂存
                continue;
            }
        }
        
        // 情况2: 在当前分支中被修改，在给定分支中未修改
        if (inSplit && inCurrent && inGiven) {
            if (givenHash == splitHash && currentHash != splitHash) {
                // 保持当前版本不变
                continue;
            }
        }
        
        // 情况3: 在两个分支中以相同方式修改
        if (inSplit && inCurrent && inGiven) {
            if (currentHash == givenHash) {
                // 文件保持不变
                continue;
            }
        }
        
        // 情况4: 仅在给定分支中存在（分割点不存在）
        if (!inSplit && !inCurrent && inGiven) {
            // 从给定分支恢复文件并暂存
            restoreFileFromCommit(givenCommit, filename);
            add(filename);
            continue;
        }
        
        // 情况5: 仅在当前分支中存在（分割点不存在）
        if (!inSplit && inCurrent && !inGiven) {
            // 保持原样
            continue;
        }
        
        // 情况6: 在分割点存在，在当前分支中未修改，在给定分支中被删除
        if (inSplit && inCurrent && !inGiven) {
            if (currentHash == splitHash) {
                // 删除文件
                if (Utils::exists(filename)) {
                    Utils::restrictedDelete(filename);
                }
                // 不跟踪该文件
                continue;
            }
        }
        
        // 情况7: 在分割点存在，在给定分支中未修改，在当前分支中被删除
        if (inSplit && !inCurrent && inGiven) {
            if (givenHash == splitHash) {
                // 保持不被跟踪和暂存
                continue;
            }
        }
        
        // 情况8: 冲突 - 在两个分支中以不同方式修改
        bool isConflict = false;
        
        if (inSplit) {
            // 文件在分割点存在
            if (inCurrent && inGiven) {
                // 在两个分支中都存在但修改不同
                if (currentHash != givenHash && currentHash != splitHash && givenHash != splitHash) {
                    isConflict = true;
                }
            } else if (inCurrent && !inGiven) {
                // 在当前分支中修改或删除，在给定分支中删除
                if (currentHash != splitHash) {
                    isConflict = true;
                }
            } else if (!inCurrent && inGiven) {
                // 在当前分支中删除，在给定分支中修改或删除
                if (givenHash != splitHash) {
                    isConflict = true;
                }
            }
        } else {
            // 文件在分割点不存在
            if (inCurrent && inGiven) {
                // 在两个分支中都存在但内容不同
                if (currentHash != givenHash) {
                    isConflict = true;
                }
            }
        }
        
        if (isConflict) {
            conflictFiles.insert(filename);
            // 解决冲突
            std::string currentContent = "";
            std::string givenContent = "";
            
            if (inCurrent) {
                std::string blobPath = objectsDir + "/" + currentHash;
                if (Utils::exists(blobPath)) {
                    currentContent = Utils::readContentsAsString(blobPath);
                }
            }
            
            if (inGiven) {
                std::string blobPath = objectsDir + "/" + givenHash;
                if (Utils::exists(blobPath)) {
                    givenContent = Utils::readContentsAsString(blobPath);
                }
            }
            
            // 创建冲突标记
            std::string conflictContent = "<<<<<<< HEAD\n";
            conflictContent += currentContent;
            conflictContent += "=======\n";
            conflictContent += givenContent;
            conflictContent += ">>>>>>>\n";
            
            Utils::writeContents(filename, conflictContent);
            add(filename); // 自动暂存冲突文件
        }
    }
    
    return conflictFiles;
}

// ==================== Subtask5 方法 ====================
void SomeObj::Impl::merge(const std::string& branchName) {
    // 1. 检查是否有未提交的更改
    if (!stagedFiles.empty() || !removedFiles.empty()) {
        Utils::exitWithMessage("You have uncommitted changes.");
    }
    
    // 2. 检查分支是否存在
    std::string givenBranchPath = gitliteDir + "/refs/heads/" + branchName;
    if (!Utils::exists(givenBranchPath)) {
        Utils::exitWithMessage("A branch with that name does not exist.");
    }
    
    // 3. 检查是否合并自身
    if (branchName == currentBranch) {
        Utils::exitWithMessage("Cannot merge a branch with itself.");
    }
    
    // 4. 获取当前分支和给定分支的提交哈希
    std::string currentCommitHash = getHeadCommitHash();
    std::string givenCommitHash = Utils::readContentsAsString(givenBranchPath);
    if (!givenCommitHash.empty() && givenCommitHash.back() == '\n') {
        givenCommitHash.pop_back();
    }
    
    // 5. 寻找分割点
    std::string splitPoint = findSplitPoint(currentCommitHash, givenCommitHash);
    
    // 6. 检查特殊情况
    if (splitPoint == givenCommitHash) {
        std::cout << "Given branch is an ancestor of the current branch." << std::endl;
        return;
    }
    
    if (splitPoint == currentCommitHash) {
        checkoutBranch(branchName);
        std::cout << "Current branch fast-forwarded." << std::endl;
        return;
    }
    
    // 7. 获取三个提交的文件状态
    auto splitFiles = getCommitFiles(splitPoint);
    auto currentFiles = getCommitFiles(currentCommitHash);
    auto givenFiles = getCommitFiles(givenCommitHash);
    
    // 8. 检查未跟踪文件冲突
    for (const auto& [filename, hash] : givenFiles) {
        bool inCurrent = (currentFiles.find(filename) != currentFiles.end());
        bool inSplit = (splitFiles.find(filename) != splitFiles.end());
        
        // 如果文件在给定分支中但不在当前提交或分割点中
        if (!inCurrent || !inSplit) {
            // 检查工作目录中是否有未跟踪的同名文件
            if (Utils::exists(filename)) {
                // 检查是否未被跟踪（不在暂存区）且不被当前提交跟踪
                if (stagedFiles.find(filename) == stagedFiles.end() && 
                    currentFiles.find(filename) == currentFiles.end()) {
                    Utils::exitWithMessage("There is an untracked file in the way; delete it, or add and commit it first.");
                }
            }
        }
    }
    
    // 9. 执行合并，跟踪修改和冲突
    bool hasConflict = false;
    
    // 收集所有涉及的文件
    std::set<std::string> allFiles;
    for (const auto& [f, h] : splitFiles) allFiles.insert(f);
    for (const auto& [f, h] : currentFiles) allFiles.insert(f);
    for (const auto& [f, h] : givenFiles) allFiles.insert(f);
    
    // 清空当前暂存区（合并会创建新的暂存状态）
    std::map<std::string, std::string> newStagedFiles;
    std::set<std::string> newRemovedFiles;
    
    for (const auto& filename : allFiles) {
        bool inSplit = (splitFiles.find(filename) != splitFiles.end());
        bool inCurrent = (currentFiles.find(filename) != currentFiles.end());
        bool inGiven = (givenFiles.find(filename) != givenFiles.end());
        
        std::string splitHash = inSplit ? splitFiles[filename] : "";
        std::string currentHash = inCurrent ? currentFiles[filename] : "";
        std::string givenHash = inGiven ? givenFiles[filename] : "";
        
        // 情况1: 在给定分支中被修改，在当前分支中未修改
        if (inSplit && inCurrent && inGiven) {
            if (currentHash == splitHash && givenHash != splitHash) {
                // 从给定分支恢复文件
                restoreFileFromCommit(givenCommitHash, filename);
                newStagedFiles[filename] = givenHash;
                continue;
            }
        }
        
        // 情况2: 仅在给定分支中存在（分割点不存在）
        if (!inSplit && !inCurrent && inGiven) {
            // 恢复给定分支的版本
            restoreFileFromCommit(givenCommitHash, filename);
            newStagedFiles[filename] = givenHash;
            continue;
        }
        
        // 情况3: 在分割点存在，在当前分支中未修改，在给定分支中被删除
        if (inSplit && inCurrent && !inGiven) {
            if (currentHash == splitHash) {
                // 删除文件
                if (Utils::exists(filename)) {
                    Utils::restrictedDelete(filename);
                }
                newRemovedFiles.insert(filename);
                continue;
            }
        }
        
        // 情况4: 在当前分支中被修改，在给定分支中未修改 - 保持原样
        if (inSplit && inCurrent && inGiven) {
            if (givenHash == splitHash && currentHash != splitHash) {
                // 保持当前版本，不需要暂存
                continue;
            }
        }
        
        // 情况5: 在两个分支中以相同方式修改 - 保持不变
        if (inSplit && inCurrent && inGiven) {
            if (currentHash == givenHash) {
                // 文件保持不变
                continue;
            }
        }
        
        // 情况6: 冲突检测
        bool isConflict = false;
        
        if (inSplit) {
            if (inCurrent && inGiven) {
                // 在两个分支中都修改了，但方式不同
                if (currentHash != givenHash && (currentHash != splitHash || givenHash != splitHash)) {
                    isConflict = true;
                }
            } else if (inCurrent && !inGiven) {
                // 在当前分支中修改或删除，在给定分支中删除
                if (currentHash != splitHash) {
                    isConflict = true;
                }
            } else if (!inCurrent && inGiven) {
                // 在当前分支中删除，在给定分支中修改
                if (givenHash != splitHash) {
                    isConflict = true;
                }
            }
        } else {
            // 文件在分割点不存在
            if (inCurrent && inGiven && currentHash != givenHash) {
                isConflict = true;
            }
        }
        
        if (isConflict) {
            hasConflict = true;
            // 解决冲突
            std::string currentContent = "";
            std::string givenContent = "";
            
            if (inCurrent) {
                std::string blobPath = objectsDir + "/" + currentHash;
                if (Utils::exists(blobPath)) {
                    currentContent = Utils::readContentsAsString(blobPath);
                }
            }
            
            if (inGiven) {
                std::string blobPath = objectsDir + "/" + givenHash;
                if (Utils::exists(blobPath)) {
                    givenContent = Utils::readContentsAsString(blobPath);
                }
            }
            
            // 创建冲突标记
            std::ostringstream conflictContent;
            conflictContent << "<<<<<<< HEAD\n";
            conflictContent << currentContent;
            if (!currentContent.empty() && currentContent.back() != '\n') {
                conflictContent << "\n";
            }
            conflictContent << "=======\n";
            conflictContent << givenContent;
            if (!givenContent.empty() && givenContent.back() != '\n') {
                conflictContent << "\n";
            }
            conflictContent << ">>>>>>>\n";
            
            std::string conflictStr = conflictContent.str();
            Utils::writeContents(filename, conflictStr);
            
            // 计算并保存冲突文件的blob
            std::string conflictHash = Utils::sha1(conflictStr);
            std::string blobPath = objectsDir + "/" + conflictHash;
            if (!Utils::exists(blobPath)) {
                Utils::writeContents(blobPath, conflictStr);
            }
            
            newStagedFiles[filename] = conflictHash;
        }
    }
    
    // 10. 更新暂存区
    stagedFiles = newStagedFiles;
    removedFiles = newRemovedFiles;
    
    // 11. 创建合并提交（无论是否有冲突）
    std::string message = "Merged " + branchName + " into " + currentBranch + ".";
    
    std::string parentHash = getHeadCommitHash();
    
    std::stringstream commitData;
    commitData << message << "\n";
    
    // 写入父提交（两个）
    if (parentHash.empty() || parentHash == "0") {
        commitData << "0\n";
    } else {
        commitData << parentHash << "\n";
    }
    
    // 第二个父提交
    commitData << givenCommitHash << "\n";
    
    // 时间戳
    std::time_t now = std::time(nullptr);
    std::tm* gmt = std::gmtime(&now);
    char timeBuffer[100];
    std::strftime(timeBuffer, sizeof(timeBuffer), "%a %b %d %H:%M:%S %Y +0000", gmt);
    commitData << timeBuffer << "\n";
    
    // 收集blob信息
    std::map<std::string, std::string> blobs;
    
    // 从第一个父提交继承blob
    if (!parentHash.empty() && parentHash != "0") {
        std::string commitPath = objectsDir + "/" + parentHash;
        if (Utils::exists(commitPath)) {
            std::string commitContent = Utils::readContentsAsString(commitPath);
            std::stringstream ss(commitContent);
            
            std::string line;
            std::getline(ss, line);  // 消息
            std::getline(ss, line);  // 第一个父提交
            std::getline(ss, line);  // 第二个父提交或时间戳
            if (line.find(":") == std::string::npos) {
                std::getline(ss, line);  // 时间戳
            }
            
            int blobCount;
            ss >> blobCount;
            
            for (int i = 0; i < blobCount; ++i) {
                std::string blobHash, blobFile;
                ss >> blobHash >> blobFile;
                blobs[blobFile] = blobHash;
            }
        }
    }
    
    // 添加暂存的文件（包括冲突文件）
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
    
    // 12. 清空暂存区
    stagedFiles.clear();
    removedFiles.clear();
    saveStaging();
    
    // 13. 处理结果
    if (hasConflict) {
        std::cout << "Encountered a merge conflict." << std::endl;
    }
    // 注意：不在merge命令中打印log，log命令会在后续调用时显示
}

// ==================== 远程相关辅助方法 ====================

std::string SomeObj::Impl::getRemoteBranchHash(const std::string& remoteName, const std::string& branchName) const {
    auto it = remotes.find(remoteName);
    if (it == remotes.end()) {
        return "";
    }
    
    std::string remotePath = it->second;
    std::string remoteBranchPath = remotePath + "/refs/heads/" + branchName;
    
    if (!Utils::exists(remoteBranchPath)) {
        return "";
    }
    
    std::string content = Utils::readContentsAsString(remoteBranchPath);
    if (!content.empty() && content.back() == '\n') {
        content.pop_back();
    }
    
    return content;
}

void SomeObj::Impl::copyObjectIfNotExists(const std::string& objectHash, const std::string& remoteObjectsDir) const {
    std::string localObjectPath = objectsDir + "/" + objectHash;
    std::string remoteObjectPath = remoteObjectsDir + "/" + objectHash;
    
    if (!Utils::exists(remoteObjectPath) && Utils::exists(localObjectPath)) {
        std::string content = Utils::readContentsAsString(localObjectPath);
        Utils::writeContents(remoteObjectPath, content);
    }
}

void SomeObj::Impl::copyCommitAndBlobs(const std::string& commitHash, const std::string& remoteObjectsDir) const {
    if (commitHash.empty() || commitHash == "0") {
        return;
    }
    
    // 如果已经存在，直接返回
    std::string remoteCommitPath = remoteObjectsDir + "/" + commitHash;
    if (Utils::exists(remoteCommitPath)) {
        return;
    }
    
    // 复制提交对象
    std::string localCommitPath = objectsDir + "/" + commitHash;
    if (!Utils::exists(localCommitPath)) {
        return;
    }
    
    std::string commitContent = Utils::readContentsAsString(localCommitPath);
    Utils::writeContents(remoteCommitPath, commitContent);
    
    // 解析提交内容，获取父提交和blobs
    std::stringstream ss(commitContent);
    std::string line;
    
    std::getline(ss, line); // 消息
    std::getline(ss, line); // 第一个父提交
    
    // 复制第一个父提交
    if (!line.empty() && line != "0") {
        copyCommitAndBlobs(line, remoteObjectsDir);
    }
    
    // 检查是否有第二个父提交
    std::getline(ss, line);
    if (line.find(":") == std::string::npos) {
        // 这是第二个父提交
        if (!line.empty() && line != "0") {
            copyCommitAndBlobs(line, remoteObjectsDir);
        }
        std::getline(ss, line); // 时间戳
    }
    
    // 复制blobs
    int blobCount;
    ss >> blobCount;
    
    for (int i = 0; i < blobCount; ++i) {
        std::string blobHash, filename;
        ss >> blobHash >> filename;
        copyObjectIfNotExists(blobHash, remoteObjectsDir);
    }
}

bool SomeObj::Impl::isAncestor(const std::string& ancestor, const std::string& descendant) const {
    if (ancestor == descendant) {
        return true;
    }
    
    std::queue<std::string> queue;
    std::set<std::string> visited;
    
    queue.push(descendant);
    visited.insert(descendant);
    
    while (!queue.empty()) {
        std::string current = queue.front();
        queue.pop();
        
        if (current == ancestor) {
            return true;
        }
        
        if (current.empty() || current == "0") {
            continue;
        }
        
        std::string commitPath = objectsDir + "/" + current;
        if (!Utils::exists(commitPath)) {
            continue;
        }
        
        std::string content = Utils::readContentsAsString(commitPath);
        std::stringstream ss(content);
        
        std::string line;
        std::getline(ss, line); // 消息
        std::getline(ss, line); // 第一个父提交
        
        if (!line.empty() && line != "0" && visited.find(line) == visited.end()) {
            queue.push(line);
            visited.insert(line);
        }
        
        // 检查是否有第二个父提交
        std::getline(ss, line);
        if (line.find(":") == std::string::npos) {
            if (!line.empty() && line != "0" && visited.find(line) == visited.end()) {
                queue.push(line);
                visited.insert(line);
            }
            std::getline(ss, line); // 时间戳
        }
    }
    
    return false;
}

// ==================== 远程相关方法 ====================

void SomeObj::Impl::addRemote(const std::string& remoteName, const std::string& directory) {
    // 检查远程是否已存在
    if (remotes.find(remoteName) != remotes.end()) {
        Utils::exitWithMessage("A remote with that name already exists.");
    }
    
    // 转换路径分隔符
    std::string remotePath = directory;
    #ifdef _WIN32
        std::replace(remotePath.begin(), remotePath.end(), '/', '\\');
    #else
        std::replace(remotePath.begin(), remotePath.end(), '\\', '/');
    #endif
    
    // 移除末尾的/.gitlite（如果存在）
    if (remotePath.length() >= 9 && remotePath.substr(remotePath.length() - 9) == "/.gitlite") {
        remotePath = remotePath.substr(0, remotePath.length() - 9);
    }
    
    // 检查远程目录是否存在
    if (!Utils::exists(remotePath + "/.gitlite")) {
        // 不检查合法性，只保存路径
    }
    
    remotes[remoteName] = remotePath;
    saveRemotes();
}

void SomeObj::Impl::rmRemote(const std::string& remoteName) {
    // 检查远程是否存在
    if (remotes.find(remoteName) == remotes.end()) {
        Utils::exitWithMessage("A remote with that name does not exist.");
    }
    
    remotes.erase(remoteName);
    saveRemotes();
}

void SomeObj::Impl::push(const std::string& remoteName, const std::string& branchName) {
    // 检查远程是否存在
    auto it = remotes.find(remoteName);
    if (it == remotes.end()) {
        Utils::exitWithMessage("Remote directory not found.");
    }
    
    std::string remotePath = it->second;
    std::string remoteGitlitePath = remotePath + "/.gitlite";
    
    // 检查远程目录是否存在
    if (!Utils::exists(remoteGitlitePath)) {
        Utils::exitWithMessage("Remote directory not found.");
    }
    
    // 获取本地分支的HEAD
    std::string localHead = getHeadCommitHash();
    if (localHead.empty()) {
        Utils::exitWithMessage("No commits in current branch.");
    }
    
    // 获取远程分支的HEAD
    std::string remoteBranchPath = remoteGitlitePath + "/refs/heads/" + branchName;
    std::string remoteHead = "";
    
    if (Utils::exists(remoteBranchPath)) {
        remoteHead = Utils::readContentsAsString(remoteBranchPath);
        if (!remoteHead.empty() && remoteHead.back() == '\n') {
            remoteHead.pop_back();
        }
    }
    
    // 检查远程分支的HEAD是否在本地分支的历史中
    if (!remoteHead.empty() && !isAncestor(remoteHead, localHead)) {
        Utils::exitWithMessage("Please pull down remote changes before pushing.");
    }
    
    // 复制所有必要的对象到远程仓库
    std::string remoteObjectsDir = remoteGitlitePath + "/objects";
    copyCommitAndBlobs(localHead, remoteObjectsDir);
    
    // 更新远程分支引用
    Utils::writeContents(remoteBranchPath, localHead + "\n");
}

void SomeObj::Impl::fetch(const std::string& remoteName, const std::string& branchName) {
    // 检查远程是否存在
    auto it = remotes.find(remoteName);
    if (it == remotes.end()) {
        Utils::exitWithMessage("Remote directory not found.");
    }
    
    std::string remotePath = it->second;
    std::string remoteGitlitePath = remotePath + "/.gitlite";
    
    // 检查远程目录是否存在
    if (!Utils::exists(remoteGitlitePath)) {
        Utils::exitWithMessage("Remote directory not found.");
    }
    
    // 检查远程分支是否存在
    std::string remoteBranchPath = remoteGitlitePath + "/refs/heads/" + branchName;
    if (!Utils::exists(remoteBranchPath)) {
        Utils::exitWithMessage("That remote does not have that branch.");
    }
    
    // 获取远程分支的HEAD
    std::string remoteHead = Utils::readContentsAsString(remoteBranchPath);
    if (!remoteHead.empty() && remoteHead.back() == '\n') {
        remoteHead.pop_back();
    }
    
    // 从远程复制所有必要的对象到本地
    std::string remoteObjectsDir = remoteGitlitePath + "/objects";
    
    // 复制提交及其所有祖先和blobs
    std::queue<std::string> commitsToCopy;
    std::set<std::string> copiedCommits;
    
    commitsToCopy.push(remoteHead);
    copiedCommits.insert(remoteHead);
    
    while (!commitsToCopy.empty()) {
        std::string commitHash = commitsToCopy.front();
        commitsToCopy.pop();
        
        // 复制提交对象
        std::string remoteCommitPath = remoteObjectsDir + "/" + commitHash;
        std::string localCommitPath = objectsDir + "/" + commitHash;
        
        if (Utils::exists(remoteCommitPath) && !Utils::exists(localCommitPath)) {
            std::string commitContent = Utils::readContentsAsString(remoteCommitPath);
            Utils::writeContents(localCommitPath, commitContent);
            
            // 解析提交内容，获取父提交
            std::stringstream ss(commitContent);
            std::string line;
            
            std::getline(ss, line); // 消息
            std::getline(ss, line); // 第一个父提交
            
            if (!line.empty() && line != "0" && copiedCommits.find(line) == copiedCommits.end()) {
                commitsToCopy.push(line);
                copiedCommits.insert(line);
            }
            
            // 检查是否有第二个父提交
            std::getline(ss, line);
            if (line.find(":") == std::string::npos) {
                if (!line.empty() && line != "0" && copiedCommits.find(line) == copiedCommits.end()) {
                    commitsToCopy.push(line);
                    copiedCommits.insert(line);
                }
                std::getline(ss, line); // 时间戳
            }
            
            // 复制blobs
            int blobCount;
            ss >> blobCount;
            
            for (int i = 0; i < blobCount; ++i) {
                std::string blobHash, filename;
                ss >> blobHash >> filename;
                
                std::string remoteBlobPath = remoteObjectsDir + "/" + blobHash;
                std::string localBlobPath = objectsDir + "/" + blobHash;
                
                if (Utils::exists(remoteBlobPath) && !Utils::exists(localBlobPath)) {
                    std::string blobContent = Utils::readContentsAsString(remoteBlobPath);
                    Utils::writeContents(localBlobPath, blobContent);
                }
            }
        }
    }
    
    // 在本地创建远程分支引用
    std::string localRemoteBranchName = remoteName + "/" + branchName;
    std::string localRemoteBranchPath = gitliteDir + "/refs/heads/" + localRemoteBranchName;
    Utils::writeContents(localRemoteBranchPath, remoteHead + "\n");
}

void SomeObj::Impl::pull(const std::string& remoteName, const std::string& branchName) {
    // 先fetch
    fetch(remoteName, branchName);
    
    // 然后merge
    std::string remoteBranchName = remoteName + "/" + branchName;
    merge(remoteBranchName);
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

// 远程方法实现
void SomeObj::addRemote(const std::string& remoteName, const std::string& directory) { 
    pImpl->addRemote(remoteName, directory); 
}
void SomeObj::rmRemote(const std::string& remoteName) { 
    pImpl->rmRemote(remoteName); 
}
void SomeObj::push(const std::string& remoteName, const std::string& branchName) { 
    pImpl->push(remoteName, branchName); 
}
void SomeObj::fetch(const std::string& remoteName, const std::string& branchName) { 
    pImpl->fetch(remoteName, branchName); 
}
void SomeObj::pull(const std::string& remoteName, const std::string& branchName) { 
    pImpl->pull(remoteName, branchName); 
}