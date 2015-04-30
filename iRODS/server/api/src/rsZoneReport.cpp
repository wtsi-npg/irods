#include "rsGlobalExtern.hpp"
#include "rodsErrorTable.h"
#include "miscServerFunct.hpp"

#include "irods_log.hpp"
#include "zone_report.hpp"
#include "server_report.hpp"
#include "irods_resource_manager.hpp"
#include "irods_resource_backport.hpp"

#include "jansson.h"



int _rsZoneReport(
    rsComm_t*    _comm,
    bytesBuf_t** _bbuf );

int rsZoneReport(
    rsComm_t*    _comm,
    bytesBuf_t** _bbuf ) {
    rodsServerHost_t* rods_host;
    int status = getAndConnRcatHost(
                     _comm,
                     MASTER_RCAT,
                     ( const char* )NULL,
                     &rods_host );
    if ( status < 0 ) {
        return status;
    }

    if ( rods_host->localFlag == LOCAL_HOST ) {
#ifdef RODS_CAT
        status = _rsZoneReport(
                     _comm,
                     _bbuf );
#else
        status = SYS_NO_RCAT_SERVER_ERR;
#endif
    }
    else {
        status = rcZoneReport( rods_host->conn,
                               _bbuf );
    }

    if ( status < 0 ) {
        rodsLog(
            LOG_ERROR,
            "rsZoneReport: rcZoneReport failed, status = %d",
            status );
    }

    return status;

} // rsZoneReport


#ifdef RODS_CAT

irods::error get_server_reports(
    rsComm_t* _comm,
    json_t*&  _resc_arr ) {

    _resc_arr = json_array();
    if ( !_resc_arr ) {
        return ERROR(
                   SYS_MALLOC_ERR,
                   "json_object() failed" );
    }

    std::map< rodsServerHost_t*, int > svr_reported;
    rodsServerHost_t* icat_host = 0;
    char* zone_name = getLocalZoneName();
    int status = getRcatHost( MASTER_RCAT, zone_name, &icat_host );
    if( status < 0 ) {
        return ERROR(
                   status,
                   "getRcatHost failed" );
    }

    for ( irods::resource_manager::iterator itr = resc_mgr.begin();
            itr != resc_mgr.end();
            ++itr ) {

        irods::resource_ptr resc = itr->second;

        rodsServerHost_t* tmp_host = 0;
        irods::error ret = resc->get_property< rodsServerHost_t* >(
                               irods::RESOURCE_HOST,
                               tmp_host );
        if ( !ret.ok() ) {
            irods::log( PASS( ret ) );
            continue;
        }

        // skip the icat server as that is done separately
        // also skip null tmp_hosts resources ( coordinating )
        // skip local host
        if( !tmp_host || tmp_host == icat_host || LOCAL_HOST == tmp_host->localFlag ) {
            continue;

        }

        // skip previously reported servers
        std::map< rodsServerHost_t*, int >::iterator svr_itr =
            svr_reported.find( tmp_host );
        if( svr_itr != svr_reported.end() ) {
            continue;

        }

        svr_reported[ tmp_host ] = 1;

        int status = svrToSvrConnect( _comm, tmp_host );
        if ( status < 0 ) {
            return ERROR(
                       status,
                       "failed in svrToSvrConnect" );
        }

        bytesBuf_t* bbuf = 0;
        status = rcServerReport(
                     tmp_host->conn,
                     &bbuf );
        if ( status < 0 ) {
            freeBBuf( bbuf );
            rodsLog(
                LOG_ERROR,
                "rcServerReport failed for [%s], status = %d",
                "",
                status );
        }

        // possible null termination issues
        std::string tmp_str;
        tmp_str.assign( (char*)bbuf->buf, bbuf->len );

        json_error_t j_err;
        json_t* j_resc = json_loads(
                             tmp_str.data(),
                             JSON_REJECT_DUPLICATES,
                             &j_err );

        freeBBuf( bbuf );
        if ( !j_resc ) {
            std::string msg( "json_loads failed [" );
            msg += j_err.text;
            msg += "]";
            irods::log( ERROR(
                            ACTION_FAILED_ERR,
                            msg ) );
            continue;
        }

        json_array_append( _resc_arr, j_resc );

    } // for itr

    return SUCCESS();

} // get_server_reports


int _rsZoneReport(
    rsComm_t*    _comm,
    bytesBuf_t** _bbuf ) {

    bytesBuf_t* bbuf = 0;
    int status = rsServerReport(
                     _comm,
                     &bbuf );
    if ( status < 0 ) {
        freeBBuf( bbuf );
        rodsLog(
            LOG_ERROR,
            "_rsZoneReport - rsServerReport failed %d",
            status );
        return status;
    }

    json_error_t j_err;
    json_t* cat_svr = json_loads( ( char* )bbuf->buf, JSON_REJECT_DUPLICATES, &j_err );
    freeBBuf( bbuf );
    if ( !cat_svr ) {
        rodsLog(
            LOG_ERROR,
            "_rsZoneReport - json_loads failed [%s]",
            j_err.text );
        return ACTION_FAILED_ERR;
    }


    json_t* svr_arr = 0;
    irods::error ret = get_server_reports( _comm, svr_arr );
    if ( !ret.ok() ) {
        rodsLog(
            LOG_ERROR,
            "_rsZoneReport - get_server_reports failed, status = %d",
            ret.code() );
        return ret.code();
    }

    json_t* zone_obj = json_object();
    if ( !zone_obj ) {
        rodsLog(
            LOG_ERROR,
            "failed to allocate json_object" );
        return SYS_MALLOC_ERR;
    }

    json_object_set( zone_obj, "icat_server", cat_svr );
    json_object_set( zone_obj, "resource_servers", svr_arr );

    json_t* zone_arr = json_array();
    if ( !zone_arr ) {
        rodsLog(
            LOG_ERROR,
            "failed to allocate json_array" );
        return SYS_MALLOC_ERR;
    }

    json_array_append( zone_arr, zone_obj );

    json_t* zone = json_object();
    if ( !zone ) {
        rodsLog(
            LOG_ERROR,
            "failed to allocate json_object" );
        return SYS_MALLOC_ERR;
    }

    json_object_set(
        zone,
        "schema_version",
        json_string(
            "https://schemas.irods.org/configuration/v2/zone_bundle.json" ) );
    json_object_set( zone, "zones", zone_arr );

    char* tmp_buf = json_dumps( zone, JSON_INDENT( 4 ) );

    // *SHOULD* free All The Things...
    json_decref( zone );

    ( *_bbuf ) = ( bytesBuf_t* ) malloc( sizeof( bytesBuf_t ) );
    if ( !( *_bbuf ) ) {
        rodsLog(
            LOG_ERROR,
            "_rsZoneReport: failed to allocate _bbuf" );
        return SYS_MALLOC_ERR;

    }

    ( *_bbuf )->buf = tmp_buf;
    ( *_bbuf )->len = strlen( tmp_buf );

    return 0;

} // _rsZoneReport

#endif // RODS_CAT
