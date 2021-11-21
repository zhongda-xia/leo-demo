/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2013-2019 Regents of the University of California.
 *
 * This file is part of ndn-cxx library (NDN C++ library with eXperimental eXtensions).
 *
 * ndn-cxx library is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * ndn-cxx library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 * You should have received copies of the GNU General Public License and GNU Lesser
 * General Public License along with ndn-cxx, e.g., in COPYING.md file.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * See AUTHORS.md for complete list of ndn-cxx authors and contributors.
 */

#ifndef NDN_UTIL_SCHEDULER_HPP
#define NDN_UTIL_SCHEDULER_HPP

#include "ndn-cxx/detail/asio-fwd.hpp"
#include "ndn-cxx/detail/cancel-handle.hpp"
#include "ndn-cxx/util/time.hpp"

#include "ns3/simulator.h"

#include <set>

namespace ndn {
namespace util {
namespace scheduler {

class Scheduler;
class EventInfo;

/** \brief Function to be invoked when a scheduled event expires
 */
using EventCallback = std::function<void()>;

/** \brief A handle of scheduled event.
 *
 *  \code
 *  EventId eid = scheduler.scheduleEvent(10_ms, [] { doSomething(); });
 *  eid.cancel(); // cancel the event
 *  \endcode
 *
 *  \note Canceling an expired (executed) or canceled event has no effect.
 *  \warning Canceling an event after the scheduler has been destructed may trigger undefined
 *           behavior.
 */
class EventId : public ndn::detail::CancelHandle
{
public:
  /** \brief Constructs an empty EventId
   */
  EventId() noexcept = default;

  /** \brief Allow implicit conversion from nullptr.
   */
  [[deprecated]]
  EventId(std::nullptr_t) noexcept
  {
  }

  /** \brief Determine whether the event is valid.
   *  \retval true The event is valid.
   *  \retval false This EventId is empty, or the event is expired or cancelled.
   */
  explicit
  operator bool() const noexcept;

  /** \brief Determine whether this and other refer to the same event, or are both
   *         empty/expired/cancelled.
   */
  bool
  operator==(const EventId& other) const noexcept;

  bool
  operator!=(const EventId& other) const noexcept
  {
    return !this->operator==(other);
  }

  /** \brief Clear this EventId without canceling.
   *  \post !(*this)
   */
  void
  reset() noexcept;

private:
  EventId(Scheduler& sched, weak_ptr<EventInfo> info);

private:
  weak_ptr<EventInfo> m_info;

  friend class Scheduler;
  friend std::ostream& operator<<(std::ostream& os, const EventId& eventId);
};

std::ostream&
operator<<(std::ostream& os, const EventId& eventId);

/** \brief A scoped handle of scheduled event.
 *
 *  Upon destruction of this handle, the event is canceled automatically.
 *  Most commonly, the application keeps a ScopedEventId as a class member field, so that it can
 *  cleanup its event when the class instance is destructed.
 *
 *  \code
 *  {
 *    ScopedEventId eid = scheduler.scheduleEvent(10_ms, [] { doSomething(); });
 *  } // eid goes out of scope, canceling the event
 *  \endcode
 *
 *  \note Canceling an expired (executed) or canceled event has no effect.
 *  \warning Canceling an event after the scheduler has been destructed may trigger undefined
 *           behavior.
 */
class ScopedEventId : public ndn::detail::ScopedCancelHandle
{
public:
  using ScopedCancelHandle::ScopedCancelHandle;

  ScopedEventId() noexcept = default;

  /** \deprecated Scheduler argument is no longer necessary. Use default construction instead.
   */
  [[deprecated]]
  explicit
  ScopedEventId(Scheduler& scheduler) noexcept
  {
  }
};

class EventQueueCompare
{
public:
  bool
  operator()(const shared_ptr<EventInfo>& a, const shared_ptr<EventInfo>& b) const noexcept;
};

using EventQueue = std::multiset<shared_ptr<EventInfo>, EventQueueCompare>;

/** \brief Generic scheduler
 */
class Scheduler : noncopyable
{
public:
  explicit
  Scheduler(DummyIoService& ioService);

  ~Scheduler();

  /** \brief Schedule a one-time event after the specified delay
   *  \return EventId that can be used to cancel the scheduled event
   */
  EventId
  scheduleEvent(time::nanoseconds after, const EventCallback& callback);

  /** \brief Cancel a scheduled event
   *
   *  You may also invoke `eid.cancel()`
   */
  void
  cancelEvent(const EventId& eid)
  {
    eid.cancel();
  }

  /** \brief Cancel all scheduled events
   */
  void
  cancelAllEvents();

private:
  void
  cancelImpl(const shared_ptr<EventInfo>& info);

  /** \brief Schedule the next event on the deadline timer
   */
  void
  scheduleNext();

  /** \brief Execute expired events
   */
  void
  executeEvent();

private:
  EventQueue m_queue;
  bool m_isEventExecuting;
  ndn::optional<ns3::EventId> m_timerEvent;

  friend EventId;
};

} // namespace scheduler

using util::scheduler::Scheduler;

} // namespace util

// for backwards compatibility
using util::scheduler::Scheduler;
using util::scheduler::EventId;

} // namespace ndn

#endif // NDN_UTIL_SCHEDULER_HPP
