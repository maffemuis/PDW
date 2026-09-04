from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


pdw_path = Path("PDW.cpp")
raw = pdw_path.read_bytes()
pdw = raw.decode("latin-1")

# Mixed Monitor-Only selections must not make Audio completely unusable.
old_audio = '''\t\telse EnableWindow(GetDlgItem(hDlg, IDC_FILTERAUDIO), FALSE); // If Monitor-Only == BST_INDETERMINATE
'''
new_audio = '''\t\telse if (multiple_edit)\n\t\t{\n\t\t\t// Mixed Monitor-Only state: keep Audio usable. Only choices that are\n\t\t\t// unambiguous across both monitor-only and normal filters are offered.\n\t\t\tSendDlgItemMessage(hDlg, IDC_FILTERAUDIO, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR) "Don't change");\n\t\t\tSendDlgItemMessage(hDlg, IDC_FILTERAUDIO, CB_SETCURSEL, (WPARAM) 1, 0L);\n\t\t\tEnableWindow(GetDlgItem(hDlg, IDC_FILTERAUDIO), TRUE);\n\t\t}\n\t\telse EnableWindow(GetDlgItem(hDlg, IDC_FILTERAUDIO), FALSE); // If Monitor-Only == BST_INDETERMINATE\n'''
pdw = replace_once(pdw, old_audio, new_audio, "mixed audio UI")

# If the user explicitly chooses No sound while Monitor-Only remains mixed,
# apply the appropriate legacy no-sound value per selected row.
old_apply = '''\t\t\t\t\tif (IsDlgButtonChecked(hDlg, IDC_FILTER_MONITOR_ONLY) != BST_INDETERMINATE)\n\t\t\t\t\t{\n\t\t\t\t\t\tProfile.filters[index].monitor_only = IsDlgButtonChecked(hDlg, IDC_FILTER_MONITOR_ONLY);\n\n\t\t\t\t\t\tif (SendDlgItemMessage(hDlg, IDC_FILTERAUDIO, CB_GETCURSEL, 0, 0L) != (Profile.filters[index].monitor_only ? 2 : FILTER_SOUND_COUNT + 2))\n\t\t\t\t\t\t{\n\t\t\t\t\t\t\tProfile.filters[index].wave_number = SendDlgItemMessage(hDlg, IDC_FILTERAUDIO, CB_GETCURSEL, 0, 0L);\n\t\t\t\t\t\t\tif (!Profile.filters[index].monitor_only) Profile.filters[index].wave_number -= 1;\n\t\t\t\t\t\t}\n\t\t\t\t\t}\n'''
new_apply = '''\t\t\t\t\tif (IsDlgButtonChecked(hDlg, IDC_FILTER_MONITOR_ONLY) != BST_INDETERMINATE)\n\t\t\t\t\t{\n\t\t\t\t\t\tProfile.filters[index].monitor_only = IsDlgButtonChecked(hDlg, IDC_FILTER_MONITOR_ONLY);\n\n\t\t\t\t\t\tif (SendDlgItemMessage(hDlg, IDC_FILTERAUDIO, CB_GETCURSEL, 0, 0L) != (Profile.filters[index].monitor_only ? 2 : FILTER_SOUND_COUNT + 2))\n\t\t\t\t\t\t{\n\t\t\t\t\t\t\tProfile.filters[index].wave_number = SendDlgItemMessage(hDlg, IDC_FILTERAUDIO, CB_GETCURSEL, 0, 0L);\n\t\t\t\t\t\t\tif (!Profile.filters[index].monitor_only) Profile.filters[index].wave_number -= 1;\n\t\t\t\t\t\t}\n\t\t\t\t\t}\n\t\t\t\t\telse if (multiple_edit && SendDlgItemMessage(hDlg, IDC_FILTERAUDIO, CB_GETCURSEL, 0, 0L) == 0)\n\t\t\t\t\t{\n\t\t\t\t\t\t// "No sound" has different legacy numeric values for monitor-only\n\t\t\t\t\t\t// and normal filters; preserve that representation per row.\n\t\t\t\t\t\tProfile.filters[index].wave_number = Profile.filters[index].monitor_only ? 0 : -1;\n\t\t\t\t\t}\n'''
pdw = replace_once(pdw, old_apply, new_apply, "mixed audio apply")

# Make the tri-state convention clearer in multi-edit mode.
pdw = replace_once(
    pdw,
    'ShowWindow(GetDlgItem(hDlg, IDC_DONTCHANGE),  SW_SHOW);\t// Show "don\'t change"',
    'SetDlgItemText(hDlg, IDC_DONTCHANGE, " NOTE : gray means \'Don\'t change\' - click to set");\n\t\t\t\tShowWindow(GetDlgItem(hDlg, IDC_DONTCHANGE),  SW_SHOW);\t// Show multi-edit hint',
    "multi-edit hint",
)

# SSL/TLS toggle: switch only conventional defaults. Preserve custom ports.
old_ssl_case = '''\t\t\tcase IDC_SMTP_MESSAGE :\n\t\t\tcase IDC_SMTP_BITRATE :\n\t\t\tcase IDC_SMTP_SSL\t  :\n\t\t\t\tProfile.nMailOptions = GetMailOptions(hDlg) ;\n'''
new_ssl_case = '''\t\t\tcase IDC_SMTP_MESSAGE :\n\t\t\tcase IDC_SMTP_BITRATE :\n\t\t\t\tProfile.nMailOptions = GetMailOptions(hDlg) ;\n'''
pdw = replace_once(pdw, old_ssl_case, new_ssl_case, "remove SSL from generic option group")

anchor = '''\t\t\tcase IDC_SMTP_SETTING :\n\t\t\t\tnOldOptions = GetMailOptions(hDlg) ;\n\t\t\t\tbreak ;\n'''
insert = '''\t\t\tcase IDC_SMTP_SETTING :\n\t\t\t\tnOldOptions = GetMailOptions(hDlg) ;\n\t\t\t\tbreak ;\n\t\t\tcase IDC_SMTP_SSL:\n\t\t\t{\n\t\t\t\tconst int current_port = GetDlgItemInt(hDlg, IDC_SMTP_PORT, NULL, FALSE);\n\t\t\t\tconst bool tls_enabled = IsDlgButtonChecked(hDlg, IDC_SMTP_SSL) == BST_CHECKED;\n\t\t\t\tif (tls_enabled && current_port == 25)\n\t\t\t\t{\n\t\t\t\t\tSetDlgItemInt(hDlg, IDC_SMTP_PORT, 465, FALSE);\n\t\t\t\t}\n\t\t\t\telse if (!tls_enabled && current_port == 465)\n\t\t\t\t{\n\t\t\t\t\tSetDlgItemInt(hDlg, IDC_SMTP_PORT, 25, FALSE);\n\t\t\t\t}\n\t\t\t\tProfile.nMailOptions = GetMailOptions(hDlg);\n\t\t\t\tnOldOptions = Profile.nMailOptions;\n\t\t\t\tbreak;\n\t\t\t}\n'''
pdw = replace_once(pdw, anchor, insert, "SSL/TLS port toggle")

if 'case IDC_SMTP_SSL:\n' not in pdw or 'current_port == 25' not in pdw:
    raise SystemExit("SSL/TLS port toggle was not installed")
if 'else if (multiple_edit && SendDlgItemMessage(hDlg, IDC_FILTERAUDIO' not in pdw:
    raise SystemExit("mixed-audio apply path was not installed")

pdw_path.write_bytes(pdw.encode("latin-1"))

rc_path = Path("Rsrc.rc")
rc = rc_path.read_bytes().decode("latin-1")
rc = replace_once(
    rc,
    '    CONTROL         "SSL",IDC_SMTP_SSL,"Button",BS_AUTOCHECKBOX | WS_TABSTOP,253,23,25,10',
    '    CONTROL         "SSL/TLS",IDC_SMTP_SSL,"Button",BS_AUTOCHECKBOX | WS_TABSTOP,253,23,43,10',
    "SSL/TLS label",
)
if 'CONTROL         "SSL/TLS",IDC_SMTP_SSL' not in rc:
    raise SystemExit("SSL/TLS label was not installed")
rc_path.write_bytes(rc.encode("latin-1"))

print("Applied reported UI fixes successfully")
