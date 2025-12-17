#include "../include/Repository.h"
#include <iostream>
#include <sstream>
#include <algorithm>

// 静态成员变量定义
std::string Repository::gitliteDir = ".gitlite";

Repository::Repository() {
    // 构造函数实现
}

Repository::~Repository() {
    // 析构函数实现
}

std::string Repository::getGitliteDir() {
    return gitliteDir;
}

bool Repository::exists() {
    return Utils::isDirectory(gitliteDir);
}

void Repository::init() {
    if (exists()) {
        Utils::exitWithMessage("A Gitlite version-control system already exists in the current directory.");
    }
    
    // 创建目录结构
    createDirectories();
    
    // 创建初始提交
    createInitialCommit();
    
    // 设置初始分支
    createInitialBranch();
}

void Repository::createDirectories() {
    // 创建.gitlite目录结构
    Utils::createDirectories(gitliteDir);
    Utils::createDirectories(Utils::join(gitliteDir, "objects"));
    Utils::createDirectories(Utils::join(gitliteDir, "refs"));
    
    // 使用多次join创建嵌套目录
    std::string refsHeads = Utils::join(Utils::join(gitliteDir, "refs"), "heads");
    Utils::createDirectories(refsHeads);
    
    std::string refsRemotes = Utils::join(Utils::join(gitliteDir, "refs"), "remotes");
    Utils::createDirectories(refsRemotes);
}

void Repository::createInitialCommit() {
    // 创建正确格式的初始提交（四行格式）
    std::stringstream commitData;
    commitData << "initial commit\n";          // 第1行：消息
    commitData << "0\n";                       // 第2行：父提交（0表示没有）
    commitData << "Thu Jan 01 00:00:00 1970 +0000\n";  // 第3行：时间戳
    commitData << "0\n";                       // 第4行：blob数量（0个文件）
    
    std::string commitContent = commitData.str();
    std::string commitHash = Utils::sha1(commitContent);
    
    // 保存提交 - 使用两次join
    std::string objectsDir = Utils::join(gitliteDir, "objects");
    std::string commitPath = Utils::join(objectsDir, commitHash);
    Utils::writeContents(commitPath, commitContent);
    
    // 保存HEAD文件
    std::string headPath = Utils::join(gitliteDir, "HEAD");
    Utils::writeContents(headPath, "ref: refs/heads/master\n");
}

void Repository::createInitialBranch() {
    // 创建正确格式的初始提交（四行格式）
    std::stringstream commitData;
    commitData << "initial commit\n";          // 第1行：消息
    commitData << "0\n";                       // 第2行：父提交（0表示没有）
    commitData << "Thu Jan 01 00:00:00 1970 +0000\n";  // 第3行：时间戳
    commitData << "0\n";                       // 第4行：blob数量（0个文件）
    
    std::string commitContent = commitData.str();
    std::string commitHash = Utils::sha1(commitContent);
    
    // 创建master分支引用 - 使用多次join
    std::string refsDir = Utils::join(gitliteDir, "refs");
    std::string headsDir = Utils::join(refsDir, "heads");
    std::string masterPath = Utils::join(headsDir, "master");
    
    Utils::writeContents(masterPath, commitHash + "\n");
}