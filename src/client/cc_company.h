#ifndef CROWNLESS_COMPANY_H
#define CROWNLESS_COMPANY_H
#include <stdbool.h>
#include <stddef.h>

typedef struct CcCompanyEntry { char id[33]; char name[65]; bool self; bool online; } CcCompanyEntry;
typedef struct CcCompany {
    char id[33];
    char name[65];
    char invitation[256];
    CcCompanyEntry worlds[16];
    CcCompanyEntry crew[8];
    int world_count;
    int crew_count;
    bool owner;
    bool paused;
} CcCompany;

void CcCompanyConfigure(const char *identity_path);
bool CcCompanyList(CcCompany *company, char *error, size_t capacity);
bool CcCompanyLoad(CcCompany *company, const char *world, char *error, size_t capacity);
bool CcCompanyCreate(CcCompany *company, const char *player, const char *name, const char *pass, char *error, size_t capacity);
bool CcCompanyJoin(CcCompany *company, const char *player, const char *invitation, char *error, size_t capacity);
bool CcCompanyInvite(CcCompany *company, bool rotate, char *error, size_t capacity);
bool CcCompanyHost(CcCompany *company, const char *action, const char *member, char *error, size_t capacity);
bool CcCompanyRequest(const char *path, const char *body, char **result, char *error, size_t capacity);
bool CcCompanyRequestStatus(const char *path, const char *body, char **result, int *status, char *error, size_t capacity);
bool CcCompanyJsonText(const char *json, const char *path, char *text, size_t capacity);
long long CcCompanyJsonNumber(const char *json, const char *path);
char *CcCompanyJsonQuote(const char *text);
char *CcCompanyJsonValue(const char *json, const char *path);
#endif
