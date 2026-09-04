#ifndef PDW_RS232_H
#define PDW_RS232_H

#define CBR_SLICER_2K            CBR_110  // 100
#define CBR_SLICER_XP            CBR_300  // 200

#define RS232_SUCCESS             0
#define RS232_NO_DUT              1
#define RS232_NO_CONNECTION       2
#define RS232_UNKNOWN             3

#define DRIVER_TYPE_NOT_LOADED    0
#define DRIVER_TYPE_SLICER        1
#define DRIVER_TYPE_RS232         2

#define RS232_RAW_LOG_MAX_BYTES   (2UL * 1024UL * 1024UL)

typedef struct {
    BOOL connected;
    BOOL original_rs232;
    BOOL slicer_driver;
    int com_port;
    DWORD baud_rate;
    BYTE byte_size;
    BYTE parity;
    BYTE stop_bits;
    BOOL cts_flow;
    BOOL dsr_flow;
    BOOL xon_xoff_in;
    BOOL xon_xoff_out;
    DWORD rx_bytes;
    DWORD rx_bits;
    DWORD last_rx_tick;
    DWORD read_errors;
    DWORD framing_errors;
    DWORD parity_errors;
    DWORD overrun_errors;
    DWORD rx_queue_bytes;
    DWORD ring_position;
    DWORD ring_wraps;
    BOOL raw_log_enabled;
    DWORD raw_log_bytes;
    BOOL raw_log_limit_reached;
} RS232_DIAGNOSTICS;

EXTERN_C int rs232_connect(const SLICER_IN_STR *pInSlicer, SLICER_OUT_STR *pOutSlicer);
EXTERN_C int rs232_transmit_data(unsigned char buffer[], int nBytes);
EXTERN_C int rs232_get_rx_data(unsigned char buffer[], int nBytes);
EXTERN_C int rs232_disconnect(void);
EXTERN_C int rs232_read(void);
EXTERN_C int slicer_read(void);

EXTERN_C void rs232_get_diagnostics(RS232_DIAGNOSTICS *out);
EXTERN_C int rs232_format_diagnostics(char *buffer, int buffer_len);
EXTERN_C int rs232_raw_log_start(const char *path);
EXTERN_C void rs232_raw_log_stop(void);

EXTERN_C int OpenComPort(void);
EXTERN_C int WriteComPort(char *szLine);
EXTERN_C int CloseComPort(void);

EXTERN_C int *FindComPorts(void);

EXTERN_C int GetRs232DriverType(void);

#endif
