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
/** @file Account.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "Account.h"
#include "ValidationSchemes.h"
#include <libdevcore/JsonUtils.h>
#include <libethcore/ChainOperationParams.h>
#include <libethereum/Precompiled.h>

#include "ValidationSchemes.h"

using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace dev::eth::validation;

namespace fs = boost::filesystem;

std::unordered_map< u256, u256 > const& Account::originalStorageValue() const {
    return m_storageOriginal;
}

void Account::setCode( bytes&& _code, u256 const& _version ) {
    auto const newHash = sha3( _code );
    if ( newHash != m_codeHash ) {
        m_codeCache = std::move( _code );
        m_hasNewCode = true;
        m_codeHash = newHash;
    }
    m_version = _version;
}

void Account::resetCode() {
    m_codeCache.clear();
    m_hasNewCode = false;
    m_codeHash = EmptySHA3;
    // Reset the version, as it was set together with code
    m_version = 0;
}

namespace js = json_spirit;

namespace {

uint64_t toUnsigned( js::mValue const& _v ) {
    switch ( _v.type() ) {
    case js::int_type:
        return _v.get_uint64();
    case js::str_type:
        return fromBigEndian< uint64_t >( fromHex( _v.get_str() ) );
    default:
        return 0;
    }
}

PrecompiledContract createPrecompiledContract( js::mObject const& _precompiled ) {
    auto n = _precompiled.at( "name" ).get_str();
    try {
        u256 startingBlock = 0;
        if ( _precompiled.count( "startingBlock" ) )
            startingBlock = u256( _precompiled.at( "startingBlock" ).get_str() );

        if ( !_precompiled.count( "linear" ) )
            return PrecompiledContract( PrecompiledRegistrar::pricer( n ),
                PrecompiledRegistrar::executor( n ), startingBlock );

        auto const& l = _precompiled.at( "linear" ).get_obj();
        unsigned base = toUnsigned( l.at( "base" ) );
        unsigned word = toUnsigned( l.at( "word" ) );

        h160Set allowedAddresses;

        auto restrictAccessIt = _precompiled.find( c_restrictAccess );
        if ( restrictAccessIt != _precompiled.end() ) {
            auto& obj = restrictAccessIt->second;
            if ( obj.type() == json_spirit::array_type ) {
                auto& arr = obj.get_array();
                for ( auto& el : arr ) {
                    if ( el.type() == json_spirit::str_type ) {
                        allowedAddresses.insert( Address( fromHex( el.get_str() ) ) );
                    } else {
                        cerror << "Error parsing access restrictions for precompiled " << n
                               << "! It should contain strings!\n";
                        cerror << DETAILED_ERROR;
                    }
                }  // for
            } else {
                cerror << "Error parsing access restrictions for precompiled " << n
                       << "! It should be array!\n";
                cerror << DETAILED_ERROR;
            }
        }  // restrictAccessIt

        return PrecompiledContract(
            base, word, PrecompiledRegistrar::executor( n ), startingBlock, allowedAddresses );
    } catch ( PricerNotFound const& ) {
        cwarn << "Couldn't create a precompiled contract account. Missing a pricer called:" << n;
        throw;
    } catch ( ExecutorNotFound const& ) {
        // Oh dear - missing a plugin?
        cwarn << "Couldn't create a precompiled contract account. Missing an executor called:" << n;
        throw;
    }
}
}  // namespace

// TODO move AccountMaskObj to libtesteth (it is used only in test logic)
AccountMap dev::eth::jsonToAccountMap( std::string const& _json, u256 const& _defaultNonce,
    AccountMaskMap* o_mask, PrecompiledContractMap* o_precompiled, const fs::path& _configPath ) {
    auto u256Safe = []( std::string const& s ) -> u256 {
        bigint ret( s );
        if ( ret >= bigint( 1 ) << 256 )
            BOOST_THROW_EXCEPTION( ValueTooLarge() << errinfo_comment(
                                       "State value is equal or greater than 2**256" ) );
        return ( u256 ) ret;
    };

    std::unordered_map< Address, Account > ret;

    js::mValue val;
    json_spirit::read_string_or_throw( _json, val );

    for ( auto const& account : val.get_obj() ) {
        Address a( fromHex( account.first ) );
        auto const& accountMaskJson = account.second.get_obj();

        bool haveBalance = ( accountMaskJson.count( c_wei ) || accountMaskJson.count( c_finney ) ||
                             accountMaskJson.count( c_balance ) );
        bool haveNonce = accountMaskJson.count( c_nonce );
        bool haveCode = accountMaskJson.count( c_code ) || accountMaskJson.count( c_codeFromFile );
        bool haveStorage = accountMaskJson.count( c_storage );
        bool shouldNotExists = accountMaskJson.count( c_shouldnotexist );

        if ( haveStorage || haveCode || haveNonce || haveBalance ) {
            u256 balance = 0;
            if ( accountMaskJson.count( c_wei ) )
                balance = u256Safe( accountMaskJson.at( c_wei ).get_str() );
            else if ( accountMaskJson.count( c_finney ) )
                balance = u256Safe( accountMaskJson.at( c_finney ).get_str() ) * finney;
            else if ( accountMaskJson.count( c_balance ) )
                balance = u256Safe( accountMaskJson.at( c_balance ).get_str() );

            u256 nonce =
                haveNonce ? u256Safe( accountMaskJson.at( c_nonce ).get_str() ) : _defaultNonce;

            ret[a] = Account( nonce, balance );
            auto codeIt = accountMaskJson.find( c_code );
            if ( codeIt != accountMaskJson.end() ) {
                auto& codeObj = codeIt->second;
                if ( codeObj.type() == json_spirit::str_type ) {
                    auto& codeStr = codeObj.get_str();
                    if ( codeStr.find( "0x" ) != 0 && !codeStr.empty() ) {
                        cerror << "Error importing code of account " << a
                               << "! Code needs to be hex bytecode prefixed by \"0x\".";
                        cerror << DETAILED_ERROR;
                    } else
                        ret[a].setCode( fromHex( codeStr ), 0 );
                } else {
                    cerror << "Error importing code of account " << a
                           << "! Code field needs to be a string";
                    cerror << DETAILED_ERROR;
                }
            }

            auto codePathIt = accountMaskJson.find( c_codeFromFile );
            if ( codePathIt != accountMaskJson.end() ) {
                auto& codePathObj = codePathIt->second;
                if ( codePathObj.type() == json_spirit::str_type ) {
                    fs::path codePath{ codePathObj.get_str() };
                    if ( codePath.is_relative() )  // Append config dir if code file path is
                                                   // relative.
                        codePath = _configPath.parent_path() / codePath;
                    bytes code = contents( codePath );
                    if ( code.empty() ) {
                        cerror << "Error importing code of account " << a << "! Code file "
                               << codePath << " empty or does not exist.\n";
                        cerror << DETAILED_ERROR;
                    }
                    ret[a].setCode( std::move( code ), 0 );
                } else {
                    cerror << "Error importing code of account " << a
                           << "! Code file path must be a string\n";
                    cerror << DETAILED_ERROR;
                }
            }

            if ( haveStorage )
                for ( pair< string, js::mValue > const& j :
                    accountMaskJson.at( c_storage ).get_obj() )
                    ret[a].setStorage( u256( j.first ), u256( j.second.get_str() ) );
        }

        if ( o_mask ) {
            ( *o_mask )[a] =
                AccountMask( haveBalance, haveNonce, haveCode, haveStorage, shouldNotExists );
            if ( !haveStorage && !haveCode && !haveNonce && !haveBalance &&
                 shouldNotExists )  // defined only shouldNotExists field
                ret[a] = Account( 0, 0 );
        }

        if ( o_precompiled && accountMaskJson.count( c_precompiled ) ) {
            js::mObject p = accountMaskJson.at( c_precompiled ).get_obj();
            o_precompiled->insert( make_pair( a, createPrecompiledContract( p ) ) );
        }
    }

    return ret;
}
