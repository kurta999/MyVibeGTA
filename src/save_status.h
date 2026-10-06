#pragma once
#include <atomic>
#include <memory>
#include <utility>

namespace savegame {
enum class WriteStatus {Pending,Succeeded,Failed,Superseded};
// The receipt follows this exact snapshot, even if a later autosave replaces
// it in the bounded pending queue. Polling never waits on disk I/O.
class WriteReceipt {
public:
    WriteReceipt()=default;
    WriteStatus status() const {return state?state->load():WriteStatus::Superseded;}
    explicit operator bool() const {return bool(state);}
private:
    friend class Writer;
    explicit WriteReceipt(std::shared_ptr<std::atomic<WriteStatus>> state):state(std::move(state)){}
    void complete(WriteStatus result) const {state->store(result);}
    std::shared_ptr<std::atomic<WriteStatus>> state;
};
}
