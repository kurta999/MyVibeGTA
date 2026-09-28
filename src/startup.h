#pragma once
#include <memory>

namespace startup {
// The loading window owns a separate message loop, so expensive shader and
// asset work cannot freeze its paint, move, or close handling.
class Session {
public:
    explicit Session(bool visible);
    ~Session();
    Session(const Session&)=delete;
    Session& operator=(const Session&)=delete;
    void finish();
    bool cancelled() const;
private:
    struct State;
    std::unique_ptr<State> state;
    friend bool report(int percent,const char* stage);
};

// Percentages are completed startup checkpoints, not a time estimate.
// Returns false when the user closed the loading window.
bool report(int percent,const char* stage);
}
