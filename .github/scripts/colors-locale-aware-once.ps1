$ErrorActionPreference = 'Stop'
$path = 'utils/windows11_ui.cpp'
$text = [System.IO.File]::ReadAllText($path)

function Replace-Exact([string]$old, [string]$new, [string]$label) {
    if (-not $script:text.Contains($old)) { throw "Expected source fragment not found: $label" }
    $script:text = $script:text.Replace($old, $new)
}

$old = @'
void SetColorsDialogText(HWND hwnd)
{
    SetWindowTextW(hwnd, L"Kleuren");
    struct ItemText { int id; const wchar_t* text; };
    const ItemText items[] = {
        { IDC_COLORBACKGND, L"Achtergrond" },
        { IDC_COLORCAPCODE, L"Capcode" },
        { IDC_COLORFLEXPHASE, L"FLEX-fase" },
        { IDC_COLORTIMESTAMP, L"Tijdstempel" },
        { IDC_COLORBITERRORS, L"Bitfouten" },
        { IDC_COLORNUMERIC, L"Numeriek" },
        { IDC_COLORALPHANUM, L"Alfanumeriek" },
        { IDC_COLORFLEXBIN, L"FLEX binair" },
        { IDC_COLORFILTMATCH, L"Filtertreffer" },
        { IDC_COLORFILTERLABEL, L"Filterlabel" },
        { IDC_COLORDEFAULT, L"Standaardkleuren" },
        { IDC_COLORWIN, L"Windows-kleuren" },
        { IDC_COLORINSTRUCTIONS, L"Klik op een kleurvak om de kleur aan te passen." },
        { IDOK, L"OK" },
        { IDCANCEL, L"Annuleren" }
    };
    for (int i = 0; i < static_cast<int>(ARRAYSIZE(items)); ++i)
    {
        HWND child = GetDlgItem(hwnd, items[i].id);
        if (child) SetWindowTextW(child, items[i].text);
    }
}
'@

$new = @'
bool UseDutchUiLanguage()
{
    LANGID language = GetUserDefaultUILanguage();
    if (PRIMARYLANGID(language) == LANG_DUTCH) return true;
    language = GetSystemDefaultUILanguage();
    return PRIMARYLANGID(language) == LANG_DUTCH;
}

const wchar_t* ColorsTitle()
{
    return UseDutchUiLanguage() ? L"Kleuren" : L"Colors";
}

const wchar_t* ColorsSubtitle()
{
    return UseDutchUiLanguage()
        ? L"Pas de weergavekleuren van gedecodeerde berichten aan."
        : L"Customize the display colors used for decoded messages.";
}

void SetColorsDialogText(HWND hwnd)
{
    const bool dutch = UseDutchUiLanguage();
    SetWindowTextW(hwnd, ColorsTitle());
    struct ItemText { int id; const wchar_t* nl; const wchar_t* en; };
    const ItemText items[] = {
        { IDC_COLORBACKGND, L"Achtergrond", L"Background" },
        { IDC_COLORCAPCODE, L"Capcode", L"Address" },
        { IDC_COLORFLEXPHASE, L"FLEX-fase", L"Phase/Function" },
        { IDC_COLORTIMESTAMP, L"Tijd/datum", L"Time/Date" },
        { IDC_COLORBITERRORS, L"Bitfouten", L"Bit Errors" },
        { IDC_COLORNUMERIC, L"Numeriek/toon", L"Numeric/Tone" },
        { IDC_COLORALPHANUM, L"Bericht", L"Message" },
        { IDC_COLORFLEXBIN, L"FLEX binair", L"FLEX Binary" },
        { IDC_COLORFILTMATCH, L"Filtertreffer", L"Filter Match" },
        { IDC_COLORFILTERLABEL, L"Filterlabel", L"Filter Label" },
        { IDC_COLORDEFAULT, L"Standaardkleuren", L"Default Colors" },
        { IDOK, L"OK", L"OK" },
        { IDCANCEL, L"Annuleren", L"Cancel" }
    };
    for (int i = 0; i < static_cast<int>(ARRAYSIZE(items)); ++i)
    {
        HWND child = GetDlgItem(hwnd, items[i].id);
        if (child) SetWindowTextW(child, dutch ? items[i].nl : items[i].en);
    }
}
'@
Replace-Exact $old $new 'language-aware colors labels'

Replace-Exact @'
    DrawTextW(hdc, L"Kleuren", -1, &title,
'@ @'
    DrawTextW(hdc, ColorsTitle(), -1, &title,
'@ 'localized colors header title'

Replace-Exact @'
    DrawTextW(hdc, L"Pas de weergavekleuren van gedecodeerde berichten aan.", -1,
'@ @'
    DrawTextW(hdc, ColorsSubtitle(), -1,
'@ 'localized colors subtitle'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($path, $text, $utf8NoBom)

git diff --check
if ($LASTEXITCODE -ne 0) { throw 'git diff --check failed' }

git config user.name 'github-actions[bot]'
git config user.email '41898282+github-actions[bot]@users.noreply.github.com'
git rm .github/workflows/colors-locale-aware-once.yml .github/scripts/colors-locale-aware-once.ps1
git add $path
git diff --cached --check
if ($LASTEXITCODE -ne 0) { throw 'cached diff check failed' }
git commit -m 'Make Colors localization language-aware'
git push origin HEAD:modernization/windows11-ui
