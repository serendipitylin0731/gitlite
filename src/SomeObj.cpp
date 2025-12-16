#include "../include/SomeObj.h"
#include "../include/Utils.h"
#include "../include/Repository.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <set>
#include <map>
#include <filesystem>

namespace fs = std::filesystem;

class SomeObj::Impl {
private:
    std::string gitliteDir = ".gitlite";
    std::string headPath;
    std::string objectsDir;
    std::string stagingPath;
    
    // 当前分支信息
    std::string currentBranch = "master";
    
    // 暂存区：改为保存文件名->哈希的映射
    std::map<std::string, std::string> stagedFiles;  // filename -> blobHash
    
    // 标记为删除的文件
    std::set<std::string> removedFiles;
    
    // 辅助方法
    std::string getHeadCommitHash() const;
    void saveHead();
    void loadHead();
    void saveStaging();
    void loadStaging();
    std::string createCommit(const std::string& message, const std::string& parentHash);
    std::string getFileHash(const std::string& filename) const;
    bool fileInCommit(const std::string& filename, const std::string& commitHash) const;
    std::string getCommitHashForFile(const std::string& filename, const std::string& commitHash) const;
    void updateBranch(const std::string& branchName, const std::string& commitHash);
    
public:
    Impl();
    
    void init();
    void add(const std::string& filename);
    void commit(const std::string& message);
    void rm(const std::string& filename);
    void status();
    void log();

    // 其他命令暂不实现
    void globalLog() { /* 待实现 */ }
    void find(const std::string& commitMessage) { /* 待实现 */ }
    void checkoutBranch(const std::string& branchName) { /* 待实现 */ }
    void checkoutFile(const std::string& filename) { /* 待实现 */ }
    void checkoutFileInCommit(const std::string& commitId, const std::string& filename) { /* 待实现 */ }
    void branch(const std::string& branchName) { /* 待实现 */ }
    void rmBranch(const std::string& branchName) { /* 待实现 */ }
    void reset(const std::string& commitId) { /* 待实现 */ }
    void merge(const std::string& branchName) { /* 待实现 */ }
    void addRemote(const std::string& remoteName, const std::string& directory) { /* 待实现 */ }
    void rmRemote(const std::string& remoteName) { /* 待实现 */ }
    void push(const std::string& remoteName, const std::string& branchName) { /* 待实现 */ }
    void fetch(const std::string& remoteName, const std::string& branchName) { /* 待实现 */ }
    void pull(const std::string& remoteName, const std::string& branchName) { /* 待实现 */ }
};

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
        if (!content.empty() && content.back() == '\n') {
            content.pop_back();
        }
        return content;
    }
    return "";
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
    
    // 保存已暂存的文件（包含哈希）
    ss << stagedFiles.size() << "\n";
    for (const auto& [filename, hash] : stagedFiles) {
        ss << filename << "\n" << hash << "\n";
    }
    
    // 保存标记为删除的文件
    ss << removedFiles.size() << "\n";
    for (const auto& file : removedFiles) {
        ss << file << "\n";
    }
    
    Utils::writeContents(stagingPath, ss.str());
}

void SomeObj::Impl::loadStaging() {
    if (!Utils::exists(stagingPath)) {
        return;
    }
    
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

// 未实现方法的空实现
std::string SomeObj::Impl::createCommit(const std::string& message, const std::string& parentHash) {
    // 空实现
    return "";
}

std::string SomeObj::Impl::getFileHash(const std::string& filename) const {
    // 空实现
    return "";
}

bool SomeObj::Impl::fileInCommit(const std::string& filename, const std::string& commitHash) const {
    // 空实现
    return false;
}

std::string SomeObj::Impl::getCommitHashForFile(const std::string& filename, const std::string& commitHash) const {
    // 空实现
    return "";
}

void SomeObj::Impl::updateBranch(const std::string& branchName, const std::string& commitHash) {
    // 空实现
}

// 核心方法实现
void SomeObj::Impl::init() {
    if (Utils::exists(gitliteDir)) {
        Utils::exitWithMessage("A Gitlite version-control system already exists in the current directory.");
    }
    
    // 使用Repository类进行初始化
    Repository repo;
    repo.init();
    
    // 设置当前分支和HEAD
    currentBranch = "master";
    saveHead();
    
    // 初始化空暂存区
    stagedFiles.clear();
    removedFiles.clear();
    saveStaging();
}

void SomeObj::Impl::add(const std::string& filename) {
    // 检查文件是否存在
    if (!Utils::exists(filename)) {
        Utils::exitWithMessage("File does not exist.");
    }
    
    // 读取当前文件内容并计算哈希
    std::string currentContent = Utils::readContentsAsString(filename);
    std::string currentHash = Utils::sha1(currentContent);
    
    // 1. 检查文件的当前版本是否与当前提交中的版本相同
    std::string commitHash = getHeadCommitHash();
    bool sameAsCommit = false;
    
    if (!commitHash.empty()) {
        std::string commitPath = objectsDir + "/" + commitHash;
        if (Utils::exists(commitPath)) {
            std::string commitContent = Utils::readContentsAsString(commitPath);
            std::stringstream ss(commitContent);
            
            // 跳过前3行
            std::string message, parent, timestamp;
            std::getline(ss, message);
            std::getline(ss, parent);
            std::getline(ss, timestamp);
            
            int blobCount;
            ss >> blobCount;
            
            // 检查文件是否在提交中且哈希相同
            for (int i = 0; i < blobCount; ++i) {
                std::string blobHash, blobFile;
                ss >> blobHash >> blobFile;
                if (blobFile == filename && blobHash == currentHash) {
                    sameAsCommit = true;
                    break;
                }
            }
        }
    }
    
    // 如果文件与当前提交中的版本相同
    if (sameAsCommit) {
        // 如果文件已在暂存区，则将其从暂存区中移除
        stagedFiles.erase(filename);
        // 如果文件处于暂存状态且标记为待删除，则将其待删除状态移除
        removedFiles.erase(filename);
        saveStaging();
        return;  // 不添加到暂存区
    }
    
    // 2. 保存blob对象
    std::string blobPath = objectsDir + "/" + currentHash;
    if (!Utils::exists(blobPath)) {
        Utils::writeContents(blobPath, currentContent);
    }
    
    // 3. 添加到暂存区（如果已存在会被覆盖）
    stagedFiles[filename] = currentHash;
    
    // 4. 如果文件处于暂存状态且标记为待删除，则将其待删除状态移除
    removedFiles.erase(filename);
    
    saveStaging();
}

void SomeObj::Impl::commit(const std::string& message) {
    if (message.empty()) {
        Utils::exitWithMessage("Please enter a commit message.");
    }
    
    // 检查暂存区是否为空
    if (stagedFiles.empty() && removedFiles.empty()) {
        Utils::exitWithMessage("No changes added to the commit.");
    }
    
    // 获取父提交
    std::string parentHash = getHeadCommitHash();
    
    // 构建新提交的数据
    std::stringstream commitData;
    commitData << message << "\n";
    commitData << parentHash << "\n";
    
    // 获取当前时间
    std::time_t now = std::time(nullptr);
    std::tm* gmt = std::gmtime(&now);
    char timeBuffer[100];
    std::strftime(timeBuffer, sizeof(timeBuffer), "%a %b %d %H:%M:%S %Y +0000", gmt);
    commitData << timeBuffer << "\n";
    
    // 收集blob信息
    std::map<std::string, std::string> blobs;
    
    // 从父提交继承blob
    if (!parentHash.empty()) {
        std::string commitPath = objectsDir + "/" + parentHash;
        if (Utils::exists(commitPath)) {
            std::string commitContent = Utils::readContentsAsString(commitPath);
            std::stringstream ss(commitContent);
            std::string line;
            
            // 跳过前3行（消息、父提交、时间戳）
            for (int i = 0; i < 3; ++i) {
                std::getline(ss, line);
            }
            
            // 读取blob数量
            int blobCount;
            ss >> blobCount;
            
            // 读取所有blob
            for (int i = 0; i < blobCount; ++i) {
                std::string blobHash, blobFile;
                ss >> blobHash >> blobFile;
                blobs[blobFile] = blobHash;
            }
        }
    }
    
    // 更新暂存的文件
    for (const auto& filename : stagedFiles) {
        // 注意：这里需要重新计算文件哈希
        std::string fileContent = Utils::readContentsAsString(filename.first);
        std::string fileHash = Utils::sha1(fileContent);
        
        // 保存blob（如果尚未保存）
        std::string blobPath = objectsDir + "/" + fileHash;
        if (!Utils::exists(blobPath)) {
            Utils::writeContents(blobPath, fileContent);
        }
        
        blobs[filename.first] = fileHash;
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
    
    // 创建提交
    std::string commitContent = commitData.str();
    std::string commitHash = Utils::sha1(commitContent);
    std::string commitPath = objectsDir + "/" + commitHash;
    Utils::writeContents(commitPath, commitContent);
    
    // 更新当前分支
    std::string branchPath = gitliteDir + "/refs/heads/" + currentBranch;
    Utils::writeContents(branchPath, commitHash + "\n");
    
    // 清空暂存区
    stagedFiles.clear();
    removedFiles.clear();
    saveStaging();
}

void SomeObj::Impl::rm(const std::string& filename) {
    // 检查文件是否在暂存区
    bool isStaged = (stagedFiles.find(filename) != stagedFiles.end());
    
    // 检查文件是否在当前提交中被跟踪
    bool isTracked = false;
    std::string currentCommitHash = getHeadCommitHash();
    if (!currentCommitHash.empty()) {
        std::string commitPath = objectsDir + "/" + currentCommitHash;
        if (Utils::exists(commitPath)) {
            std::string commitContent = Utils::readContentsAsString(commitPath);
            std::stringstream ss(commitContent);
            std::string line;
            
            // 跳过前3行
            for (int i = 0; i < 3; ++i) {
                std::getline(ss, line);
            }
            
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
        // 从暂存区移除
        stagedFiles.erase(filename);
    }
    
    if (isTracked) {
        // 标记为待删除
        removedFiles.insert(filename);
        
        // 如果文件存在于工作目录，删除它
        if (Utils::exists(filename)) {
            if (!Utils::restrictedDelete(filename)) {
                // 如果无法删除，至少从暂存区移除
                std::cerr << "Warning: Could not delete file from working directory." << std::endl;
            }
        }
    }
    
    saveStaging();
}

void SomeObj::Impl::status() {
    std::cout << "=== Branches ===" << std::endl;
    std::cout << "*" << currentBranch << std::endl;
    
    // 获取所有分支
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
    // 这里可以添加检测未暂存的修改的逻辑
    
    std::cout << std::endl << "=== Untracked Files ===" << std::endl;
    // 这里可以添加检测未跟踪文件的逻辑
}

void SomeObj::Impl::log() {
    std::string commitHash = getHeadCommitHash();
    
    while (!commitHash.empty()) {
        std::string commitPath = objectsDir + "/" + commitHash;
        if (!Utils::exists(commitPath)) {
            break;
        }
        
        std::string content = Utils::readContentsAsString(commitPath);
        std::stringstream ss(content);
        
        std::string message, parentHash, timestamp;
        std::getline(ss, message);    // 提交消息
        std::getline(ss, parentHash); // 父提交哈希
        std::getline(ss, timestamp);  // 时间戳
        
        // 跳过blob信息
        int blobCount;
        ss >> blobCount;
        
        std::cout << "===" << std::endl;
        std::cout << "commit " << commitHash << std::endl;
        std::cout << "Date: " << timestamp << std::endl;
        std::cout << message << std::endl;
        std::cout << std::endl;
        
        // 移动到父提交
        commitHash = parentHash;
        if (commitHash == "0") {
            commitHash = "";  // 初始提交
        }
    }
}

// ================================
// SomeObj公共接口实现
// ================================

SomeObj::SomeObj() : pImpl(std::make_unique<Impl>()) {}
SomeObj::~SomeObj() = default;

void SomeObj::init() { pImpl->init(); }
void SomeObj::add(const std::string& filename) { pImpl->add(filename); }
void SomeObj::commit(const std::string& message) { pImpl->commit(message); }
void SomeObj::rm(const std::string& filename) { pImpl->rm(filename); }
void SomeObj::log() { pImpl->log(); }
void SomeObj::globalLog() { pImpl->globalLog(); }
void SomeObj::find(const std::string& commitMessage) { pImpl->find(commitMessage); }
void SomeObj::status() { pImpl->status(); }
void SomeObj::checkoutBranch(const std::string& branchName) { pImpl->checkoutBranch(branchName); }
void SomeObj::checkoutFile(const std::string& filename) { pImpl->checkoutFile(filename); }
void SomeObj::checkoutFileInCommit(const std::string& commitId, const std::string& filename) { pImpl->checkoutFileInCommit(commitId, filename); }
void SomeObj::branch(const std::string& branchName) { pImpl->branch(branchName); }
void SomeObj::rmBranch(const std::string& branchName) { pImpl->rmBranch(branchName); }
void SomeObj::reset(const std::string& commitId) { pImpl->reset(commitId); }
void SomeObj::merge(const std::string& branchName) { pImpl->merge(branchName); }
void SomeObj::addRemote(const std::string& remoteName, const std::string& directory) { pImpl->addRemote(remoteName, directory); }
void SomeObj::rmRemote(const std::string& remoteName) { pImpl->rmRemote(remoteName); }
void SomeObj::push(const std::string& remoteName, const std::string& branchName) { pImpl->push(remoteName, branchName); }
void SomeObj::fetch(const std::string& remoteName, const std::string& branchName) { pImpl->fetch(remoteName, branchName); }
void SomeObj::pull(const std::string& remoteName, const std::string& branchName) { pImpl->pull(remoteName, branchName); }