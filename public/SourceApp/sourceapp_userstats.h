#ifndef SOURCEAPP_USERSTATS_H
#define SOURCEAPP_USERSTATS_H

class ISourceAppUserStats {
public:
    virtual void SetAchievement(const char* name) = 0;
};

class CSourceAppApicontext {
public:
    virtual ISourceAppUserStats* SourceAppUserStats() = 0;
};

extern CSourceAppApicontext* SourceAppApicontext;


#endif // SOURCEAPP_USERSTATS_H
