/* ********************************************************************
   * Project   : ${PROJECT_NAME}
   * Author    : ${USER}
   *********************************************************************/

/*---------------------------------------------------------------------
  -- compatibility
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes (import)
  ---------------------------------------------------------------------*/
#include "CommandLine.h"

/*---------------------------------------------------------------------
  -- project includes (export)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- C standard includes
  ---------------------------------------------------------------------*/
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#ifdef _MSC_VER
#else
#include <sys/syscall.h>
#include <unistd.h>
#endif

/*---------------------------------------------------------------------
  -- C++ standard includes
  ---------------------------------------------------------------------*/
#include <algorithm>
#include <iostream>
#include <sstream>
#include <array>
#include <mutex>
#include <cstring>
#include <vector>
#include <future>
#include <source_location>
#include <format>
#include <chrono>
#include <map>

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/
#define WRITE_LOG(format, ...) write_log(std::source_location(), false, true, format, std::make_format_args(__VA_ARGS__))
#define WRITE_LOG_NO_LF(format, ...) write_log(std::source_location(), false, false, format, std::make_format_args(__VA_ARGS__))
#define WRITE_LOG_LF_BEFORE(format, ...) write_log(std::source_location(), true, true, format, std::make_format_args(__VA_ARGS__))

#define DEFAULT_IP INADDR_ANY
#define DEFAULT_PORT 8023

/*---------------------------------------------------------------------
  -- forward declarations
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/
static void write_log(std::source_location location, bool lf_before, bool lf_after, std::string_view format, std::format_args&& args);

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- local variables
  ---------------------------------------------------------------------*/
static std::atomic<bool> running{true};

/*---------------------------------------------------------------------
  -- private functions
  ---------------------------------------------------------------------*/
static void write_log(
  std::source_location location,
  // ReSharper disable once CppDFAConstantParameter
  const bool lf_before,
  // ReSharper disable once CppDFAConstantParameter
  const bool lf_after,
  std::string_view format,
  std::format_args&& args
) {
  static std::mutex log_mutex;

  // We only want the file name, not the path
  const char *file = location.file_name();
  if (const char *p = strrchr(file, '/'))
  {
      file = p + 1;
  }
  uint32_t line = location.line();

  // get the time stamp
  std::string time_stamp;
  {
    std::chrono::time_point<std::chrono::utc_clock> epoch = std::chrono::utc_clock::now();
    time_stamp = std::format("{0:%F} {0:%T%z}", epoch);
  }

  // get process identifiers
#ifdef _MSC_VER
  long pid = 0;
  long tid = 0;
#else
  pid_t pid = getpid();
  long tid = syscall(SYS_gettid);
#endif

  // Create the function that will evaluate the header
  std::string header_str;
  if (pid == tid) {
      header_str = std::format("{} [{}@{:05}:{:05}] ", time_stamp, file, line, pid);
  }
  else {
      header_str = std::format("{} [{}@{:05}:{:05}:{:x}] ", time_stamp, file, line, pid, tid);
  }

  // This is required by C++20 to allow us to pass the format arguments to std::vformat
  std::string body_str;
  {
    const std::format_args &my_args = std::move(args);
    body_str = std::vformat(format, my_args);
  }

  // Log the line
  std::lock_guard lock(log_mutex);
  std::cout
    // ReSharper disable once CppDFAConstantConditions
    // ReSharper disable once CppDFAUnreachableCode
    << (lf_before ? "\r\n" : "") // If we want a line feed before, then we print it
    << header_str << body_str  // Print the header and body
    <<  "\x1b[K\r" // Clear to end of line, and return to the start of the line
    // ReSharper disable once CppDFAUnreachableCode
    // ReSharper disable once CppDFAConstantConditions
    << (lf_after ? "\n" : ""); // If we want a line feed after, then we print it
}

static void ex(std::ostream &os) {
  (void)os;

  running = false;
}

static void dir(std::ostream &os) {
  os << "Directory..." << std::endl;
}

static std::string &toupper(std::string &str) {
  std::ranges::transform(
    str,
    str.begin(),
    [](unsigned char chr) { return std::toupper(chr); }
  );

  return str;
}

static void connection_command(std::string &line, std::ostream &os) {
  static std::map<std::string_view, std::function<void(std::ostream &os)>> tokens {
    { "EX", ex },
    { "DIR", dir },
  };

  char *tmp_ptr{nullptr};
  const char *token = strtok_r(line.data(), " ", &tmp_ptr);
  while (token) {
    // Copy the token out to where we can process it
    std::string token_str{token};
    toupper(token_str);

    // If this token exists then call it
    if (auto this_token = tokens.find(token_str); this_token != tokens.end())
      this_token->second(os);

    // Carry on tokenising
    token = strtok_r(nullptr, " ", &tmp_ptr);
  }
}

static void connection(int s) {
  // Print prompt
  send(s, ">>", 2, 0);

  // Run until stopped
  std::string line;
  std::array<char, 256> buffer{};
  std::string_view error_message;
  while (running) {
    // Wait for data
    fd_set recv_fds;
    FD_ZERO(&recv_fds);
    FD_SET(s, &recv_fds);
    struct timeval timeout{
      .tv_sec = 0,
      .tv_usec = 100,
    };
    switch (select(s + 1, &recv_fds, nullptr, nullptr, &timeout)) {
      case -1:
        error_message = strerror(errno);
        WRITE_LOG("Failed to select on socket: {0} {1}", errno, error_message);
        running = false;
        break;
      case 0:
        // Timeout, so just wait again
        break;
      default:
        // We have data to read, so read it
        ssize_t bytes_received = recv(s, buffer.data(), buffer.size(), 0);

        // If we couldn't receive then abort
        if (bytes_received == -1) {
          error_message = strerror(errno);
          WRITE_LOG("Failed to receive data: {0} {1}", errno, error_message);
          running = false;
          break;
        }

        // If the client closed the connection then abort
        if (bytes_received == 0) {
          WRITE_LOG("Connection closed by client");
          running = false;
          break;
        }

        // Process the received data.
        for (int i = 0; i < bytes_received; i++) {
          // Get this character
          char chr = buffer[i];

          // If this is a line terminator then we have a line to process
          if (chr == '\r' || chr == '\n') {
            if (!line.empty()) {
              // Process this line
              std::stringstream os;
              connection_command(line, os);

              // Send the response to the client
              std::string response = os.str();
              if (!response.empty()) {
                send(s, "\r\n", 2, 0);
                send(s, response.c_str(), response.size(), 0);
              }

              // Line has been processed
              line.clear();
            }

            // Send pŕompt to client
            send(s, "\r\n>>", 4, 0);
            continue;
          }

          // Dump special characters
          if (!isprint(chr)) {
            continue;
          }

          // Accumulate character
          send(s, &chr, 1, 0);
          line += chr;
        }
        break;
    }
  }
}

/*---------------------------------------------------------------------
  -- public functions
  ---------------------------------------------------------------------*/

int main(int argc, char *argv[]) {
  int s{-1};
  std::vector<std::future<void>> threads;

  WRITE_LOG("Hello");

  // Simplify error handling
  do {
    // Get the parameters
    CommandLine cmd_run;
    cmd_run.AddOption("host", 'h', false, HasValue::Required, Occurs::AtMost, 1, "IP host address to bind to.");
    cmd_run.AddOption("port", 'p', false, HasValue::Required, Occurs::AtMost, 1, "TCP port to bind to.");

    if (std::stringstream error_message; !cmd_run.Parse(argc, argv, error_message)) {
      std::cerr << error_message.str() << std::endl;
      cmd_run.PrintUsage(argv);
      break;
    }

    // Resolve the host name to an IP address
    struct hostent host_buf{};
    struct hostent *host_ptr{nullptr};
    if (cmd_run.IsOptionValue("host")) {
      std::array<char, 8192> tmp_buf{};
      int gethost_errno{0};
      if (gethostbyname_r(cmd_run.GetOptionValues("host")[0].c_str(), &host_buf, tmp_buf.data(), tmp_buf.size(), &host_ptr, &gethost_errno) != 0) {
        std::string_view error_message = hstrerror(gethost_errno);
        WRITE_LOG("Failed to resolve host {0}: {1} {2}", cmd_run.GetOptionValues("host")[0], gethost_errno, error_message);
        break;
      }
    }

    // Create a socket and bind it to the specified host and port
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
      std::string_view error_message = strerror(errno);
      WRITE_LOG("Failed to create socket: {0} {1}", errno, error_message);
      break;
    }

    // Bind the socket to the specified host and port

    if (sockaddr_in addr{
          .sin_family = AF_INET,
          .sin_port = htons(static_cast<uint16_t>(cmd_run.IsOptionValue("port") ? std::stoi(cmd_run.GetOptionValues("port")[0]) : DEFAULT_PORT)),
          .sin_addr = host_ptr && host_ptr->h_addr_list[0] ? *reinterpret_cast<in_addr *>(host_ptr->h_addr_list[0]) : in_addr{.s_addr = DEFAULT_IP},
          .sin_zero{0}
        }; bind(s, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == -1) {
      std::string_view error_message = strerror(errno);
      WRITE_LOG("Failed to bind socket: {0} {1}", errno, error_message);
      break;
    }

    // Listen for incoming connections
    if (listen(s, 5) == -1) {
      std::string_view error_message = strerror(errno);
      WRITE_LOG("Failed to listen on socket: {0} {1}", errno, error_message);
      break;
    }

    // Run until stopped
    while (running) {
      // Wait for a connection
      fd_set accept_fds;
      FD_ZERO(&accept_fds);
      FD_SET(s, &accept_fds);
      struct timeval timeout{
        .tv_sec = 0,
        .tv_usec = 100,
      };
      switch (select(s + 1, &accept_fds, nullptr, nullptr, &timeout)) {
        case -1:
          {
            std::string_view error_message = strerror(errno);
            WRITE_LOG("Failed to select on socket: {0} {1}", errno, error_message);
            continue;
          }
        case 0:
          {
            // Timeout, so just clean up the threads and wait again{
            auto i = std::begin(threads);
            while (i != std::end(threads)) {
              if (i->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                i->get();
                threads.erase(i);
              }
            }
          }
          break;
        default:
          // Accept the connection
          int client_socket;
          if ((client_socket = accept(s, nullptr, nullptr)) == -1) {
            std::string_view error_message = strerror(errno);
            WRITE_LOG("Failed to accept connection: {0} {1}", errno, error_message);
            continue;
          }

          // Start a thread to handle the connection
          std::mutex cm; // Mutex to synchronise the client socket transfer to the thread
          std::unique_lock cl{cm}; // Lock the mutex before starting the thread to ensure that the thread waits until we have moved the client socket into it
          std::condition_variable cv{}; // Condition variable to signal the thread that the client socket has been moved into it
          auto future = std::async(std::launch::async, [&client_socket, &cl, &cv] {
            // We need to move the client socket into the thread, so we will set it to -1 in the main thread to indicate that it has been moved
            int my_socket = client_socket;
            cl.unlock();
            cv.notify_all();

            // Run the connection handler
            connection(my_socket);

            // Close the client socket when done
            close(my_socket);
          });
          threads.emplace_back(std::move(future));

          // Wait for the client socket to be moved into the thread before we can continue accepting connections
          while (cv.wait_for(cl, std::chrono::milliseconds(100)) != std::cv_status::no_timeout)
            ;
      }
    }
  } while (false);

  // Wait for all threads to finish
  for (auto &thread : threads) {
    thread.wait();
    thread.get();
  }

  // Close socket
  if (s != -1) {
    close(s);
  }
}
