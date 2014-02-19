
#define VERSION			"0.0.1"
#define PACKAGE_NAME		"gstpgm"
#define PACKAGE			PACKAGE_NAME

#define PGM_NETWORK             ";239.192.0.1"
#define PGM_PORT                7500
#define PGM_UDP_ENCAP_PORT	3056
#define PGM_URI			"pgm://"PGM_NETWORK":"G_STRINGIFY(PGM_PORT)":"G_STRINGIFY(PGM_UDP_ENCAP_PORT)
#define PGM_MAX_TPDU            1500
#define PGM_TXW_SQNS            32
#define PGM_RXW_SQNS            32
#define PGM_HOPS                16
#define PGM_SPM_AMBIENT         ( pgm_secs(30) )
#define PGM_IHB_MIN		( pgm_msecs(100) )
#define PGM_IHB_MAX		( pgm_secs(30) )
#define PGM_PEER_EXPIRY         ( pgm_secs(300) )
#define PGM_SPMR_EXPIRY         ( pgm_msecs(250) )
#define PGM_NAK_BO_IVL          ( pgm_msecs(50) )
#define PGM_NAK_RPT_IVL         ( pgm_secs(2) )
#define PGM_NAK_RDATA_IVL       ( pgm_secs(2) )
#define PGM_NAK_DATA_RETRIES    5
#define PGM_NAK_NCF_RETRIES     2

