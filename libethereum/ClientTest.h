/*
    Modifications Copyright (C) 2018 SKALE Labs

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
/** @file ClientTest.h
 * @author Kholhlov Dimitry <dimitry@ethdev.com>
 * @date 2016
 */

#pragma once

#include <libethereum/Client.h>
#include <boost/filesystem/path.hpp>
#include <tuple>

namespace dev {
namespace eth {

DEV_SIMPLE_EXCEPTION( ChainParamsInvalid );
DEV_SIMPLE_EXCEPTION( ImportBlockFailed );

class ClientTest : public Client {
public:
    /// Trivial forwarding constructor.
    ClientTest( ChainParams const& _params, int _networkID,
        std::shared_ptr< GasPricer > _gpForAdoption,
        std::shared_ptr< SnapshotManager > _snapshotManager,
        std::shared_ptr< InstanceMonitor > _instanceMonitor,
        boost::filesystem::path const& _dbPath = boost::filesystem::path(),
        WithExisting _forceAction = WithExisting::Trust,
        TransactionQueue::Limits const& _l = TransactionQueue::Limits{ 1024, 1024 } );
    ~ClientTest();

    bool mineBlocks( unsigned _count ) noexcept;
    void modifyTimestamp( int64_t _timestamp );
    void rewindToBlock( unsigned _number );
    h256 importRawBlock( std::string const& _blockRLP );

protected:
    unsigned const m_singleBlockMaxMiningTimeInSeconds = 5;
};

ClientTest& asClientTest( Interface& _c );
ClientTest* asClientTest( Interface* _c );

}  // namespace eth
}  // namespace dev
