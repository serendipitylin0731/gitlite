#ifndef REPOSITORY_H
#define REPOSITORY_H

#include <string>
#include "Utils.h"

class Repository {
public:
    Repository();
    ~Repository();
    
    static std::string getGitliteDir();
    static bool exists();
    
    void init();
    
private:
    static std::string gitliteDir;
    
    void createDirectories();
    void createInitialCommit();
    void createInitialBranch();
};

#endif