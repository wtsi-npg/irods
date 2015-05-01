#ifndef ZONE_REPORT_HPP
#define ZONE_REPORT_HPP

// =-=-=-=-=-=-=-
// irods includes
#include "rods.h"
#include "rcMisc.h"
#include "procApiRequest.h"
#include "apiNumber.h"

#define ZONE_REPORT_AN 10205

#ifdef RODS_SERVER
#define RS_ZONE_REPORT rsZoneReport
int rsZoneReport(
    rsComm_t*,      // server comm ptr
    bytesBuf_t** ); // json response
#else
#define RS_ZONE_REPORT NULL
#endif

#ifdef __cplusplus
extern "C" {
#endif
// =-=-=-=-=-=-=-
// prototype for client
int rcZoneReport(
    rcComm_t*,      // server comm ptr
    bytesBuf_t** ); // json response
#ifdef __cplusplus
}
#endif



#endif //  ZONE_REPORT_HPP




