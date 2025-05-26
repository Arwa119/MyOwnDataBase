#include "acl_manager.h"

std::unordered_map<Role, ACL> rolePermissions = {
    {ADMIN, {ADMIN, true, true, true, true}},      // Full access
    {DEVELOPER, {DEVELOPER, true, true, false, true}}, // Can't DROP tables
    {USER, {USER, false, false, false, true}}     // Only READ access
};

// Returns permissions based on role
ACL getPermissions(Role role) {
    return rolePermissions[role];
}

// Checks if user has permission for an action
bool hasAccess(Role role, std::string action) {
    ACL acl = rolePermissions[role];

    if (action == "CREATE" && acl.canCreate) return true;
    if (action == "UPDATE" && acl.canUpdate) return true;
    if (action == "DROP" && acl.canDrop) return true;
    if (action == "READ" && acl.canRead) return true;

    return false;
}
