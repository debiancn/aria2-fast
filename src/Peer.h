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
#ifndef D_PEER_H
#define D_PEER_H

#include "common.h"

#include <cassert>
#include <string>
#include <vector>
#include <algorithm>

#include "SharedHandle.h"
#include "TimerA2.h"
#include "BtConstants.h"
#include "PeerStat.h"
#include "a2functional.h"
#include "Command.h"

namespace aria2 {

class PeerSessionResource;
class BtMessageDispatcher;

class Peer {
private:
  std::string ipaddr_;
  // TCP port of the other end of communication.  If incoming_ is
  // true, then this port is not a port the peer is listening to and
  // we cannot connect to it.
  uint16_t port_;

  std::string id_;

  cuid_t cuid_;

  unsigned char peerId_[PEER_ID_LENGTH];

  Timer firstContactTime_;

  Timer badConditionStartTime_;

  bool seeder_;

  PeerSessionResource* res_;

  // If true, port is assumed not to be a listening port.
  bool incoming_;

  // If true, this peer is from local network.
  bool localPeer_;

  // If true, this peer is disconnected gracefully.
  bool disconnectedGracefully_;

  // Before calling updateSeeder(),  make sure that
  // allocateSessionResource() is called and res_ is created.
  // Otherwise assertion fails.
  void updateSeeder();
public:
  Peer(std::string ipaddr, uint16_t port, bool incoming = false);

  ~Peer();

  bool operator==(const Peer& p)
  {
    return id_ == p.id_;
  }
  
  bool operator!=(const Peer& p)
  {
    return !(*this == p);
  }

  void resetStatus();

  const std::string& getIPAddress() const
  {
    return ipaddr_;
  }

  uint16_t getPort() const
  {
    return port_;
  }

  void setPort(uint16_t port)
  {
    port_ = port;
  }

  void usedBy(cuid_t cuid);

  cuid_t usedBy() const
  {
    return cuid_;
  }

  bool unused() const
  {
    return cuid_ == 0;
  }

  // Returns true iff res_ != 0.
  bool isActive() const
  {
    return res_ != 0;
  }

  void setPeerId(const unsigned char* peerId);

  const unsigned char* getPeerId() const
  {
    return peerId_;
  }

  bool isSeeder() const
  {
    return seeder_;
  }

  const std::string& getID() const
  {
    return id_;
  }

  void startBadCondition();

  bool isGood() const;

  void allocateSessionResource(int32_t pieceLength, off_t totalLength);

  void reconfigureSessionResource(int32_t pieceLength, off_t totalLength);

  void releaseSessionResource();

  const Timer& getFirstContactTime() const
  {
    return firstContactTime_;
  }

  void setFirstContactTime(const Timer& time);

  const Timer& getBadConditionStartTime() const
  {
    return badConditionStartTime_;
  }

  // Before calling following member functions,  make sure that
  // allocateSessionResource() is called and res_ is created.
  // Otherwise assertion fails.

  // localhost is choking this peer
  bool amChoking() const;

  void amChoking(bool b) const;

  // localhost is interested in this peer
  bool amInterested() const;

  void amInterested(bool b) const;

  // this peer is choking localhost
  bool peerChoking() const;

  void peerChoking(bool b) const;

  // this peer is interested in localhost
  bool peerInterested() const;

  void peerInterested(bool b);
  
  // this peer should be choked
  bool chokingRequired() const;

  void chokingRequired(bool b);

  // this peer is eligible for unchoking optionally.
  bool optUnchoking() const;

  void optUnchoking(bool b);

  // this peer is snubbing.
  bool snubbing() const;

  void snubbing(bool b);

  void updateUploadLength(int32_t bytes);

  void updateDownloadLength(int32_t bytes);

  /**
   * Returns the transfer rate from localhost to remote host.
   */
  int calculateUploadSpeed();

  /**
   * Returns the transfer rate from remote host to localhost.
   */
  int calculateDownloadSpeed();

  /**
   * Returns the number of bytes uploaded to the remote host.
   */
  off_t getSessionUploadLength() const;

  /**
   * Returns the number of bytes downloaded from the remote host.
   */
  off_t getSessionDownloadLength() const;
  
  void setBitfield(const unsigned char* bitfield, size_t bitfieldLength);

  const unsigned char* getBitfield() const;

  size_t getBitfieldLength() const;

  void setAllBitfield();

  /**
   * operation = 1: set index-th bit to 1
   * operation = 0: set index-th bit to 0
   */
  void updateBitfield(size_t index, int operation);
  
  void setFastExtensionEnabled(bool enabled);

  bool isFastExtensionEnabled() const;

  void addPeerAllowedIndex(size_t index);

  bool isInPeerAllowedIndexSet(size_t index) const;

  size_t countPeerAllowedIndexSet() const;

  const std::vector<size_t>& getPeerAllowedIndexSet() const;

  void addAmAllowedIndex(size_t index);

  bool isInAmAllowedIndexSet(size_t index) const;

  void setExtendedMessagingEnabled(bool enabled);

  bool isExtendedMessagingEnabled() const;

  void setDHTEnabled(bool enabled);

  bool isDHTEnabled() const;

  bool shouldBeChoking() const;

  bool hasPiece(size_t index) const;

  uint8_t getExtensionMessageID(const std::string& name) const;

  std::string getExtensionName(uint8_t id) const;

  void setExtension(const std::string& name, uint8_t id);

  const Timer& getLastDownloadUpdate() const;

  const Timer& getLastAmUnchoking() const;

  off_t getCompletedLength() const;

  bool isIncomingPeer() const
  {
    return incoming_;
  }

  void setIncomingPeer(bool incoming);

  bool isLocalPeer() const
  {
    return localPeer_;
  }

  void setLocalPeer(bool flag)
  {
    localPeer_ = flag;
  }

  bool isDisconnectedGracefully() const
  {
    return disconnectedGracefully_;
  }

  void setDisconnectedGracefully(bool f)
  {
    disconnectedGracefully_ = f;
  }

  void setBtMessageDispatcher(BtMessageDispatcher* dpt);

  size_t countOutstandingUpload() const;
};

template<typename InputIterator>
size_t countSeeder(InputIterator first, InputIterator last)
{
  return std::count_if(first, last, mem_fun_sh(&Peer::isSeeder));
}

} // namespace aria2

#endif // D_PEER_H
