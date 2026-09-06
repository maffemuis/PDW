from pathlib import Path
import runpy

_original_read_text = Path.read_text
_original_write_text = Path.write_text


def compatible_read_text(self, *args, **kwargs):
    try:
        return _original_read_text(self, *args, **kwargs)
    except UnicodeDecodeError:
        retry = dict(kwargs)
        retry["encoding"] = "cp1252"
        return _original_read_text(self, *args, **retry)


def compatible_write_text(self, data, *args, **kwargs):
    options = dict(kwargs)
    # PDW.cpp is still a legacy Windows-1252 source file. Preserve that byte
    # encoding so localization changes do not rewrite unrelated historical text.
    if self.name.lower() == "pdw.cpp":
        options["encoding"] = "cp1252"
    return _original_write_text(self, data, *args, **options)


Path.read_text = compatible_read_text
Path.write_text = compatible_write_text
runpy.run_path(".github/scripts/apply_main_ui_stability.py", run_name="__main__")
