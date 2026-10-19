param(
    [Parameter(Mandatory = $true)]
    [string]$Executable
)

$ErrorActionPreference = "Stop"

if (!(Test-Path $Executable)) {
    throw "AtlasDB executable not found: $Executable"
}

$testRoot = Join-Path $env:TEMP ("atlasdb_cli_test_" + [guid]::NewGuid().ToString("N"))
$dbPath = Join-Path $testRoot "database"

function Invoke-AtlasDB {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,

        [Parameter(Mandatory = $true)]
        [int]$ExpectedExitCode
    )

    # Windows PowerShell can turn native stderr into a terminating error
    # when ErrorActionPreference is Stop. Preserve stderr as test output.
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $output = (& $script:Executable @Arguments 2>&1 | Out-String)
        $actualExitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }

    Write-Host $output

    if ($actualExitCode -ne $ExpectedExitCode) {
        throw "AtlasDB exit code $actualExitCode; expected $ExpectedExitCode. Arguments: $($Arguments -join ' ')"
    }

    return $output
}

function Assert-Contains {
    param(
        [string]$Text,
        [string]$Expected,
        [string]$Description
    )

    if (!$Text.Contains($Expected)) {
        throw "$Description failed. Expected output to contain: $Expected"
    }
}

try {
    New-Item -ItemType Directory -Force -Path $testRoot | Out-Null

    Write-Host "`n[1/8] Help command"
    $output = Invoke-AtlasDB -Arguments @("--help") -ExpectedExitCode 0
    Assert-Contains $output "Usage:" "Help command"
    Assert-Contains $output "--help" "Help command"

    Write-Host "`n[2/8] Version command"
    $output = Invoke-AtlasDB -Arguments @("--version") -ExpectedExitCode 0
    Assert-Contains $output "AtlasDB" "Version command"

    Write-Host "`n[3/8] Unknown option returns exit code 2"
    $output = Invoke-AtlasDB -Arguments @("--invalid-option") -ExpectedExitCode 2
    Assert-Contains $output "Usage:" "Unknown option"
    $output = Invoke-AtlasDB -Arguments @("--version") -ExpectedExitCode 0
    Assert-Contains $output "AtlasDB" "Version command"

    Write-Host "`n[4/8] Create table and insert multiple rows"
    $sql = "CREATE TABLE products (id INT64, name STRING, price DOUBLE); INSERT INTO products VALUES (1, 'Keyboard', 49.99); INSERT INTO products VALUES (2, 'Mouse', 19.5);"
    $output = Invoke-AtlasDB -Arguments @("--database", $dbPath, "--sql", $sql) -ExpectedExitCode 0
    Assert-Contains $output "Table created: products" "CREATE TABLE"
    Assert-Contains $output "Rows inserted: 1" "INSERT"

    Write-Host "`n[5/8] Verify persistence after process restart"
    $output = Invoke-AtlasDB -Arguments @("--database", $dbPath, "--sql", "SELECT * FROM products;") -ExpectedExitCode 0
    Assert-Contains $output "Keyboard" "Persisted first product"
    Assert-Contains $output "Mouse" "Persisted second product"

    Write-Host "`n[6/8] Verify a second independent table persists"
    $sql = "CREATE TABLE audit (id INT64, event STRING); INSERT INTO audit VALUES (1, 'startup');"
    $output = Invoke-AtlasDB -Arguments @("--database", $dbPath, "--sql", $sql) -ExpectedExitCode 0
    $output = Invoke-AtlasDB -Arguments @("--database", $dbPath, "--sql", "SELECT * FROM audit;") -ExpectedExitCode 0
    Assert-Contains $output "startup" "Persisted audit table"

    Write-Host "`n[7/8] Verify SQL errors return a nonzero exit code"
    $output = Invoke-AtlasDB -Arguments @("--database", $dbPath, "--sql", "SELECT * FROM missing_table;") -ExpectedExitCode 1
    Assert-Contains $output "SQL error" "Missing table error"

    Write-Host "`n[8/8] Verify legacy demo command still works"
    $output = Invoke-AtlasDB -Arguments @("--sql", "SELECT * FROM users;") -ExpectedExitCode 0
    Assert-Contains $output "Aman" "Legacy users demo"
    Assert-Contains $output "Priya" "Legacy users demo"

    Write-Host "`nALL CLI INTEGRATION CHECKS PASSED" -ForegroundColor Green
}
finally {
    if (Test-Path $testRoot) {
        Remove-Item $testRoot -Recurse -Force
    }
}
