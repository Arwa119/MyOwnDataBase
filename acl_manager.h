#ifndef ACL_MANAGER_H
#define ACL_MANAGER_H

#include <string>
#include <unordered_map>

enum Role { ADMIN, DEVELOPER, USER };

struct ACL {
    Role role;
    bool canCreate;
    bool canUpdate;
    bool canDrop;
    bool canRead;
};

ACL getPermissions(Role role);
bool hasAccess(Role role, std::string action);

#endif
