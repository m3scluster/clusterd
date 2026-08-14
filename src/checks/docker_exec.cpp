// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <stout/json.hpp>
#include <stout/result.hpp>
#include <stout/stringify.hpp>

#include "docker/docker.hpp"

using std::cerr;
using std::endl;
using std::string;
using std::vector;

namespace {

struct Response
{
  int status;
  string body;
  string remainder;
};

bool writeAll(int fd, const char* data, size_t length)
{
  while (length > 0) {
    ssize_t written = ::write(fd, data, length);
    if (written < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }

    data += written;
    length -= written;
  }

  return true;
}

bool writeAll(int fd, const string& data)
{
  return writeAll(fd, data.data(), data.size());
}

bool sendAll(int fd, const char* data, size_t length)
{
  while (length > 0) {
    ssize_t sent = ::send(fd, data, length, MSG_NOSIGNAL);
    if (sent < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }

    data += sent;
    length -= sent;
  }

  return true;
}

int connectSocket(const string& configuredSocket)
{
  const string prefix = "unix://";
  const string socketPath = configuredSocket.compare(0, prefix.size(), prefix) == 0
    ? configuredSocket.substr(prefix.size())
    : configuredSocket;

  if (socketPath.size() >= sizeof(sockaddr_un::sun_path)) {
    cerr << "Docker socket path is too long" << endl;
    return -1;
  }

  int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    cerr << "Failed to create Docker socket: " << std::strerror(errno) << endl;
    return -1;
  }

  sockaddr_un address;
  std::memset(&address, 0, sizeof(address));
  address.sun_family = AF_UNIX;
  std::memcpy(address.sun_path, socketPath.c_str(), socketPath.size() + 1);

  if (::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
    cerr << "Failed to connect to Docker socket '" << socketPath
         << "': " << std::strerror(errno) << endl;
    ::close(fd);
    return -1;
  }

  return fd;
}

Result<Response> readResponse(int fd, bool streaming)
{
  string data;
  char buffer[8192];
  size_t headerEnd = string::npos;
  size_t delimiterLength = 0;

  while (headerEnd == string::npos) {
    ssize_t length = ::read(fd, buffer, sizeof(buffer));
    if (length < 0) {
      if (errno == EINTR) {
        continue;
      }
      return Error("Failed to read Docker response: " + string(std::strerror(errno)));
    }
    if (length == 0) {
      return Error("Docker closed the connection before sending HTTP headers");
    }

    data.append(buffer, length);
    headerEnd = data.find("\r\n\r\n");
    if (headerEnd != string::npos) {
      delimiterLength = 4;
    } else {
      // Some legacy Docker API proxies emit LF-only response headers for
      // streaming endpoints.
      headerEnd = data.find("\n\n");
      if (headerEnd != string::npos) {
        delimiterLength = 2;
      }
    }
  }

  const string headers = data.substr(0, headerEnd + delimiterLength);
  const size_t firstSpace = headers.find(' ');
  if (firstSpace == string::npos) {
    return Error("Malformed Docker HTTP status line");
  }

  const int status = std::atoi(headers.c_str() + firstSpace + 1);
  string body = data.substr(headerEnd + delimiterLength);

  if (streaming) {
    return Response{status, "", body};
  }

  size_t contentLength = 0;
  const string name = "Content-Length:";
  size_t position = headers.find(name);
  if (position != string::npos) {
    contentLength = std::strtoul(headers.c_str() + position + name.size(), nullptr, 10);
  }

  while (body.size() < contentLength) {
    ssize_t length = ::read(fd, buffer, sizeof(buffer));
    if (length < 0) {
      if (errno == EINTR) {
        continue;
      }
      return Error("Failed to read Docker response body: " +
                   string(std::strerror(errno)));
    }
    if (length == 0) {
      break;
    }
    body.append(buffer, length);
  }

  if (contentLength > 0 && body.size() > contentLength) {
    body.resize(contentLength);
  }

  return Response{status, body, ""};
}

Result<Response> request(
    const string& socket,
    const string& method,
    const string& endpoint,
    const string& body)
{
  int fd = connectSocket(socket);
  if (fd < 0) {
    return None();
  }

  const string headers =
    method + " " + endpoint + " HTTP/1.1\r\n" +
    "Host: localhost\r\n" +
    "Content-Type: application/json\r\n" +
    "Content-Length: " + stringify(body.size()) + "\r\n" +
    "Connection: close\r\n\r\n";

  if (!writeAll(fd, headers) || !writeAll(fd, body)) {
    ::close(fd);
    return Error("Failed to write Docker API request");
  }

  Result<Response> response = readResponse(fd, false);
  ::close(fd);
  return response;
}

string urlEncode(const string& value)
{
  static const char hex[] = "0123456789ABCDEF";
  string result;
  for (unsigned char c : value) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') {
      result.push_back(c);
    } else {
      result.push_back('%');
      result.push_back(hex[c >> 4]);
      result.push_back(hex[c & 0x0f]);
    }
  }
  return result;
}

bool relayStreams(int fd, bool tty, string pending)
{
  static const string cursorPositionReport = "\x1b[1;1R";

  char buffer[8192];
  bool inputOpen = true;
  Docker::TtyOutputFilter ttyOutputFilter;

  while (true) {
    if (tty && !pending.empty()) {
      const Docker::TtyOutput output = ttyOutputFilter.process(pending);

      for (size_t i = 0; i < output.cursorPositionQueries; ++i) {
        if (!sendAll(
                fd,
                cursorPositionReport.data(),
                cursorPositionReport.size())) {
          cerr << "Failed to answer Docker exec cursor position query: "
               << std::strerror(errno) << endl;
          return false;
        }
      }

      if (!writeAll(STDOUT_FILENO, output.data)) {
        return false;
      }
      pending.clear();
    }

    if (!tty) {
      while (pending.size() >= 8) {
        const unsigned char* header =
          reinterpret_cast<const unsigned char*>(pending.data());
        const size_t length =
          (static_cast<size_t>(header[4]) << 24) |
          (static_cast<size_t>(header[5]) << 16) |
          (static_cast<size_t>(header[6]) << 8) |
          static_cast<size_t>(header[7]);

        if (pending.size() < 8 + length) {
          break;
        }

        const int output = header[0] == 2 ? STDERR_FILENO : STDOUT_FILENO;
        if (!writeAll(output, pending.data() + 8, length)) {
          return false;
        }
        pending.erase(0, 8 + length);
      }
    }

    pollfd descriptors[2] = {
      {fd, POLLIN, 0},
      {STDIN_FILENO, POLLIN, 0}};

    int ready = ::poll(descriptors, inputOpen ? 2 : 1, -1);
    if (ready < 0) {
      if (errno != EINTR) {
        cerr << "Failed to poll Docker exec streams: "
             << std::strerror(errno) << endl;
        return false;
      }
      continue;
    }

    if ((descriptors[0].revents & (POLLERR | POLLNVAL)) != 0 ||
        (inputOpen &&
         (descriptors[1].revents & (POLLERR | POLLNVAL)) != 0)) {
      cerr << "Docker exec stream polling failed" << endl;
      return false;
    }

    if (inputOpen &&
        (descriptors[1].revents & (POLLIN | POLLHUP)) != 0) {
      ssize_t length = ::read(STDIN_FILENO, buffer, sizeof(buffer));
      if (length < 0) {
        if (errno != EINTR) {
          cerr << "Failed to read Docker exec input: "
               << std::strerror(errno) << endl;
          return false;
        }
      } else if (length == 0) {
        inputOpen = false;
        // Legacy Docker proxies such as Weave discard pending TTY output when
        // the client half-closes the upgraded connection. Keep the socket
        // writable until the Docker exec process exits; non-TTY streams
        // support the regular EOF propagation.
        if (!tty) {
          ::shutdown(fd, SHUT_WR);
        }
      } else if (!sendAll(fd, buffer, length)) {
        cerr << "Failed to write Docker exec input: "
             << std::strerror(errno) << endl;
        return false;
      }
    }

    if ((descriptors[0].revents & (POLLIN | POLLHUP)) != 0) {
      ssize_t length = ::read(fd, buffer, sizeof(buffer));
      if (length < 0) {
        if (errno != EINTR) {
          cerr << "Failed to read Docker exec stream: "
               << std::strerror(errno) << endl;
          return false;
        }
      } else if (length == 0) {
        break;
      } else {
        pending.append(buffer, length);
      }
    }
  }

  if (tty) {
    if (!writeAll(STDOUT_FILENO, ttyOutputFilter.flush())) {
      return false;
    }
  } else if (!pending.empty()) {
    cerr << "Docker exec stream ended with an incomplete frame" << endl;
    return false;
  }

  return true;
}

} // namespace {

int main(int argc, char** argv)
{
  if (argc != 2 || string(argv[1]).compare(0, 9, "--config=") != 0) {
    cerr << "Usage: mesos-docker-exec --config=<json>" << endl;
    return EXIT_FAILURE;
  }

  Try<JSON::Object> configuration =
    JSON::parse<JSON::Object>(string(argv[1]).substr(9));
  if (configuration.isError()) {
    cerr << "Invalid Docker exec configuration: " << configuration.error() << endl;
    return EXIT_FAILURE;
  }

  Result<JSON::String> socket = configuration->find<JSON::String>("socket");
  Result<JSON::String> container =
    configuration->find<JSON::String>("container");
  Result<JSON::Array> command = configuration->find<JSON::Array>("command");
  Result<JSON::Array> environment = configuration->find<JSON::Array>("environment");
  Result<JSON::Boolean> tty = configuration->find<JSON::Boolean>("tty");
  Result<JSON::String> user = configuration->find<JSON::String>("user");

  if (socket.isNone() || container.isNone() || command.isNone() ||
      environment.isNone() || tty.isNone()) {
    cerr << "Docker exec configuration is missing a required field" << endl;
    return EXIT_FAILURE;
  }

  if (tty->value) {
    Try<Nothing> configured = Docker::configureTtyInput(STDIN_FILENO);
    if (configured.isError()) {
      cerr << configured.error() << endl;
      return EXIT_FAILURE;
    }
  }

  JSON::Array commandJson;
  foreach (const JSON::Value& value, command->values) {
    commandJson.values.push_back(value);
  }

  JSON::Array environmentJson;
  foreach (const JSON::Value& value, environment->values) {
    environmentJson.values.push_back(value);
  }

  JSON::Object createBody;
  createBody.values["AttachStdin"] = true;
  createBody.values["AttachStdout"] = true;
  createBody.values["AttachStderr"] = true;
  createBody.values["Tty"] = tty->value;
  createBody.values["Cmd"] = commandJson;
  createBody.values["Env"] = environmentJson;
  if (user.isSome()) {
    createBody.values["User"] = user->value;
  }

  Result<Response> create = request(
      socket->value,
      "POST",
      "/containers/" + urlEncode(container->value) + "/exec",
      stringify(createBody));

  if (!create.isSome() || create->status < 200 || create->status >= 300) {
    cerr << "Docker exec create failed";
    if (create.isError()) {
      cerr << ": " << create.error();
    } else if (create.isSome()) {
      cerr << " with HTTP " << create->status << ": " << create->body;
    }
    cerr << endl;
    return EXIT_FAILURE;
  }

  Try<JSON::Object> created = JSON::parse<JSON::Object>(create->body);
  if (created.isError()) {
    cerr << "Invalid Docker exec create response: " << created.error() << endl;
    return EXIT_FAILURE;
  }

  Result<JSON::String> execId = created->find<JSON::String>("Id");
  if (execId.isNone()) {
    cerr << "Docker exec create response has no Id" << endl;
    return EXIT_FAILURE;
  }

  JSON::Object startBody;
  startBody.values["Detach"] = false;
  startBody.values["Tty"] = tty->value;
  const string startJson = stringify(startBody);

  int stream = connectSocket(socket->value);
  if (stream < 0) {
    return EXIT_FAILURE;
  }

  const string startHeaders =
    "POST /exec/" + urlEncode(execId->value) + "/start HTTP/1.1\r\n" +
    "Host: localhost\r\n" +
    "Content-Type: application/json\r\n" +
    // Do not request an HTTP connection upgrade here. Legacy Docker proxies
    // such as Weave 2.12 return 101 but fail to bridge the hijacked stream.
    // Docker's regular HTTP 200 streaming mode works through those proxies.
    "Content-Length: " + stringify(startJson.size()) + "\r\n\r\n";

  if (!writeAll(stream, startHeaders) || !writeAll(stream, startJson)) {
    cerr << "Failed to start Docker exec" << endl;
    ::close(stream);
    return EXIT_FAILURE;
  }

  Result<Response> started = readResponse(stream, true);
  if (!started.isSome() ||
      (started->status != 101 && started->status != 200)) {
    cerr << "Docker exec start failed";
    if (started.isError()) {
      cerr << ": " << started.error();
    } else if (started.isSome()) {
      cerr << " with HTTP " << started->status;
    }
    cerr << endl;
    ::close(stream);
    return EXIT_FAILURE;
  }

  const bool relayed = relayStreams(stream, tty->value, started->remainder);
  ::shutdown(stream, SHUT_RDWR);
  ::close(stream);

  if (!relayed) {
    return EXIT_FAILURE;
  }

  // The stream can close just before Docker publishes the exit code. During
  // that small window `/exec/{id}/json` reports `"ExitCode": null`.
  for (size_t attempt = 0; attempt < 500; ++attempt) {
    Result<Response> inspect = request(
        socket->value,
        "GET",
        "/exec/" + urlEncode(execId->value) + "/json",
        "");
    if (!inspect.isSome() || inspect->status != 200) {
      cerr << "Docker exec inspect failed" << endl;
      return EXIT_FAILURE;
    }

    Try<JSON::Object> inspected = JSON::parse<JSON::Object>(inspect->body);
    if (inspected.isError()) {
      cerr << "Invalid Docker exec inspect response: "
           << inspected.error() << endl;
      return EXIT_FAILURE;
    }

    Result<JSON::Number> exitCode =
      inspected->find<JSON::Number>("ExitCode");
    if (exitCode.isSome()) {
      return static_cast<int>(exitCode->as<int64_t>());
    }

    ::usleep(10000);
  }

  cerr << "Docker exec inspect did not publish an ExitCode" << endl;
  return EXIT_FAILURE;
}
