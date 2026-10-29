#include "GameplayUI.hpp"
#include <mutex>

namespace GameplayUI {
    namespace {std::mutex Mutex;State Current;std::vector<Request> Requests;}
    void Publish(State Value) {std::lock_guard<std::mutex> Lock(Mutex);Current=std::move(Value);}
    State Snapshot() {std::lock_guard<std::mutex> Lock(Mutex);return Current;}
    bool Queue(Request Value) {std::lock_guard<std::mutex> Lock(Mutex);if(Requests.size()>=32)return false;Requests.push_back(Value);return true;}
    std::vector<Request> Take() {std::lock_guard<std::mutex> Lock(Mutex);std::vector<Request> Result;Result.swap(Requests);return Result;}
    void Clear() {std::lock_guard<std::mutex> Lock(Mutex);Current={};Requests.clear();}
}
