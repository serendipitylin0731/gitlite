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
    
    //std::cout << "Initialized gitlite directory." << std::endl;
}

void Repository::createDirectories() {
    // 创建.gitlite目录结构
    Utils::createDirectories(gitliteDir);
    Utils::createDirectories(Utils::join(gitliteDir, "objects"));
    Utils::createDirectories(Utils::join(gitliteDir, "refs"));
    Utils::createDirectories(Utils::join(gitliteDir, "refs", "heads"));
    Utils::createDirectories(Utils::join(gitliteDir, "refs", "remotes"));
}

void Repository::createInitialCommit() {
    // 创建初始提交
    std::string commitContent = "initial commit\n0\n";
    std::string commitHash = Utils::sha1(commitContent);
    
    // 保存提交
    std::string commitPath = Utils::join(gitliteDir, "objects", commitHash);
    Utils::writeContents(commitPath, commitContent);
    
    // 保存提交引用
    std::string headPath = Utils::join(gitliteDir, "HEAD");
    Utils::writeContents(headPath, "ref: refs/heads/master\n");
}

void Repository::createInitialBranch() {
    // 创建master分支
    std::string commitContent = "initial commit\n0\n";
    std::string commitHash = Utils::sha1(commitContent);
    
    // 修复join调用 - 使用链式方式
    std::string refsDir = Utils::join(gitliteDir, "refs");
    std::string headsDir = Utils::join(refsDir, "heads");
    std::string masterPath = Utils::join(headsDir, "master");
    
    Utils::writeContents(masterPath, commitHash + "\n");
    
    // 同时需要确保目录存在
    Utils::createDirectories(Utils::join(refsDir, "heads"));
}