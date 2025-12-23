#ifndef SOMEOBJ_H
#define SOMEOBJ_H

#include <string>
#include <vector>
#include <memory>

class SomeObj {
public:
    SomeObj();
    ~SomeObj();

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
    void checkoutBranch(const std::string& branchName);
    void branch(const std::string& branchName);
    void rmBranch(const std::string& branchName);
    void reset(const std::string& commitId);
    void merge(const std::string& branchName);
    void addRemote(const std::string& remoteName, const std::string& directory);
    void rmRemote(const std::string& remoteName);
    void push(const std::string& remoteName, const std::string& branchName);
    void fetch(const std::string& remoteName, const std::string& branchName);
    void pull(const std::string& remoteName, const std::string& branchName);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

#endif