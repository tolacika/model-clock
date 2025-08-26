param (
    [Parameter(Mandatory=$true, ValueFromRemainingArguments=$true)]
    [string[]]$FilePaths
)

# Output string builder
$Output = ""

foreach ($File in $FilePaths) {
    if (Test-Path $File) {
        $Output += "//// ----`n"
        $Output += "//// Content of '$File'`n"
        $Output += "//// ----`n`n"
        $Output += (Get-Content -Raw -Path $File) + "`n`n"
    }
    else {
        $Output += "//// ----`n"
        $Output += "//// File '$File' not found`n"
        $Output += "//// ----`n`n"
    }
}

# Copy to clipboard
$Output | Set-Clipboard

# Print status
Write-Host "$($FilePaths.Count) file(s) processed and copied to clipboard." -ForegroundColor Green
