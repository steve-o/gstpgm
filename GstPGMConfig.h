#define VERSION             "5.1.0"
#define PACKAGE             "PGM"

#define PGM_DEFAULT_NETWORK          "eth0;239.255.255.250"
#define PGM_DEFAULT_PORT             7500
#define PGM_DEFAULT_UDP_ENCAP_PORT   8080
#define PGM_DEFAULT_URI              "pgm://" PGM_DEFAULT_NETWORK ":" G_STRINGIFY(PGM_DEFAULT_PORT) ":" G_STRINGIFY(PGM_DEFAULT_UDP_ENCAP_PORT)
#define PGM_DEFAULT_MAX_TPDU         1500
#define PGM_DEFAULT_TXW_SQNS         64
#define PGM_DEFAULT_RXW_SQNS         64
#define PGM_DEFAULT_HOPS             16
#define PGM_DEFAULT_SPM_AMBIENT      ( pgm_secs(30) )
#define PGM_DEFAULT_IHB_MIN          ( pgm_msecs(100) )
#define PGM_DEFAULT_IHB_MAX          ( pgm_secs(30) )
#define PGM_DEFAULT_PEER_EXPIRY      ( pgm_secs(300) )
#define PGM_DEFAULT_SPMR_EXPIRY      ( pgm_msecs(250) )
#define PGM_DEFAULT_NAK_BO_IVL       ( pgm_msecs(50) )
#define PGM_DEFAULT_NAK_RPT_IVL      ( pgm_secs(2) )
#define PGM_DEFAULT_NAK_RDATA_IVL    ( pgm_secs(2) )
#define PGM_DEFAULT_NAK_DATA_RETRIES 5
#define PGM_DEFAULT_NAK_NCF_RETRIES  2

#define GST_PACKAGE_NAME  PACKAGE
