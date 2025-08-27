/*
  Copyright (c) DataStax, Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include "process.hpp"

#include <iostream>
#include <sstream>
#include <string.h>
#include <stdlib.h>

#include <uv.h>

// Platform-specific includes for environment handling
#ifdef _WIN32
#include <windows.h>
#else
extern char **environ;
#endif

// Create simple console logging functions
#define LOG_MESSAGE(message, is_output)               \
  if (is_output) {                                    \
    std::cerr << "process> " << message << std::endl; \
  }
#define LOG_ERROR(message) LOG_MESSAGE(message, true)

namespace utils {

Process::Result Process::execute(const Args& command) {
  // Call the new overload with use_java17_env = false to maintain existing behavior
  return execute(command, false);
}

Process::Result Process::execute(const Args& command, bool use_java17_env) {
  Result result;

  // Create the loop
  uv_loop_t loop;
  uv_loop_init(&loop);
  uv_process_options_t options;
  memset(&options, 0, sizeof(uv_process_options_t));

  // Create the options for reading information from the spawn pipes
  uv_pipe_t standard_output;
  uv_pipe_t standard_error;
  uv_pipe_init(&loop, &standard_output, 0);
  uv_pipe_init(&loop, &standard_error, 0);
  uv_stdio_container_t stdio[3];
  options.stdio_count = 3;
  options.stdio = stdio;
  options.stdio[0].flags = UV_IGNORE;
  options.stdio[1].flags = static_cast<uv_stdio_flags>(UV_CREATE_PIPE | UV_WRITABLE_PIPE);
  options.stdio[1].data.stream = reinterpret_cast<uv_stream_t*>(&standard_output);
  options.stdio[2].flags = static_cast<uv_stdio_flags>(UV_CREATE_PIPE | UV_WRITABLE_PIPE);
  options.stdio[2].data.stream = reinterpret_cast<uv_stream_t*>(&standard_error);

  // Create the options for the process
  std::vector<const char*> args;
  for (Args::const_iterator it = command.begin(), end = command.end(); it != end; ++it) {
    args.push_back(it->c_str());
  }
  args.push_back(NULL);
  options.args = const_cast<char**>(&args[0]);
  options.exit_cb = Process::on_exit;
  options.file = command[0].c_str();

  // Handle environment modification for Java 17 (only for subprocess, not global)
  std::vector<std::string> env_strings;
  std::vector<char*> env_array;
  
  if (use_java17_env) {
    const char* java17_home = getenv("JAVA17_HOME");
    
    if (!java17_home) {
      std::cerr << "WARNING: JAVA17_HOME not set. Cassandra 5.0+ requires Java 11 or higher." << std::endl;
      std::cerr << "         Set JAVA17_HOME environment variable before running tests." << std::endl;
      // Continue anyway - let CCM fail with its own error message
    } else {
      // Build modified environment for the subprocess only
#ifdef _WIN32
      // Windows: Get current environment and modify it
      LPCH env_block = GetEnvironmentStrings();
      if (env_block) {
        LPTSTR var = env_block;
        while (*var) {
          std::string env_var(var);
          
          // Skip existing JAVA_HOME, we'll set our own
          if (env_var.find("JAVA_HOME=") == 0) {
            var += lstrlen(var) + 1;
            continue;
          }
          
          // Modify PATH to prepend Java 17 bin directory
          if (env_var.find("PATH=") == 0 || env_var.find("Path=") == 0) {
            size_t equals_pos = env_var.find('=');
            std::string path_value = env_var.substr(equals_pos + 1);
            env_strings.push_back("PATH=" + std::string(java17_home) + "\\bin;" + path_value);
          } else {
            env_strings.push_back(env_var);
          }
          
          var += lstrlen(var) + 1;
        }
        FreeEnvironmentStrings(env_block);
        
        // Add JAVA_HOME pointing to Java 17
        env_strings.push_back(std::string("JAVA_HOME=") + java17_home);
      }
#else
      // Linux/Unix: Copy current environment and modify it
      for (char **env = environ; *env != nullptr; ++env) {
        std::string env_var(*env);
        
        // Skip existing JAVA_HOME, we'll set our own
        if (env_var.find("JAVA_HOME=") == 0) {
          continue;
        }
        
        // Modify PATH to prepend Java 17 bin directory
        if (env_var.find("PATH=") == 0) {
          size_t equals_pos = env_var.find('=');
          std::string path_value = env_var.substr(equals_pos + 1);
          env_strings.push_back("PATH=" + std::string(java17_home) + "/bin:" + path_value);
        } else {
          env_strings.push_back(env_var);
        }
      }
      
      // Add JAVA_HOME pointing to Java 17
      env_strings.push_back(std::string("JAVA_HOME=") + java17_home);
#endif
      
      // Convert string vector to char* array for libuv
      for (auto& str : env_strings) {
        env_array.push_back(const_cast<char*>(str.c_str()));
      }
      env_array.push_back(nullptr);
      
      // Set the environment for the subprocess only
      options.env = env_array.data();
      
      // Log that we're using Java 17 environment
      std::cerr << "INFO: Using Java 17 environment for CCM (JAVA_HOME=" << java17_home << ")" << std::endl;
    }
  }
  // If use_java17_env is false or JAVA17_HOME not set, options.env remains nullptr 
  // which means the subprocess inherits the current environment unchanged

  // Spawn the process
  uv_process_t process;
  process.data = &result;
  int rc = uv_spawn(&loop, &process, &options);
  if (rc == 0) {

    // Configure the storage for the output pipes
    standard_output.data = &result.standard_output;
    standard_error.data = &result.standard_error;

    // Start the output thread loops
    uv_read_start(reinterpret_cast<uv_stream_t*>(&standard_output), Process::on_allocate,
                  Process::on_read);
    uv_read_start(reinterpret_cast<uv_stream_t*>(&standard_error), Process::on_allocate,
                  Process::on_read);

    // Start the process loop
    uv_run(&loop, UV_RUN_DEFAULT);
  } else {
    std::stringstream message;
    message << "Failed to spawn process with error \"" << uv_strerror(rc) << "\"";
    result.standard_error.append(message.str());
  }

  // Close the loop
  rc = uv_loop_close(&loop);
  if (rc != 0) {
    uv_print_all_handles(&loop, stderr);
    if (!result.standard_error.empty()) {
      std::cerr << result.standard_error << std::endl;
    }
    assert(false && "Process loop still has pending handles");
  }

  return result;
}

void Process::on_exit(uv_process_t* process, int64_t exit_status, int term_signal) {
  Result* result = reinterpret_cast<Result*>(process->data);
  result->exit_status = exit_status;
  uv_close(reinterpret_cast<uv_handle_t*>(process), NULL);
}

void Process::on_allocate(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = new char[suggested_size];
  buf->len = suggested_size;
}

void Process::on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  std::string* message = reinterpret_cast<std::string*>(stream->data);

  if (nread > 0) {
    message->append(buf->base, nread);
  } else if (nread < 0) {
    if (nread != UV_EOF) {
      LOG_ERROR("Failed to read stream with error \"" << uv_strerror(nread) << "\"");
    }
    uv_close(reinterpret_cast<uv_handle_t*>(stream), NULL);
  }

  delete[] buf->base;
}

} // namespace utils
