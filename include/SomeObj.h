#ifndef SOMEOBJ_H
#define SOMEOBJ_H

#include <string>
#include <vector>
#include <memory>

// SomeObj类，对应main.cpp中的bloop对象
class SomeObj {
public:
    SomeObj();
    ~SomeObj();
    
    // 基本命令
    void init();
    void add(const std::string& filename);
    void commit(const std::string& message);
    void rm(const std::string& filename);
    void log();
    void globalLog();
    void find(const std::string& commitMessage);
    void status();
    
    // checkout相关
    void checkoutBranch(const std::string& branchName);
    void checkoutFile(const std::string& filename);
    void checkoutFileInCommit(const std::string& commitId, const std::string& filename);
    
    // 分支相关
    void branch(const std::string& branchName);
    void rmBranch(const std::string& branchName);
    
    // 重置和合并
    void reset(const std::string& commitId);
    void merge(const std::string& branchName);
    
    // 远程操作（稍后实现）
    void addRemote(const std::string& remoteName, const std::string& directory);
    void rmRemote(const std::string& remoteName);
    void push(const std::string& remoteName, const std::string& branchName);
    void fetch(const std::string& remoteName, const std::string& branchName);
    void pull(const std::string& remoteName, const std::string& branchName);
    
private:
    // 内部实现细节
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

#endif