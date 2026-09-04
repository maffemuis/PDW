from pathlib import Path


def read(path):
    return Path(path).read_text(encoding="cp1252")


def write(path, text):
    Path(path).write_text(text, encoding="cp1252")


def once(text, old, new, label):
    n = text.count(old)
    if n != 1:
        raise SystemExit(f"{label}: expected one anchor, got {n}")
    return text.replace(old, new, 1)


p = "Headers/Resource.h"
s = read(p)
s = once(
    s,
    "#define IDC_DEBUG_TEST            1181\n",
    "#define IDC_DEBUG_TEST            1181\n"
    "#define IDC_DEBUG_RXDETAIL        1182\n"
    "#define IDC_DEBUG_RAW_START       1183\n"
    "#define IDC_DEBUG_RAW_STOP        1184\n"
    "#define IDC_DEBUG_COPY            1185\n",
    p,
)
write(p, s)

p = "Rsrc.rc"
s = read(p)
a = s.index("DEBUGDLGBOX DIALOG")
b = s.index("PRCANCEL DIALOG", a)
block = s[a:b]
block = once(block, "DEBUGDLGBOX DIALOG 10, 10, 221, 86", "DEBUGDLGBOX DIALOG 10, 10, 360, 190", "debug dialog size")
block = once(block, 'GROUPBOX        "",IDC_STATIC,2,0,217,84', 'GROUPBOX        "",IDC_STATIC,2,0,356,188', "debug group size")
block = once(
    block,
    '    LTEXT           ".....",IDC_DEBUG_TEST,38,70,67,8,NOT WS_VISIBLE\nEND\n\n',
    '    LTEXT           ".....",IDC_DEBUG_TEST,38,70,67,8,NOT WS_VISIBLE\n'
    '    LTEXT           "Serial / RX diagnostics (Release-build):",IDC_STATIC,8,88,180,8\n'
    '    EDITTEXT        IDC_DEBUG_RXDETAIL,8,99,344,62,ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL\n'
    '    PUSHBUTTON      "Start Raw RX Log",IDC_DEBUG_RAW_START,8,168,78,14\n'
    '    PUSHBUTTON      "Stop Raw RX Log",IDC_DEBUG_RAW_STOP,92,168,78,14\n'
    '    PUSHBUTTON      "Copy diagnostics",IDC_DEBUG_COPY,176,168,78,14\n'
    'END\n\n',
    "debug controls",
)
s = s[:a] + block + s[b:]
write(p, s)

p = "utils/rs232.cpp"
s = read(p)
s = once(s, "static DWORD g_raw_log_start_tick = 0;\n", "static DWORD g_raw_log_start_tick = 0;\nstatic DWORD g_diag_open_tick = 0;\n", "diag open tick")
s = once(s, "    g_rs232_diag.ring_wraps = 0;\n}\n", "    g_rs232_diag.ring_wraps = 0;\n    g_diag_open_tick = GetTickCount();\n}\n", "diag runtime reset")
old = '''int rs232_format_diagnostics(char *buffer, int buffer_len)
{
    RS232_DIAGNOSTICS d;
    DWORD now;
    DWORD age;
    if(!buffer || buffer_len <= 0) return 0;
    rs232_get_diagnostics(&d);
    now = GetTickCount();
    age = d.last_rx_tick ? now - d.last_rx_tick : 0;
    return _snprintf(buffer, buffer_len - 1,
        "Serial/RS232: %s COM%d\\r\\n"
        "DCB: %lu baud, %u data, parity=%u, stop=%u, CTS=%s, DSR=%s, XON-in=%s, XON-out=%s\\r\\n"
        "RX: bytes=%lu bits=%lu last=%s%lu ms queue=%lu ring=%lu wraps=%lu\\r\\n"
        "Errors: read=%lu framing=%lu parity=%lu overrun=%lu\\r\\n"
        "Raw RX log: %s bytes=%lu limit=%s",
        d.connected ? "OPEN" : "CLOSED", d.com_port,
        d.baud_rate, d.byte_size, d.parity, d.stop_bits,
        d.cts_flow ? "on" : "off", d.dsr_flow ? "on" : "off",
        d.xon_xoff_in ? "on" : "off", d.xon_xoff_out ? "on" : "off",
        d.rx_bytes, d.rx_bits, d.last_rx_tick ? "" : "never/", age,
        d.rx_queue_bytes, d.ring_position, d.ring_wraps,
        d.read_errors, d.framing_errors, d.parity_errors, d.overrun_errors,
        d.raw_log_enabled ? "ON" : "OFF", d.raw_log_bytes,
        d.raw_log_limit_reached ? "REACHED" : "no");
}
'''
new = '''int rs232_format_diagnostics(char *buffer, int buffer_len)
{
    RS232_DIAGNOSTICS d;
    DWORD now;
    DWORD age;
    DWORD elapsed;
    DWORD bytes_per_sec;
    int rc;
    if(!buffer || buffer_len <= 0) return 0;
    rs232_get_diagnostics(&d);
    now = GetTickCount();
    age = d.last_rx_tick ? now - d.last_rx_tick : 0;
    elapsed = (d.connected && g_diag_open_tick) ? now - g_diag_open_tick : 0;
    bytes_per_sec = elapsed ? (DWORD)(((ULONGLONG)d.rx_bytes * 1000ULL) / elapsed) : 0;
    rc = _snprintf(buffer, buffer_len - 1,
        "Serial/RS232: %s COM%d\\r\\n"
        "DCB: %lu baud, %u data, parity=%u, stop=%u, CTS=%s, DSR=%s, XON-in=%s, XON-out=%s\\r\\n"
        "RX: bytes=%lu rate=%lu B/s bits=%lu last=%s%lu ms queue=%lu ring=%lu wraps=%lu\\r\\n"
        "Errors: read=%lu framing=%lu parity=%lu overrun=%lu\\r\\n"
        "Raw RX log: %s bytes=%lu/%lu limit=%s",
        d.connected ? "OPEN" : "CLOSED", d.com_port,
        d.baud_rate, d.byte_size, d.parity, d.stop_bits,
        d.cts_flow ? "on" : "off", d.dsr_flow ? "on" : "off",
        d.xon_xoff_in ? "on" : "off", d.xon_xoff_out ? "on" : "off",
        d.rx_bytes, bytes_per_sec, d.rx_bits, d.last_rx_tick ? "" : "never/", age,
        d.rx_queue_bytes, d.ring_position, d.ring_wraps,
        d.read_errors, d.framing_errors, d.parity_errors, d.overrun_errors,
        d.raw_log_enabled ? "ON" : "OFF", d.raw_log_bytes, (DWORD)RS232_RAW_LOG_MAX_BYTES,
        d.raw_log_limit_reached ? "REACHED" : "no");
    buffer[buffer_len - 1] = 0;
    return rc;
}
'''
s = once(s, old, new, "rs232 formatter")
write(p, s)

p = "PDW.cpp"
s = read(p)
a = s.index("BOOL FAR PASCAL DebugDlgProc")
b = s.index("} // end of DebugDlgProc", a) + len("} // end of DebugDlgProc")
block = s[a:b]
block = once(block, '\tchar temp[32];\n', '\tchar temp[32];\n\tchar rxdiag[2048];\n', "debug buffer")
block = once(
    block,
    '\t\tSetDlgItemText(hDlg, IDC_DEBUG_BLOCKBUFFER, szTEMP);\n\n\t\treturn (TRUE);\n',
    '\t\tSetDlgItemText(hDlg, IDC_DEBUG_BLOCKBUFFER, szTEMP);\n\n'
    '\t\tif (Profile.comPortEnabled && Profile.comPortRS232)\n'
    '\t\t{\n'
    '\t\t\tRS232_DIAGNOSTICS d;\n'
    '\t\t\trs232_get_diagnostics(&d);\n'
    '\t\t\trs232_format_diagnostics(rxdiag, sizeof(rxdiag));\n'
    '\t\t\tSetDlgItemText(hDlg, IDC_DEBUG_RXDETAIL, rxdiag);\n'
    '\t\t\tEnableWindow(GetDlgItem(hDlg, IDC_DEBUG_RAW_START), d.connected && !d.raw_log_enabled);\n'
    '\t\t\tEnableWindow(GetDlgItem(hDlg, IDC_DEBUG_RAW_STOP), d.raw_log_enabled);\n'
    '\t\t}\n'
    '\t\telse\n'
    '\t\t{\n'
    '\t\t\tSetDlgItemText(hDlg, IDC_DEBUG_RXDETAIL, "Serial/RS232 diagnostics are inactive (input is not RS232).");\n'
    '\t\t\tEnableWindow(GetDlgItem(hDlg, IDC_DEBUG_RAW_START), FALSE);\n'
    '\t\t\tEnableWindow(GetDlgItem(hDlg, IDC_DEBUG_RAW_STOP), FALSE);\n'
    '\t\t}\n\n'
    '\t\treturn (TRUE);\n',
    "debug refresh",
)
block = once(
    block,
    '\t\t\tcase IDCANCEL:\n',
    '\t\t\tcase IDC_DEBUG_RAW_START:\n'
    '\t\t\t{\n'
    '\t\t\t\tRS232_DIAGNOSTICS d;\n'
    '\t\t\t\trs232_get_diagnostics(&d);\n'
    '\t\t\t\tif (!Profile.comPortEnabled || !Profile.comPortRS232 || !d.connected)\n'
    '\t\t\t\t{\n'
    '\t\t\t\t\tMessageBox(hDlg, "Raw RX logging requires an open explicit RS232 input. No logging was started.", "PDW RX diagnostics", MB_OK | MB_ICONINFORMATION);\n'
    '\t\t\t\t}\n'
    '\t\t\t\telse if (!rs232_raw_log_start("pdw-rx-raw.log"))\n'
    '\t\t\t\t{\n'
    '\t\t\t\t\tMessageBox(hDlg, "Unable to create pdw-rx-raw.log. No logging was started.", "PDW RX diagnostics", MB_OK | MB_ICONERROR);\n'
    '\t\t\t\t}\n'
    '\t\t\t\tSendMessage(hDlg, WM_WININICHANGE, 0, 0L);\n'
    '\t\t\t\tbreak;\n'
    '\t\t\t}\n\n'
    '\t\t\tcase IDC_DEBUG_RAW_STOP:\n'
    '\t\t\t\trs232_raw_log_stop();\n'
    '\t\t\t\tSendMessage(hDlg, WM_WININICHANGE, 0, 0L);\n'
    '\t\t\t\tbreak;\n\n'
    '\t\t\tcase IDC_DEBUG_COPY:\n'
    '\t\t\t{\n'
    '\t\t\t\tHGLOBAL hMem = NULL;\n'
    '\t\t\t\tif (Profile.comPortEnabled && Profile.comPortRS232) rs232_format_diagnostics(rxdiag, sizeof(rxdiag));\n'
    '\t\t\t\telse strcpy(rxdiag, "Serial/RS232 diagnostics are inactive (input is not RS232).");\n'
    '\t\t\t\tif (OpenClipboard(hDlg))\n'
    '\t\t\t\t{\n'
    '\t\t\t\t\tEmptyClipboard();\n'
    '\t\t\t\t\thMem = GlobalAlloc(GMEM_MOVEABLE, strlen(rxdiag) + 1);\n'
    '\t\t\t\t\tif (hMem)\n'
    '\t\t\t\t\t{\n'
    '\t\t\t\t\t\tchar *clip = (char *)GlobalLock(hMem);\n'
    '\t\t\t\t\t\tif (clip)\n'
    '\t\t\t\t\t\t{\n'
    '\t\t\t\t\t\t\tstrcpy(clip, rxdiag);\n'
    '\t\t\t\t\t\t\tGlobalUnlock(hMem);\n'
    '\t\t\t\t\t\t\tif (SetClipboardData(CF_TEXT, hMem)) hMem = NULL;\n'
    '\t\t\t\t\t\t}\n'
    '\t\t\t\t\t}\n'
    '\t\t\t\t\tCloseClipboard();\n'
    '\t\t\t\t}\n'
    '\t\t\t\tif (hMem) GlobalFree(hMem);\n'
    '\t\t\t\tbreak;\n'
    '\t\t\t}\n\n'
    '\t\t\tcase IDCANCEL:\n',
    "debug commands",
)
s = s[:a] + block + s[b:]
write(p, s)

print("bounded RX diagnostics patch applied")
