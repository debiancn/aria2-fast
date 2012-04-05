/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2009 Tatsuhiro Tsujikawa
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */
/* copyright --> */
#include "HttpServer.h"

#include <sstream>

#include "HttpHeader.h"
#include "SocketCore.h"
#include "HttpHeaderProcessor.h"
#include "DlAbortEx.h"
#include "message.h"
#include "util.h"
#include "LogFactory.h"
#include "Logger.h"
#include "base64.h"
#include "a2functional.h"
#include "fmt.h"
#include "SocketRecvBuffer.h"
#include "TimeA2.h"
#include "array_fun.h"

namespace aria2 {

HttpServer::HttpServer
(const SharedHandle<SocketCore>& socket,
 DownloadEngine* e)
 : socket_(socket),
   socketRecvBuffer_(new SocketRecvBuffer(socket_)),
   socketBuffer_(socket),
   e_(e),
   headerProcessor_(new HttpHeaderProcessor()),
   keepAlive_(true),
   gzip_(false),
   acceptsPersistentConnection_(true),
   acceptsGZip_(false)
{}

HttpServer::~HttpServer() {}

SharedHandle<HttpHeader> HttpServer::receiveRequest()
{
  if(socketRecvBuffer_->bufferEmpty()) {
    if(socketRecvBuffer_->recv() == 0 &&
       !socket_->wantRead() && !socket_->wantWrite()) {
      throw DL_ABORT_EX(EX_EOF_FROM_PEER);
    }
  }
  headerProcessor_->update(socketRecvBuffer_->getBuffer(),
                           socketRecvBuffer_->getBufferLength());
  if(headerProcessor_->eoh()) {
    SharedHandle<HttpHeader> header = headerProcessor_->getHttpRequestHeader();
    size_t putbackDataLength = headerProcessor_->getPutBackDataLength();
    A2_LOG_INFO(fmt("HTTP Server received request\n%s",
                    headerProcessor_->getHeaderString().c_str()));
    socketRecvBuffer_->shiftBuffer
      (socketRecvBuffer_->getBufferLength()-putbackDataLength);
    lastRequestHeader_ = header;
    lastBody_.clear();
    lastBody_.str("");
    lastContentLength_ =
      lastRequestHeader_->findAsLLInt(HttpHeader::CONTENT_LENGTH);
    if(lastContentLength_ < 0) {
      throw DL_ABORT_EX("Content-Length must be positive.");
    }
    headerProcessor_->clear();

    const std::string& connection =
      lastRequestHeader_->find(HttpHeader::CONNECTION);
    acceptsPersistentConnection_ =
      util::strifind(connection.begin(),
                     connection.end(),
                     HttpHeader::CLOSE.begin(),
                     HttpHeader::CLOSE.end()) == connection.end() &&
      (lastRequestHeader_->getVersion() == HttpHeader::HTTP_1_1 ||
       util::strifind(connection.begin(),
                      connection.end(),
                      HttpHeader::KEEP_ALIVE.begin(),
                      HttpHeader::KEEP_ALIVE.end()) != connection.end());

    std::vector<Scip> acceptEncodings;
    const std::string& acceptEnc =
      lastRequestHeader_->find(HttpHeader::ACCEPT_ENCODING);
    util::splitIter(acceptEnc.begin(), acceptEnc.end(),
                    std::back_inserter(acceptEncodings), ',', true);
    acceptsGZip_ = false;
    for(std::vector<Scip>::const_iterator i = acceptEncodings.begin(),
          eoi = acceptEncodings.end(); i != eoi; ++i) {
      if(util::strieq((*i).first, (*i).second,
                      HttpHeader::GZIP.begin(), HttpHeader::GZIP.end())) {
        acceptsGZip_ = true;
        break;
      }
    }
    return header;
  } else {
    socketRecvBuffer_->clearBuffer();
    return SharedHandle<HttpHeader>();
  }
}

bool HttpServer::receiveBody()
{
  if(lastContentLength_ == 0) {
    return true;
  }
  if(socketRecvBuffer_->bufferEmpty()) {
    if(socketRecvBuffer_->recv() == 0 &&
       !socket_->wantRead() && !socket_->wantWrite()) {
      throw DL_ABORT_EX(EX_EOF_FROM_PEER);
    }
  }
  size_t length =
    std::min(socketRecvBuffer_->getBufferLength(),
             static_cast<size_t>(lastContentLength_-lastBody_.tellg()));
  lastBody_.write(reinterpret_cast<const char*>(socketRecvBuffer_->getBuffer()),
                  length);
  socketRecvBuffer_->shiftBuffer(length);
  return lastContentLength_ == lastBody_.tellp();
}

std::string HttpServer::getBody() const
{
  return lastBody_.str();
}

const std::string& HttpServer::getMethod() const
{
  return lastRequestHeader_->getMethod();
}

const std::string& HttpServer::getRequestPath() const
{
  return lastRequestHeader_->getRequestPath();
}

void HttpServer::feedResponse(std::string& text, const std::string& contentType)
{
  feedResponse("200 OK", "", text, contentType);
}

void HttpServer::feedResponse(const std::string& status,
                              const std::string& headers,
                              std::string& text,
                              const std::string& contentType)
{
  std::string httpDate = Time().toHTTPDate();
  std::string header= fmt("HTTP/1.1 %s\r\n"
                          "Date: %s\r\n"
                          "Content-Type: %s\r\n"
                          "Content-Length: %lu\r\n"
                          "Expires: %s\r\n"
                          "Cache-Control: no-cache\r\n"
                          "%s%s",
                          status.c_str(),
                          httpDate.c_str(),
                          contentType.c_str(),
                          static_cast<unsigned long>(text.size()),
                          httpDate.c_str(),
                          supportsGZip() ?
                          "Content-Encoding: gzip\r\n" : "",
                          !supportsPersistentConnection() ?
                          "Connection: close\r\n" : "");
  if(!allowOrigin_.empty()) {
    header += "Access-Control-Allow-Origin: ";
    header += allowOrigin_;
    header += "\r\n";
  }
  if(!headers.empty()) {
    header += headers;
    if(headers.size() < 2 ||
       (headers[headers.size()-2] != '\r' &&
        headers[headers.size()-1] != '\n')) {
      header += "\r\n";
    }
  }

  header += "\r\n";
  A2_LOG_DEBUG(fmt("HTTP Server sends response:\n%s", header.c_str()));
  socketBuffer_.pushStr(header);
  socketBuffer_.pushStr(text);
}

ssize_t HttpServer::sendResponse()
{
  return socketBuffer_.send();
}

bool HttpServer::sendBufferIsEmpty() const
{
  return socketBuffer_.sendBufferIsEmpty();
}

bool HttpServer::authenticate()
{
  if(username_.empty()) {
    return true;
  }

  const std::string& authHeader =
    lastRequestHeader_->find(HttpHeader::AUTHORIZATION);
  if(authHeader.empty()) {
    return false;
  }
  std::pair<Scip, Scip> p;
  util::divide(p, authHeader.begin(), authHeader.end(), ' ');
  if(!util::streq(p.first.first, p.first.second, "Basic")) {
    return false;
  }
  std::string userpass = base64::decode(p.second.first, p.second.second);
  std::pair<Sip, Sip> up;
  util::divide(up, userpass.begin(), userpass.end(), ':');
  return util::streq(up.first.first, up.first.second,
                     username_.begin(), username_.end()) &&
    util::streq(up.second.first, up.second.second,
                password_.begin(), password_.end());
}

void HttpServer::setUsernamePassword
(const std::string& username, const std::string& password)
{
  username_ = username;
  password_ = password;
}

} // namespace aria2
