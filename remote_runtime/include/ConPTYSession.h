#pragma once
#include <windows.h>
#include <string>
#include <atomic>
#include <thread>
#include <functional>
#include <cstdint>

namespace sara::remote {

// ConPTYSession — wraps a real Windows CreatePseudoConsole() PTY.
// ONE instance = ONE persistent shell session.
// Shell state, CWD, env vars, and running processes all persist.
class ConPTYSession {
public:
    using OutputCallback = std::function<void(const std::string& data)>;
    using ExitCallback   = std::function<void(int exit_code)>;

    ConPTYSession() = default;
    ~ConPTYSession();

    // Start the PTY with the given shell, initial size.
    // on_output is called on the reader thread with raw PTY bytes.
    // on_exit is called when the shell process terminates.
    bool start(const std::string& shell_cmd,
               int cols, int rows,
               bool admin_mode,
               OutputCallback on_output,
               ExitCallback   on_exit);

    // Write raw bytes to the PTY stdin (keyboard input from browser).
    void write(const std::string& data);

    // Resize the PTY window.
    void resize(int cols, int rows);

    // Gracefully stop the PTY session.
    void stop();

    bool is_alive() const;
    int  cols() const { return cols_; }
    int  rows() const { return rows_; }

private:
    void reader_loop();

    HPCON hpc_            = INVALID_HANDLE_VALUE;
    HANDLE proc_          = INVALID_HANDLE_VALUE;
    HANDLE thread_        = INVALID_HANDLE_VALUE;
    HANDLE pipe_in_write_ = INVALID_HANDLE_VALUE;  // we write here → PTY stdin
    HANDLE pipe_out_read_ = INVALID_HANDLE_VALUE;  // we read here ← PTY stdout

    std::atomic<bool> running_{false};
    std::thread reader_thread_;
    OutputCallback on_output_;
    ExitCallback   on_exit_;
    int cols_ = 220;
    int rows_ = 50;
};

} // namespace sara::remote
