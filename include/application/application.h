#pragma once

#include <memory>
#include <functional>

namespace engine {
namespace application {

/// Application abstraction for platform layer initialization and main loop running
class Application
{
  public:
    /// Constructor
    Application();

    /// Disable default constructors / assignment
    Application(const Application&) = delete;
    Application(Application&&);
    Application& operator = (const Application&) = delete;
    Application& operator = (Application&&) = delete;

    /// Exit code
    int get_exit_code() const;

    /// Has application exited
    bool has_exited() const;

    /// Notify application to exit
    void exit(int code = 0);

    /// Current time
    static double time();

    /// Idle function handler
    /// returns number of milliseconds to sleep
    typedef std::function<size_t ()> IdleHandler;

    /// Main loop
    void main_loop(const IdleHandler& = IdleHandler());

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

}}
