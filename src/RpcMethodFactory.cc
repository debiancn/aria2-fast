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
#include "RpcMethodFactory.h"
#include "RpcMethodImpl.h"
#include "OptionParser.h"
#include "OptionHandler.h"

namespace aria2 {

namespace rpc {

namespace {
std::map<std::string, std::unique_ptr<RpcMethod>> cache;
} // namespace

namespace {
std::unique_ptr<RpcMethod> noSuchRpcMethod;
} // namespace

namespace {
std::unique_ptr<RpcMethod>
createMethod(const std::string& methodName)
{
  if(methodName == AddUriRpcMethod::getMethodName()) {
    return make_unique<AddUriRpcMethod>();
#ifdef ENABLE_BITTORRENT
  } else if(methodName == AddTorrentRpcMethod::getMethodName()) {
    return make_unique<AddTorrentRpcMethod>();
#endif // ENABLE_BITTORRENT
#ifdef ENABLE_METALINK
  }
  else if(methodName == AddMetalinkRpcMethod::getMethodName()) {
    return make_unique<AddMetalinkRpcMethod>();
#endif // ENABLE_METALINK
  }
  else if(methodName == RemoveRpcMethod::getMethodName()) {
    return make_unique<RemoveRpcMethod>();
  } else if(methodName == PauseRpcMethod::getMethodName()) {
    return make_unique<PauseRpcMethod>();
  } else if(methodName == ForcePauseRpcMethod::getMethodName()) {
    return make_unique<ForcePauseRpcMethod>();
  } else if(methodName == PauseAllRpcMethod::getMethodName()) {
    return make_unique<PauseAllRpcMethod>();
  } else if(methodName == ForcePauseAllRpcMethod::getMethodName()) {
    return make_unique<ForcePauseAllRpcMethod>();
  } else if(methodName == UnpauseRpcMethod::getMethodName()) {
    return make_unique<UnpauseRpcMethod>();
  } else if(methodName == UnpauseAllRpcMethod::getMethodName()) {
    return make_unique<UnpauseAllRpcMethod>();
  } else if(methodName == ForceRemoveRpcMethod::getMethodName()) {
    return make_unique<ForceRemoveRpcMethod>();
  } else if(methodName == ChangePositionRpcMethod::getMethodName()) {
    return make_unique<ChangePositionRpcMethod>();
  } else if(methodName == TellStatusRpcMethod::getMethodName()) {
    return make_unique<TellStatusRpcMethod>();
  } else if(methodName == GetUrisRpcMethod::getMethodName()) {
    return make_unique<GetUrisRpcMethod>();
  } else if(methodName == GetFilesRpcMethod::getMethodName()) {
    return make_unique<GetFilesRpcMethod>();
#ifdef ENABLE_BITTORRENT
  }
  else if(methodName == GetPeersRpcMethod::getMethodName()) {
    return make_unique<GetPeersRpcMethod>();
#endif // ENABLE_BITTORRENT
  } else if(methodName == GetServersRpcMethod::getMethodName()) {
    return make_unique<GetServersRpcMethod>();
  } else if(methodName == TellActiveRpcMethod::getMethodName()) {
    return make_unique<TellActiveRpcMethod>();
  } else if(methodName == TellWaitingRpcMethod::getMethodName()) {
    return make_unique<TellWaitingRpcMethod>();
  } else if(methodName == TellStoppedRpcMethod::getMethodName()) {
    return make_unique<TellStoppedRpcMethod>();
  } else if(methodName == GetOptionRpcMethod::getMethodName()) {
    return make_unique<GetOptionRpcMethod>();
  } else if(methodName == ChangeUriRpcMethod::getMethodName()) {
    return make_unique<ChangeUriRpcMethod>();
  } else if(methodName == ChangeOptionRpcMethod::getMethodName()) {
    return make_unique<ChangeOptionRpcMethod>();
  } else if(methodName == GetGlobalOptionRpcMethod::getMethodName()) {
    return make_unique<GetGlobalOptionRpcMethod>();
  } else if(methodName == ChangeGlobalOptionRpcMethod::getMethodName()) {
    return make_unique<ChangeGlobalOptionRpcMethod>();
  } else if(methodName == PurgeDownloadResultRpcMethod::getMethodName()) {
    return make_unique<PurgeDownloadResultRpcMethod>();
  } else if(methodName == RemoveDownloadResultRpcMethod::getMethodName()) {
    return make_unique<RemoveDownloadResultRpcMethod>();
  } else if(methodName == GetVersionRpcMethod::getMethodName()) {
    return make_unique<GetVersionRpcMethod>();
  } else if(methodName == GetSessionInfoRpcMethod::getMethodName()) {
    return make_unique<GetSessionInfoRpcMethod>();
  } else if(methodName == ShutdownRpcMethod::getMethodName()) {
    return make_unique<ShutdownRpcMethod>();
  } else if(methodName == ForceShutdownRpcMethod::getMethodName()) {
    return make_unique<ForceShutdownRpcMethod>();
  } else if(methodName == GetGlobalStatRpcMethod::getMethodName()) {
    return make_unique<GetGlobalStatRpcMethod>();
  } else if(methodName == SaveSessionRpcMethod::getMethodName()) {
    return make_unique<SaveSessionRpcMethod>();
  } else if(methodName == SystemMulticallRpcMethod::getMethodName()) {
    return make_unique<SystemMulticallRpcMethod>();
  } else {
    return nullptr;
  }
}
} // namespace

RpcMethod* getMethod(const std::string& methodName)
{
  auto itr = cache.find(methodName);
  if(itr == cache.end()) {
    auto m = createMethod(methodName);
    if(m) {
      auto rv = cache.insert(std::make_pair(methodName, std::move(m)));
      return (*rv.first).second.get();
    } else {
      if(!noSuchRpcMethod) {
        noSuchRpcMethod = make_unique<NoSuchMethodRpcMethod>();
      }
      return noSuchRpcMethod.get();
    }
  } else {
    return (*itr).second.get();
  }
}

} // namespace rpc

} // namespace aria2
