/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2006 Tatsuhiro Tsujikawa
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
#include "ServerStatMan.h"

#include <cstring>
#include <cstdio>
#include <algorithm>
#include <iterator>
#include <map>
#include <vector>

#include "ServerStat.h"
#include "util.h"
#include "RecoverableException.h"
#include "a2functional.h"
#include "BufferedFile.h"
#include "message.h"
#include "fmt.h"
#include "LogFactory.h"
#include "File.h"

namespace aria2 {

ServerStatMan::ServerStatMan() {}

ServerStatMan::~ServerStatMan() {}

SharedHandle<ServerStat> ServerStatMan::find(const std::string& hostname,
                                             const std::string& protocol) const
{
  SharedHandle<ServerStat> ss(new ServerStat(hostname, protocol));
  std::deque<SharedHandle<ServerStat> >::const_iterator i =
    std::lower_bound(serverStats_.begin(), serverStats_.end(), ss,
                     DerefLess<SharedHandle<ServerStat> >());
  if(i != serverStats_.end() &&
     (*i)->getHostname() == hostname && (*i)->getProtocol() == protocol) {
    return *i;
  } else {
    return SharedHandle<ServerStat>();
  }
}

bool ServerStatMan::add(const SharedHandle<ServerStat>& serverStat)
{
  std::deque<SharedHandle<ServerStat> >::iterator i =
    std::lower_bound(serverStats_.begin(), serverStats_.end(), serverStat,
                     DerefLess<SharedHandle<ServerStat> >());

  if(i != serverStats_.end() && *(*i) == *serverStat) {
    return false;
  } else {
    serverStats_.insert(i, serverStat);
    return true;
  } 
}

bool ServerStatMan::save(const std::string& filename) const
{
  std::string tempfile = filename;
  tempfile += "__temp";
  {
    BufferedFile fp(tempfile, BufferedFile::WRITE);
    if(!fp) {
      A2_LOG_ERROR(fmt(MSG_OPENING_WRITABLE_SERVER_STAT_FILE_FAILED,
                       filename.c_str()));
      return false;
    }
    for(std::deque<SharedHandle<ServerStat> >::const_iterator i =
          serverStats_.begin(), eoi = serverStats_.end(); i != eoi; ++i) {
      std::string l = (*i)->toString();
      l += "\n";
      if(fp.write(l.data(), l.size()) != l.size()) {
        A2_LOG_ERROR(fmt(MSG_WRITING_SERVER_STAT_FILE_FAILED,
                         filename.c_str()));
      }
    }
    if(fp.close() == EOF) {
      A2_LOG_ERROR(fmt(MSG_WRITING_SERVER_STAT_FILE_FAILED, filename.c_str()));
      return false;
    }
  }
  if(File(tempfile).renameTo(filename)) {
    A2_LOG_NOTICE(fmt(MSG_SERVER_STAT_SAVED, filename.c_str()));
    return true;
  } else {
    A2_LOG_ERROR(fmt(MSG_WRITING_SERVER_STAT_FILE_FAILED, filename.c_str()));
    return false;
  }
}

bool ServerStatMan::load(const std::string& filename)
{
  static const std::string S_HOST = "host";
  static const std::string S_PROTOCOL = "protocol";
  static const std::string S_DL_SPEED = "dl_speed";
  static const std::string S_SC_AVG_SPEED = "sc_avg_speed";
  static const std::string S_MC_AVG_SPEED = "mc_avg_speed";
  static const std::string S_LAST_UPDATED = "last_updated";
  static const std::string S_COUNTER = "counter";
  static const std::string S_STATUS = "status";

  BufferedFile fp(filename, BufferedFile::READ);
  if(!fp) {
    A2_LOG_ERROR(fmt(MSG_OPENING_READABLE_SERVER_STAT_FILE_FAILED,
                     filename.c_str()));
    return false;
  }
  char buf[4096];
  while(1) {
    if(!fp.getsn(buf, sizeof(buf))) {
      if(fp.eof()) {
        break;
      } else {
        A2_LOG_ERROR(fmt(MSG_READING_SERVER_STAT_FILE_FAILED,
                         filename.c_str()));
        return false;
      }
    }
    std::pair<std::string::const_iterator,
              std::string::const_iterator> p =
      util::stripIter(&buf[0], &buf[strlen(buf)]);
    if(p.first == p.second) {
      continue;
    }
    std::string line(p.first, p.second);
    std::vector<Scip> items;
    util::splitIter(line.begin(), line.end(), std::back_inserter(items), ',');
    std::map<std::string, std::string> m;
    for(std::vector<Scip>::const_iterator i = items.begin(),
          eoi = items.end(); i != eoi; ++i) {
      std::pair<Scip, Scip> p;
      util::divide(p, (*i).first, (*i).second, '=');
      m[std::string(p.first.first, p.first.second)] =
        std::string(p.second.first, p.second.second);
    }
    if(m[S_HOST].empty() || m[S_PROTOCOL].empty()) {
      continue;
    }
    SharedHandle<ServerStat> sstat(new ServerStat(m[S_HOST], m[S_PROTOCOL]));
    try {
      const std::string& dlSpeed = m[S_DL_SPEED];
      sstat->setDownloadSpeed(util::parseUInt(dlSpeed));
      // Old serverstat file doesn't contains SC_AVG_SPEED
      if(m.find(S_SC_AVG_SPEED) != m.end()) {
        const std::string& s = m[S_SC_AVG_SPEED];
        sstat->setSingleConnectionAvgSpeed(util::parseUInt(s));
      }
      // Old serverstat file doesn't contains MC_AVG_SPEED
      if(m.find(S_MC_AVG_SPEED) != m.end()) {
        const std::string& s = m[S_MC_AVG_SPEED];
        sstat->setMultiConnectionAvgSpeed(util::parseUInt(s));
      }
      // Old serverstat file doesn't contains COUNTER_SPEED
      if(m.find(S_COUNTER) != m.end()) {
        const std::string& s = m[S_COUNTER];
        sstat->setCounter(util::parseUInt(s));
      }
      const std::string& lastUpdated = m[S_LAST_UPDATED];
      sstat->setLastUpdated(Time(util::parseInt(lastUpdated)));
      sstat->setStatus(m[S_STATUS]);
      add(sstat);
    } catch(RecoverableException& e) {
      continue;
    }
  }
  A2_LOG_NOTICE(fmt(MSG_SERVER_STAT_LOADED, filename.c_str()));
  return true;
}

namespace {
class FindStaleServerStat {
private:
  time_t timeout_;
  Time time_;
public:
  FindStaleServerStat(time_t timeout):timeout_(timeout) {}

  bool operator()(const SharedHandle<ServerStat>& ss) const
  {
    return ss->getLastUpdated().difference(time_) >= timeout_;
  }
};
} // namespace

void ServerStatMan::removeStaleServerStat(time_t timeout)
{
  serverStats_.erase(std::remove_if(serverStats_.begin(), serverStats_.end(),
                                    FindStaleServerStat(timeout)),
                     serverStats_.end());
}

} // namespace aria2
