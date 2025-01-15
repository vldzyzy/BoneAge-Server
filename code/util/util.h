#pragma once
#include <string>
#include "InetAddress.h"

ssize_t readn(int fd, void *buff, size_t n);
ssize_t readn(int fd, std::string &inBuffer, bool &zero);
ssize_t readn(int fd, std::string &inBuffer);
ssize_t writen(int fd, void *buff, size_t n);
ssize_t writen(int fd, std::string &sbuff);
void handle_for_sigpipe();
int setNonBlocking(int fd);
void setNodelay(int fd);
void setNoLinger(int fd);
void shutDownWR(int fd);
int setReuseAddr(int fd);
int setReusePort(int fd);
int bindSocket(int fd, const InetAddress& addr);
int listenSocket(int fd, int backlog = SOMAXCONN);