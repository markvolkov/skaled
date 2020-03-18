#if ( !defined __SKUTILS_STATS_H )
#define __SKUTILS_STATS_H 1

#include <atomic>
#include <map>
#include <mutex>
#include <string>

#include <skutils/atomic_shared_ptr.h>
#include <skutils/multithreading.h>
#include <skutils/stats.h>

#include <inttypes.h>

#include <json.hpp>

namespace skutils {
namespace task {
namespace performance {

typedef std::chrono::system_clock clock;
// typedef std::chrono::high_resolution_clock clock;
typedef std::chrono::time_point< clock > time_point;

typedef size_t index_type;
typedef std::atomic_size_t atomic_index_type;

typedef std::atomic_bool atomic_bool;

typedef std::string string;

typedef nlohmann::json json;

typedef skutils::multithreading::mutex_type mutex_type;
typedef skutils::multithreading::recursive_mutex_type recursive_mutex_type;

class item;
class queue;
class tracker;
class action;

typedef skutils::retain_release_ptr< item > item_ptr;
typedef skutils::retain_release_ptr< queue > queue_ptr;
typedef skutils::retain_release_ptr< tracker > tracker_ptr;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class lockable {
public:
    typedef recursive_mutex_type mutex_type;
    typedef std::lock_guard< mutex_type > lock_type;

private:
    mutable mutex_type mtx_;

public:
    lockable();
    lockable( const lockable& ) = delete;
    lockable( lockable&& ) = delete;
    virtual ~lockable();
    lockable& operator=( const lockable& ) = delete;
    lockable& operator=( lockable&& ) = delete;
    mutex_type& mtx() const;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class describable {
    const string strName_;
    const json json_;  // attached custom data
public:
    describable( const string& strName, const json& jsn );
    describable( const describable& ) = delete;
    describable( describable&& ) = delete;
    virtual ~describable();
    describable& operator=( const describable& ) = delete;
    describable& operator=( describable&& ) = delete;
    const string& get_name() const;
    const json& get_json() const;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class index_holder {
    atomic_index_type nextItemIndex_;

public:
    index_holder();
    index_holder( const index_holder& ) = delete;
    index_holder( index_holder&& ) = delete;
    virtual ~index_holder();
    index_holder& operator=( const index_holder& ) = delete;
    index_holder& operator=( index_holder&& ) = delete;
    index_type alloc_index();
    void reset();
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class item : public skutils::ref_retain_release, public describable {
    mutable queue_ptr pQueue_;
    index_type indexQ_, indexT_;
    time_point tpStart_, tpEnd_;
    atomic_bool isFinished_;

public:
    item( const string& strName, const json& jsn, queue_ptr pQueue, index_type indexQ,
        index_type indexT );
    item( const item& ) = delete;
    item( item&& ) = delete;
    virtual ~item();
    item& operator=( const item& ) = delete;
    item& operator=( item&& ) = delete;
    queue_ptr get_queue() const;
    tracker_ptr get_tracker() const;
    index_type get_index_in_queue() const;
    index_type get_index_in_tracker() const;
    json compose_json() const;
    void finish();
    bool is_funished() const;
    time_point tp_start() const;
    time_point tp_end() const;
    string tp_start_s( bool isUTC = true, bool isDaysInsteadOfYMD = false ) const;
    string tp_end_s( bool isUTC = true, bool isDaysInsteadOfYMD = false ) const;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class queue : public skutils::ref_retain_release,
              public describable,
              public lockable,
              public index_holder {
    mutable tracker_ptr pTracker_;

    typedef std::map< index_type, item_ptr > map_type;
    mutable map_type map_;

public:
    queue( const string& strName, const json& jsn, tracker_ptr pTracker );
    queue( const queue& ) = delete;
    queue( queue&& ) = delete;
    virtual ~queue();
    queue& operator=( const queue& ) = delete;
    queue& operator=( queue&& ) = delete;

private:
    void reset();

public:
    tracker_ptr get_tracker() const;
    item_ptr new_item( const string& strName, const json& jsn );
    json compose_json( index_type minIndexT = 0 ) const;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class tracker : public skutils::ref_retain_release, public lockable, public index_holder {
    atomic_bool enabled_;

    typedef std::map< string, queue_ptr > map_type;
    mutable map_type map_;

public:
    tracker();
    tracker( const tracker& ) = delete;
    tracker( tracker&& ) = delete;
    virtual ~tracker();
    tracker& operator=( const tracker& ) = delete;
    tracker& operator=( tracker&& ) = delete;

private:
    void reset();

public:
    bool is_enabled() const;
    void set_enabled( bool b = true );
    queue_ptr get_queue( const string& strName );
    json compose_json( index_type minIndexT = 0 ) const;
    void start();
    json stop( index_type minIndexT = 0 );
};

extern tracker_ptr get_default_tracker();

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class action {
    item_ptr pItem_;

public:
    action( const string& strQueueName, const string& strActionName, const json& jsnAction,
        tracker_ptr pTracker );
    action( const string& strQueueName, const string& strActionName, const json& jsnAction );
    action( const string& strQueueName, const string& strActionName );
    action( const action& ) = delete;
    action( action&& ) = delete;
    virtual ~action();
    action& operator=( const action& ) = delete;
    action& operator=( action&& ) = delete;

private:
    void init( const string& strQueueName, const string& strActionName, const json& jsnAction,
        tracker_ptr pTracker );

public:
    item_ptr get_item() const;
    queue_ptr get_queue() const;
    tracker_ptr get_tracker() const;
    void finish();
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

};  // namespace performance
};  // namespace task
};  // namespace skutils

#endif  /// (!defined __SKUTILS_STATS_H)
