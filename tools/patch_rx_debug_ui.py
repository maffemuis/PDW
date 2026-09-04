from pathlib import Path


def patch(path, old, new, count=1):
    p = Path(path)
    data = p.read_bytes().decode("latin-1")
    actual = data.count(old)
    if actual != count:
        raise SystemExit(f"{path}: expected {count} matches, got {actual}")
    data = data.replace(old, new, count)
    p.write_bytes(data.encode("latin-1"))


patch(
    "PDW.cpp",
    '#include "utils\\rs232.h"\n#include "utils\\debug.h"\n',
    '#include "utils\\rs232.h"\n#include "utils\\debug.h"\n#include "utils\\rx_diagnostics.h"\n',
)

patch(
    "PDW.cpp",
    'char *pdw_version = "PDW v3.2b01";\t\t// Current version info\n',
    'char *pdw_version = "PDW v3.3.0-alpha1";\t// Current version info\n'
    'char *pdw_title = "PDW v3.3.0-alpha1 - Powered by Nick ECO AI";\n',
)

patch(
    "PDW.cpp",
    '\tghWnd = NULL;\n\tghInstance = hInstance;\n\n\tProfile.LabelLog',
    '\tghWnd = NULL;\n\tghInstance = hInstance;\n\tRxDiagnosticsInit();\n\tRxDiagnosticsReset();\n\n\tProfile.LabelLog',
)

patch(
    "PDW.cpp",
    '\tsprintf(szWindowText[0], " %s", pdw_version);\t// PH: Set version info in szWindowText buffer\n',
    '\tsprintf(szWindowText[0], " %s", pdw_title);\t// Set visible modernization build identity\n',
)

patch(
    "PDW.cpp",
    '\t\tKillTimer(ghWnd, SECOND_TIMER);\n\n\t\tif (pLogFile)',
    '\t\tKillTimer(ghWnd, SECOND_TIMER);\n\t\tRxDiagnosticsStopRawLog();\n\n\t\tif (pLogFile)',
)

patch(
    "PDW.cpp",
    '\tchar rxqual[10]="- %";\n\tchar temp[32];\n',
    '\tchar rxqual[10]="- %";\n\tchar temp[32];\n\tchar rxText[4096];\n\tRX_DIAG_SNAPSHOT rxSnapshot;\n',
)

patch(
    "PDW.cpp",
    '\t\tSetDlgItemText(hDlg, IDC_DEBUG_BLOCKBUFFER, szTEMP);\n\n\t\treturn (TRUE);',
    '\t\tSetDlgItemText(hDlg, IDC_DEBUG_BLOCKBUFFER, szTEMP);\n\n'
    '\t\tRxDiagnosticsGetSnapshot(&rxSnapshot);\n'
    '\t\tRxDiagnosticsFormatSnapshot(&rxSnapshot, rxText, sizeof(rxText));\n'
    '\t\tSetDlgItemText(hDlg, IDC_DEBUG_RXTEXT, rxText);\n'
    '\t\tEnableWindow(GetDlgItem(hDlg, IDC_DEBUG_RAW_START), rxSnapshot.raw_log_enabled ? FALSE : TRUE);\n'
    '\t\tEnableWindow(GetDlgItem(hDlg, IDC_DEBUG_RAW_STOP),  rxSnapshot.raw_log_enabled ? TRUE : FALSE);\n\n'
    '\t\treturn (TRUE);',
)

patch(
    "PDW.cpp",
    '\t\tswitch (LOWORD(wParam))\n\t\t{\n\t\t\tcase IDCANCEL:\n\n\t\t\tEndDialog(hDlg, TRUE);',
    '\t\tswitch (LOWORD(wParam))\n\t\t{\n'
    '\t\t\tcase IDC_DEBUG_RAW_START:\n'
    '\t\t\t\tif (!RxDiagnosticsStartRawLog(szPath, RX_DIAG_DEFAULT_RAW_LIMIT))\n'
    '\t\t\t\t{\n'
    '\t\t\t\t\tMessageBox(hDlg, "Unable to start the bounded Raw RX log.", "PDW RX Diagnostics", MB_ICONERROR | MB_OK);\n'
    '\t\t\t\t}\n'
    '\t\t\t\tSendMessage(hDlg, WM_WININICHANGE, 0, 0L);\n'
    '\t\t\t\treturn (TRUE);\n\n'
    '\t\t\tcase IDC_DEBUG_RAW_STOP:\n'
    '\t\t\t\tRxDiagnosticsStopRawLog();\n'
    '\t\t\t\tSendMessage(hDlg, WM_WININICHANGE, 0, 0L);\n'
    '\t\t\t\treturn (TRUE);\n\n'
    '\t\t\tcase IDC_DEBUG_COPY:\n'
    '\t\t\t\tif (!RxDiagnosticsCopySnapshotToClipboard(hDlg))\n'
    '\t\t\t\t\tMessageBox(hDlg, "Unable to copy diagnostics to the clipboard.", "PDW RX Diagnostics", MB_ICONERROR | MB_OK);\n'
    '\t\t\t\treturn (TRUE);\n\n'
    '\t\t\tcase IDCANCEL:\n\n'
    '\t\t\tEndDialog(hDlg, TRUE);',
)

patch(
    "Headers/Resource.h",
    '#define IDC_DEBUG_TEST            1181\n',
    '#define IDC_DEBUG_TEST            1181\n'
    '#define IDC_DEBUG_RXTEXT          1182\n'
    '#define IDC_DEBUG_RAW_START       1183\n'
    '#define IDC_DEBUG_RAW_STOP        1184\n'
    '#define IDC_DEBUG_COPY            1185\n',
)

old_debug = '''DEBUGDLGBOX DIALOG 10, 10, 221, 86
STYLE DS_SETFONT | DS_MODALFRAME | DS_3DLOOK | WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU
CAPTION "PDW Debug Information"
FONT 8, "Verdana"
BEGIN
    GROUPBOX        "",IDC_STATIC,2,0,217,84
    LTEXT           "OS :",IDC_STATIC,8,10,16,8
    LTEXT           ".....",IDC_DEBUG_OS,46,10,164,8
    LTEXT           "Started :",IDC_STATIC,8,20,30,8
    LTEXT           ".....",IDC_DEBUG_STARTED,46,20,65,8
    LTEXT           "Running :",IDC_STATIC,8,30,32,8
    LTEXT           ".....",IDC_DEBUG_RUNNING,46,30,57,8
    LTEXT           "Input :",IDC_STATIC,8,40,23,8
    LTEXT           ".....",IDC_DEBUG_INPUT,46,40,48,8
    LTEXT           "FlexTIME :",IDC_STATIC,8,50,38,8
    LTEXT           ".....",IDC_DEBUG_FLEXTIME,46,50,48,8
    LTEXT           "Messages :",IDC_STATIC,116,20,38,8
    LTEXT           ".....",IDC_DEBUG_MSG,183,20,34,8
    LTEXT           "· Rejected :",IDC_STATIC,116,30,39,8
    LTEXT           ".....",IDC_DEBUG_REJECTED,183,30,34,8
    LTEXT           "· Blocked :",IDC_STATIC,116,40,36,8
    LTEXT           ".....",IDC_DEBUG_BLOCKED,183,40,34,8
    LTEXT           "· Buffer (timer) :",IDC_STATIC,120,50,56,8
    LTEXT           ".....",IDC_DEBUG_BLOCKBUFFER,183,50,34,8
    LTEXT           "· Groupcalls :",IDC_STATIC,116,60,45,8
    LTEXT           ".....",IDC_DEBUG_GROUPMSG,183,60,34,8
    LTEXT           "Missed groupcalls :",IDC_STATIC,116,70,63,8
    LTEXT           ".....",IDC_DEBUG_MISSED,183,70,34,8
    LTEXT           "TEST :",IDC_STATIC,8,70,23,8,NOT WS_VISIBLE
    LTEXT           ".....",IDC_DEBUG_TEST,38,70,67,8,NOT WS_VISIBLE
END
'''

new_debug = '''DEBUGDLGBOX DIALOG 10, 10, 430, 240
STYLE DS_SETFONT | DS_MODALFRAME | DS_3DLOOK | WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU
CAPTION "PDW Debug Information"
FONT 8, "Verdana"
BEGIN
    GROUPBOX        "Session",IDC_STATIC,2,0,426,84
    LTEXT           "OS :",IDC_STATIC,8,10,16,8
    LTEXT           ".....",IDC_DEBUG_OS,46,10,164,8
    LTEXT           "Started :",IDC_STATIC,8,20,30,8
    LTEXT           ".....",IDC_DEBUG_STARTED,46,20,65,8
    LTEXT           "Running :",IDC_STATIC,8,30,32,8
    LTEXT           ".....",IDC_DEBUG_RUNNING,46,30,57,8
    LTEXT           "Input :",IDC_STATIC,8,40,23,8
    LTEXT           ".....",IDC_DEBUG_INPUT,46,40,60,8
    LTEXT           "FlexTIME :",IDC_STATIC,8,50,38,8
    LTEXT           ".....",IDC_DEBUG_FLEXTIME,46,50,60,8
    LTEXT           "Messages :",IDC_STATIC,225,20,38,8
    LTEXT           ".....",IDC_DEBUG_MSG,292,20,45,8
    LTEXT           "· Rejected :",IDC_STATIC,225,30,39,8
    LTEXT           ".....",IDC_DEBUG_REJECTED,292,30,45,8
    LTEXT           "· Blocked :",IDC_STATIC,225,40,36,8
    LTEXT           ".....",IDC_DEBUG_BLOCKED,292,40,45,8
    LTEXT           "· Buffer (timer) :",IDC_STATIC,229,50,56,8
    LTEXT           ".....",IDC_DEBUG_BLOCKBUFFER,292,50,45,8
    LTEXT           "· Groupcalls :",IDC_STATIC,225,60,45,8
    LTEXT           ".....",IDC_DEBUG_GROUPMSG,292,60,45,8
    LTEXT           "Missed groupcalls :",IDC_STATIC,225,70,63,8
    LTEXT           ".....",IDC_DEBUG_MISSED,292,70,45,8
    LTEXT           "TEST :",IDC_STATIC,8,70,23,8,NOT WS_VISIBLE
    LTEXT           ".....",IDC_DEBUG_TEST,38,70,67,8,NOT WS_VISIBLE
    GROUPBOX        "RX / COM diagnostics",IDC_STATIC,2,88,426,146
    EDITTEXT        IDC_DEBUG_RXTEXT,8,100,414,102,ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL
    PUSHBUTTON      "Start Raw RX Log",IDC_DEBUG_RAW_START,8,211,82,15
    PUSHBUTTON      "Stop Raw RX Log",IDC_DEBUG_RAW_STOP,98,211,82,15
    PUSHBUTTON      "Copy diagnostics",IDC_DEBUG_COPY,188,211,82,15
    LTEXT           "Raw log is bounded to 16 MiB and stops automatically at the limit.",IDC_STATIC,279,213,143,10
END
'''
patch("Rsrc.rc", old_debug, new_debug)

patch(
    "Rsrc.rc",
    '    LTEXT           "2013 GNU GENERAL PUBLIC LICENSE",IDC_STATIC,124,57,124,8\n',
    '    LTEXT           "2013 GNU GENERAL PUBLIC LICENSE",IDC_STATIC,124,57,124,8\n'
    '    LTEXT           "Powered by Nick ECO AI",IDC_STATIC,121,88,110,8\n',
)

patch(
    "Rsrc.rc",
    ' FILEVERSION 3,1,0,0\n PRODUCTVERSION 3,1,0,0\n',
    ' FILEVERSION 3,3,0,1\n PRODUCTVERSION 3,3,0,1\n',
)
patch(
    "Rsrc.rc",
    '            VALUE "FileVersion", "3.1"\n',
    '            VALUE "FileVersion", "3.3.0-alpha1"\n',
)
patch(
    "Rsrc.rc",
    '            VALUE "OriginalFilename", "PDW3_1.exe"\n',
    '            VALUE "OriginalFilename", "PDW.exe"\n',
)
patch(
    "Rsrc.rc",
    '            VALUE "ProductVersion", "3.1"\n',
    '            VALUE "ProductVersion", "3.3.0-alpha1"\n',
)
patch(
    "Rsrc.rc",
    '    IDS_APPNAME             " PDW v3.2b01 - Windows POCSAG, FLEX, ACARS, MOBITEX & ERMES Decoder"\n',
    '    IDS_APPNAME             " PDW v3.3.0-alpha1 - Powered by Nick ECO AI - Windows POCSAG, FLEX, ACARS, MOBITEX & ERMES Decoder"\n',
)

print("RX_DEBUG_UI_PATCH_READY")
