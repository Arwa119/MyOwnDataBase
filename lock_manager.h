#ifndef LOCK_MANAGER_H
#define LOCK_MANAGER_H

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <queue>

enum class LockType { SHARED, EXCLUSIVE };

struct LockRequest {
    uint32_t transaction_id;
    LockType lock_type;
    LockRequest(uint32_t tid, LockType type) : transaction_id(tid), lock_type(type) {}
};

class LockManager {
private:
    struct LockInfo {
        LockType current_lock_type;
        std::unordered_set<uint32_t> holding_transactions;
        std::queue<LockRequest> waiting_requests;
        LockInfo() : current_lock_type(LockType::SHARED) {}
    };

    std::unordered_map<std::string, LockInfo> table_locks;
    std::unordered_map<uint32_t, std::unordered_set<std::string>> transaction_locks;

    void buildWaitGraph(std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& graph);
    bool hasCycle(uint32_t transaction_id, std::unordered_set<uint32_t>& visited,
                  std::unordered_set<uint32_t>& rec_stack,
                  const std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& graph);

public:
    LockManager() = default;
    bool acquireLock(uint32_t transaction_id, const std::string& table_name, LockType lock_type);
    void releaseLock(uint32_t transaction_id, const std::string& table_name);
    void releaseAllLocks(uint32_t transaction_id);
    bool detectDeadlock(uint32_t transaction_id, const std::string& table_name, LockType lock_type);
};

#endif