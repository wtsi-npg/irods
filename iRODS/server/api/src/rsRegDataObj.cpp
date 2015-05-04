/*** Copyright (c), The Regents of the University of California            ***
 *** For more information please refer to files in the COPYRIGHT directory ***/
/* regDataObj.c
 */

#include "regDataObj.h"
#include "icatHighLevelRoutines.hpp"
#include "fileDriver.hpp"

#include "irods_file_object.hpp"

/* rsRegDataObj - This call is strictly an API handler and should not be
 * called directly in the server. For server calls, use svrRegDataObj
 */
int
rsRegDataObj( rsComm_t *rsComm, dataObjInfo_t *dataObjInfo,
              dataObjInfo_t **outDataObjInfo ) {
    int status;
    rodsServerHost_t *rodsServerHost = NULL;

    *outDataObjInfo = NULL;

    status = getAndConnRcatHost( rsComm, MASTER_RCAT, ( const char* )dataObjInfo->objPath,
                                 &rodsServerHost );
    if ( status < 0 || NULL == rodsServerHost ) { // JMC cppcheck - nullptr
        return status;
    }

    if ( rodsServerHost->localFlag == LOCAL_HOST ) {
#ifdef RODS_CAT
        status = _rsRegDataObj( rsComm, dataObjInfo );
        if ( status >= 0 ) {
            *outDataObjInfo = ( dataObjInfo_t * ) malloc( sizeof( dataObjInfo_t ) );
            /* fake pointers will be deleted by the packing */
            //**outDataObjInfo = *dataObjInfo;
            memcpy( *outDataObjInfo, dataObjInfo, sizeof( dataObjInfo_t ) );
        }
#else
        status = SYS_NO_RCAT_SERVER_ERR;
#endif
    }
    else {
        status = rcRegDataObj( rodsServerHost->conn, dataObjInfo,
                               outDataObjInfo );
    }
    return status;
}

int
_rsRegDataObj( rsComm_t *rsComm, dataObjInfo_t *dataObjInfo ) {
#ifdef RODS_CAT
    int status;
    irods::error ret;
    status = chlRegDataObj( rsComm, dataObjInfo );
    if ( status < 0 ) {
        char* sys_error;
        char* rods_error = rodsErrorName( status, &sys_error );
        std::stringstream msg;
        msg << __FUNCTION__;
        msg << " - Failed to register data object \"" << dataObjInfo->objPath << "\"";
        msg << " - " << rods_error << " " << sys_error;
        ret = ERROR( status, msg.str() );
        irods::log( ret );
    }
    else {
        irods::file_object_ptr file_obj(
            new irods::file_object(
                rsComm,
                dataObjInfo ) );
        ret = fileRegistered( rsComm, file_obj );
        if ( !ret.ok() ) {
            std::stringstream msg;
            msg << __FUNCTION__;
            msg << " - Failed to signal resource that the data object \"";
            msg << dataObjInfo->objPath;
            msg << "\" was registered";
            ret = PASSMSG( msg.str(), ret );
            irods::log( ret );
            status = ret.code();
        }
    }
    return status;
#else
    return SYS_NO_RCAT_SERVER_ERR;
#endif

}

int
svrRegDataObj( rsComm_t *rsComm, dataObjInfo_t *dataObjInfo ) {
    int status;
    rodsServerHost_t *rodsServerHost = NULL;

    if ( dataObjInfo->specColl != NULL ) {
        rodsLog( LOG_NOTICE,
                 "svrRegDataObj: Reg path %s is in spec coll",
                 dataObjInfo->objPath );
        return SYS_REG_OBJ_IN_SPEC_COLL;
    }

    status = getAndConnRcatHost( rsComm, MASTER_RCAT, ( const char* )dataObjInfo->objPath,
                                 &rodsServerHost );
    if ( status < 0 || NULL == rodsServerHost ) { // JMC cppcheck - nullptr
        return status;
    }

    if ( rodsServerHost->localFlag == LOCAL_HOST ) {
#ifdef RODS_CAT
        status = _rsRegDataObj( rsComm, dataObjInfo );
#else
        status = SYS_NO_RCAT_SERVER_ERR;
#endif
    }
    else {
        dataObjInfo_t *outDataObjInfo = NULL;
        status = rcRegDataObj( rodsServerHost->conn, dataObjInfo,
                               &outDataObjInfo );
        if ( status >= 0 && NULL != outDataObjInfo ) { // JMC cppcheck - nullptr
            dataObjInfo->dataId = outDataObjInfo->dataId;
            clearKeyVal( &outDataObjInfo->condInput );
            free( outDataObjInfo );
        }
    }

    return status;
}

