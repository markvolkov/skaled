/*
    Modifications Copyright (C) 2018-2019 SKALE Labs

    This file is part of cpp-ethereum.

    cpp-ethereum is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    cpp-ethereum is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file Worker.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "Worker.h"

#include "Log.h"

#include <skutils/utils.h>

#include <chrono>
#include <thread>

#include <libdevcore/microprofile.h>

using namespace std;
using namespace dev;

void Worker::startWorking() {
    //	cnote << "startWorking for thread" << m_name;
    std::unique_lock< std::mutex > l( x_work );
    if ( m_work ) {
        WorkerState ex = WorkerState::Stopped;
        m_state.compare_exchange_strong( ex, WorkerState::Starting );
        m_state_notifier.notify_all();
    } else {
        m_state = WorkerState::Starting;
        m_state_notifier.notify_all();
        m_work.reset( new thread( [&]() {
            setThreadName( m_name.c_str() );
            //			cnote << "Thread begins";
            while ( m_state != WorkerState::Killing ) {
                WorkerState ex = WorkerState::Starting;
                {
                    // the condition variable-related lock
                    unique_lock< mutex > l( x_work );
                    m_state = WorkerState::Started;
                }
                //				cnote << "Trying to set Started: Thread was" << (unsigned)ex << "; "
                //<< ok;
                m_state_notifier.notify_all();

                try {
                    startedWorking();
                    workLoop();
                    doneWorking();
                } catch ( std::exception const& _e ) {
                    cwarn << "Exception thrown in Worker thread: " << _e.what();
                    cwarn << DETAILED_ERROR;
                } catch ( ... ) {
                    cerror << "CRITICAL unknown exception thrown in Worker thread";
                    cerror << DETAILED_ERROR;
                    cerror << "\n" << skutils::signal::generate_stack_trace() << "\n" << std::endl;
                }

                //				ex = WorkerState::Stopping;
                //				m_state.compare_exchange_strong(ex, WorkerState::Stopped);

                {
                    // the condition variable-related lock
                    unique_lock< mutex > l( x_work );
                    ex = m_state.exchange( WorkerState::Stopped );
                    //					cnote << "State: Stopped: Thread was" << (unsigned)ex;
                    if ( ex == WorkerState::Killing || ex == WorkerState::Starting )
                        m_state.exchange( ex );
                }
                m_state_notifier.notify_all();
                //				cnote << "Waiting until not Stopped...";

                {
                    unique_lock< mutex > l( x_work );
                    DEV_TIMED_ABOVE( "Worker stopping", 100 )
                    while ( m_state == WorkerState::Stopped )
                        m_state_notifier.wait( l );
                }
            }
        } ) );
        //		cnote << "Spawning" << m_name;
    }

    DEV_TIMED_ABOVE( "Start worker", 100 )
    while ( m_state == WorkerState::Starting )
        m_state_notifier.wait( l );
}

void Worker::stopWorking() {
    std::unique_lock< Mutex > l( x_work );
    if ( m_work ) {
        MICROPROFILE_SCOPEI( "Worker", "stopWorking", MP_DIMGREY );
        WorkerState ex = WorkerState::Started;
        if ( !m_state.compare_exchange_strong( ex, WorkerState::Stopping ) )
            return;
        m_state_notifier.notify_all();

        DEV_TIMED_ABOVE( "Stop worker", 100 )
        while ( m_state != WorkerState::Stopped )
            m_state_notifier.wait( l );  // but yes who can wake this up, when the mutex is taken.
    }
}

void Worker::terminate() {
    //	cnote << "stopWorking for thread" << m_name;
    std::unique_lock< Mutex > l( x_work );
    if ( m_work ) {
        if ( m_state.exchange( WorkerState::Killing ) == WorkerState::Killing )
            return;  // Somebody else is doing this
        l.unlock();
        m_state_notifier.notify_all();
        DEV_TIMED_ABOVE( "Terminate worker", 100 )
        m_work->join();

        l.lock();
        m_work.reset();
    }
}

void Worker::workLoop() {
    while ( m_state == WorkerState::Started ) {
        if ( m_idleWaitMs ) {
            MICROPROFILE_SCOPEI( "Worker", "sleep_for m_idleWaitMs", MP_DIMGRAY );
            this_thread::sleep_for( chrono::milliseconds( m_idleWaitMs ) );
        }
        doWork();
    }
}
