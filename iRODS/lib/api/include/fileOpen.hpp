/*** Copyright (c), The Regents of the University of California            ***
 *** For more information please refer to files in the COPYRIGHT directory ***/
/* fileOpen.h - This file may be generated by a program or script
 */

#ifndef FILE_OPEN_HPP
#define FILE_OPEN_HPP

/* This is a low level file type API call */

#include "rodsConnect.h"
#include "rcMisc.hpp"
#include "procApiRequest.h"
#include "apiNumber.h"

/* definition for otherFlags */

#define NO_CHK_PERM_FLAG                0x1 // JMC - backport 4758
#define UNIQUE_REM_COMM_FLAG    0x2
#define FORCE_FLAG                      0x4

typedef struct {

    char resc_name_[MAX_NAME_LEN];
    char resc_hier_[MAX_NAME_LEN];
    char objPath[MAX_NAME_LEN];

    int otherFlags;     /* for chkPerm, uniqueRemoteConn */
    rodsHostAddr_t addr;
    char fileName[MAX_NAME_LEN];
    int flags;
    int mode;
    rodsLong_t dataSize;
    keyValPair_t condInput;
    char in_pdmo[MAX_NAME_LEN];
} fileOpenInp_t;

#define fileOpenInp_PI "str resc_name_[MAX_NAME_LEN]; str resc_hier_[MAX_NAME_LEN]; str objPath[MAX_NAME_LEN]; int otherFlags; struct RHostAddr_PI; str fileName[MAX_NAME_LEN]; int flags; int mode; double dataSize; struct KeyValPair_PI; str in_pdmo[MAX_NAME_LEN];"

#if defined(RODS_SERVER)
#define RS_FILE_OPEN rsFileOpen
/* prototype for the server handler */
int
rsFileOpen( rsComm_t *rsComm, fileOpenInp_t *fileOpenInp );
int
rsFileOpenByHost( rsComm_t *rsComm, fileOpenInp_t *fileOpenInp,
                  rodsServerHost_t *rodsServerHost );
int
_rsFileOpen( rsComm_t *rsComm, fileOpenInp_t *fileOpenInp );
int
remoteFileOpen( rsComm_t *rsComm, fileOpenInp_t *fileOpenInp,
                rodsServerHost_t *rodsServerHost );
#else
#define RS_FILE_OPEN NULL
#endif

/* prototype for the client call */
#ifdef __cplusplus
extern "C" {
#endif
int
rcFileOpen( rcComm_t *conn, fileOpenInp_t *fileOpenInp );
#ifdef __cplusplus
}
#endif

#endif  /* FILE_OPEN_H */
