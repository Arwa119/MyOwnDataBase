#include "lock_manager.h"
#include <algorithm>
#include <cstdint>
#include <queue>

bool LockManager::acquireLock(uint32_t transaction_id, const std::string& table_name, LockType lock_type) {
    if (transaction_id == 0) {
        return true; // Non-transactional operations bypass locking
    }

    LockInfo& lock_info = table_locks[table_name];

    // Check if transaction already holds a compatible lock
    if (lock_info.holding_transactions.count(transaction_id) > 0) {
        if (lock_type == LockType::SHARED || 
            (lock_type == LockType::EXCLUSIVE && lock_info.current_lock_type == LockType::EXCLUSIVE)) {
            return true; // Already have compatible lock
        }
        // Lock upgrade needed
        if (lock_info.holding_transactions.size() == 1) {
            lock_info.current_lock_type = LockType::EXCLUSIVE;
            return true;
        }
    }

    // Check if lock can be granted immediately
    if (lock_info.holding_transactions.empty()) {
        lock_info.current_lock_type = lock_type;
        lock_info.holding_transactions.insert(transaction_id);
        transaction_locks[transaction_id].insert(table_name);
        return true;
    }

    // Check compatibility
    if (lock_type == LockType::SHARED && lock_info.current_lock_type == LockType::SHARED) {
        lock_info.holding_transactions.insert(transaction_id);
        transaction_locks[transaction_id].insert(table_name);
        return true;
    }

    // Cannot grant lock - check for deadlock before waiting
    if (detectDeadlock(transaction_id, table_name, lock_type)) {
        return false; // Deadlock detected
    }

    lock_info.waiting_requests.emplace(transaction_id, lock_type);
    return false;
}

void LockManager::releaseLock(uint32_t transaction_id, const std::string& table_name) {
    if (transaction_id == 0) {
        return;
    }

    auto it = table_locks.find(table_name);
    if (it == table_locks.end()) return;

    LockInfo& lock_info = it->second;
    lock_info.holding_transactions.erase(transaction_id);
    transaction_locks[transaction_id].erase(table_name);

    // If no one is holding the lock, try to grant waiting requests
    if (lock_info.holding_transactions.empty()) {
        // Try to grant locks to waiting transactions
        std::queue<LockRequest> temp_waiting_requests;
        while (!lock_info.waiting_requests.empty()) {
            LockRequest request = lock_info.waiting_requests.front();
            lock_info.waiting_requests.pop();

            if (request.transaction_id == 0) { // Skip non-transactional requests
                continue;
            }

            if (lock_info.holding_transactions.empty()) { // No one holding, grant the first request
                lock_info.current_lock_type = request.lock_type;
                lock_info.holding_transactions.insert(request.transaction_id);
                transaction_locks[request.transaction_id].insert(table_name);
            } else if (request.lock_type == LockType::SHARED && lock_info.current_lock_type == LockType::SHARED) {
                // If current is SHARED and request is SHARED, grant
                lock_info.holding_transactions.insert(request.transaction_id);
                transaction_locks[request.transaction_id].insert(table_name);
            } else {
                // Cannot grant this request now, re-queue
                temp_waiting_requests.push(request);
            }
        }
        lock_info.waiting_requests = temp_waiting_requests; // Restore remaining waiting requests
    }

    if (lock_info.holding_transactions.empty() && lock_info.waiting_requests.empty()) {
        table_locks.erase(table_name);
    }
}

void LockManager::releaseAllLocks(uint32_t transaction_id) {
    if (transaction_id == 0) {
        return; // Non-transactional operations have no locks
    }

    auto locked_tables = transaction_locks[transaction_id];
    for (const auto& table_name : locked_tables) {
        releaseLock(transaction_id, table_name);
    }
    transaction_locks.erase(transaction_id);
}

bool LockManager::detectDeadlock(uint32_t transaction_id, const std::string& table_name, LockType lock_type) {
    if (transaction_id == 0) {
        return false; // Non-transactional operations cannot cause deadlocks
    }

    std::unordered_map<uint32_t, std::unordered_set<uint32_t>> wait_graph;
    buildWaitGraph(wait_graph);

    std::unordered_set<uint32_t> visited;
    std::unordered_set<uint32_t> rec_stack;
    return hasCycle(transaction_id, visited, rec_stack, wait_graph);
}

void LockManager::buildWaitGraph(std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& graph) {
    graph.clear(); // Clear previous graph before building

    for (const auto& pair : table_locks) {
        const LockInfo& lock_info = pair.second;

        // Iterate through waiting requests for this table
        std::queue<LockRequest> temp_waiting_requests = lock_info.waiting_requests; // Create a copy to iterate
        while (!temp_waiting_requests.empty()) {
            LockRequest waiting_request = temp_waiting_requests.front();
            temp_waiting_requests.pop();

            uint32_t waiting_tid = waiting_request.transaction_id;
            if (waiting_tid == 0) continue;

            // Add an edge from the waiting transaction to every transaction currently holding the lock
            for (uint32_t holding_tid : lock_info.holding_transactions) {
                if (holding_tid != 0 && holding_tid != waiting_tid) { // Ensure self-wait is not considered
                    graph[waiting_tid].insert(holding_tid);
                }
            }
        }
    }
}

bool LockManager::hasCycle(uint32_t transaction_id, std::unordered_set<uint32_t>& visited,
                           std::unordered_set<uint32_t>& rec_stack,
                           const std::unordered_map<uint32_t, std::unordered_set<uint32_t>>& graph) {
    if (transaction_id == 0) {
        return false; // Non-transactional operations cannot be part of a cycle
    }

    if (rec_stack.count(transaction_id) > 0) {
        return true;
    }

    if (visited.count(transaction_id) > 0) {
        return false;
    }

    visited.insert(transaction_id);
    rec_stack.insert(transaction_id);

    auto it = graph.find(transaction_id);
    if (it != graph.end()) {
        for (uint32_t neighbor : it->second) {
            if (hasCycle(neighbor, visited, rec_stack, graph)) {
                return true;
            }
        }
    }

    rec_stack.erase(transaction_id);
    return false;
}