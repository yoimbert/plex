/*
 *      Copyright (C) 2005-2008 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "DVDMessageQueue.h"
#include "DVDDemuxers/DVDDemuxUtils.h"
#include "utils/log.h"
#include "SingleLock.h"

using namespace std;

CDVDMessageQueue::CDVDMessageQueue(const string &owner)
{
  m_owner = owner;
  m_iDataSize     = 0;
  m_bAbortRequest = false;
  m_bInitialized  = false;
  m_bCaching      = false;
  m_bEmptied      = true;

  m_hEvent = CreateEvent(NULL, true, false, NULL);
}

CDVDMessageQueue::~CDVDMessageQueue()
{
  // remove all remaining messages
  Flush();

  CloseHandle(m_hEvent);
}

void CDVDMessageQueue::Init()
{
  m_iDataSize     = 0;
  m_bAbortRequest = false;
  m_bEmptied      = true;
  m_bInitialized  = true;
}

void CDVDMessageQueue::Flush(CDVDMsg::Message type)
{
  CSingleLock lock(m_section);

  for(SList::iterator it = m_list.begin(); it != m_list.end();)
  {
    if (it->pMsg->IsType(type) ||  type == CDVDMsg::NONE)
    {
      it->pMsg->Release();
      it = m_list.erase(it);
    }
    else
      it++;
  }

  if (type == CDVDMsg::DEMUXER_PACKET ||  type == CDVDMsg::NONE)
  {
    m_iDataSize = 0;
    m_bEmptied = true;
  }
}

void CDVDMessageQueue::Abort()
{
  CSingleLock lock(m_section);

  m_bAbortRequest = true;

  SetEvent(m_hEvent); // inform waiter for abort action
}

void CDVDMessageQueue::End()
{
  CSingleLock lock(m_section);

  Flush();

  m_bInitialized  = false;
  m_iDataSize     = 0;
  m_bAbortRequest = false;
}


MsgQueueReturnCode CDVDMessageQueue::Put(CDVDMsg* pMsg, int priority)
{
  CSingleLock lock(m_section);

  if (!m_bInitialized)
  {
    CLog::Log(LOGWARNING, "CDVDMessageQueue(%s)::Put MSGQ_NOT_INITIALIZED", m_owner.c_str());
    pMsg->Release();
    return MSGQ_NOT_INITIALIZED;
  }
  if (!pMsg)
  {
    CLog::Log(LOGFATAL, "CDVDMessageQueue(%s)::Put MSGQ_INVALID_MSG", m_owner.c_str());
    return MSGQ_INVALID_MSG;
  }

  DVDMessageListItem item;

  item.pMsg = pMsg;
  item.priority = priority;

  SList::iterator it = m_list.begin();
  while(it != m_list.end())
  {
    if(priority <= it->priority)
      break;
    it++;
  }
  m_list.insert(it, item);

  if (pMsg->IsType(CDVDMsg::DEMUXER_PACKET))
  {
    CDVDMsgDemuxerPacket* pMsgDemuxerPacket = (CDVDMsgDemuxerPacket*)pMsg;
    m_iDataSize += pMsgDemuxerPacket->GetPacketSize();
  }

  SetEvent(m_hEvent); // inform waiter for new packet

  return MSGQ_OK;
}

MsgQueueReturnCode CDVDMessageQueue::Get(CDVDMsg** pMsg, unsigned int iTimeoutInMilliSeconds, int priority)
{
  CSingleLock lock(m_section);

  *pMsg = NULL;

  int ret = 0;

  if (!m_bInitialized)
  {
    CLog::Log(LOGFATAL, "CDVDMessageQueue(%s)::Get MSGQ_NOT_INITIALIZED", m_owner.c_str());
    return MSGQ_NOT_INITIALIZED;
  }

  while (!m_bAbortRequest)
  {
    if(m_list.size() && m_list.back().priority >= priority && !m_bCaching)
    {
      DVDMessageListItem item(m_list.back());

      if (item.pMsg->IsType(CDVDMsg::DEMUXER_PACKET))
      {
        CDVDMsgDemuxerPacket* pMsgDemuxerPacket = (CDVDMsgDemuxerPacket*)item.pMsg;
        m_iDataSize -= pMsgDemuxerPacket->GetPacketSize();
        if(m_iDataSize == 0)
        {
          if(!m_bEmptied && m_owner != "teletext") // Prevent log flooding
            CLog::Log(LOGWARNING, "CDVDMessageQueue(%s)::Get - retrieved last data packet of queue", m_owner.c_str());
          m_bEmptied = true;
        }
        else
          m_bEmptied = false;
      }

      *pMsg = item.pMsg;
      m_list.pop_back();

      ret = MSGQ_OK;
      break;
    }
    else if (!iTimeoutInMilliSeconds)
    {
      ret = MSGQ_TIMEOUT;
      break;
    }
    else
    {
      ResetEvent(m_hEvent);
      lock.Leave();

      // wait for a new message
      if (WaitForSingleObject(m_hEvent, iTimeoutInMilliSeconds) == WAIT_TIMEOUT)
        return MSGQ_TIMEOUT;

      lock.Enter();
    }
  }

  if (m_bAbortRequest) return MSGQ_ABORT;

  return (MsgQueueReturnCode)ret;
}


unsigned CDVDMessageQueue::GetPacketCount(CDVDMsg::Message type)
{
  CSingleLock lock(m_section);

  if (!m_bInitialized)
    return 0;

  unsigned count = 0;
  for(SList::iterator it = m_list.begin(); it != m_list.end();it++)
  {
    if(it->pMsg->IsType(type))
      count++;
  }

  return count;
}

void CDVDMessageQueue::WaitUntilEmpty()
{
    CLog::Log(LOGNOTICE, "CDVDMessageQueue(%s)::WaitUntilEmpty", m_owner.c_str());
    CDVDMsgGeneralSynchronize* msg = new CDVDMsgGeneralSynchronize(40000, 0);
    Put(msg->Acquire());
    msg->Wait(&m_bAbortRequest, 0);
    msg->Release();
}
