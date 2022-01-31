& (Join-Path $PSScriptRoot ".." ".." ".." "build" "set-env.ps1");
$IsCI = "$Env:TF_BUILD" -ne "" -or "$Env:CI" -eq "true";

$script:allOk = $true;

Push-Location $PSScriptRoot
    # Import ConvertFrom-Toml and ConvertTo-Toml, used for setting versions and
    # crate types. Note that because a workspace file is used, Cargo.toml must
    # exist in this directory before any call to cargo run. We copy over the
    # template unmodified to handle that limitation.
    Copy-Item Cargo.toml.template Cargo.toml

    # We can now import TOML handling implemented in the t2j crate.
    # TODO @cgranade: Consider moving to use objconv instead to decouple
    #                 dependencies here.
    . (Join-Path $PSScriptRoot ".." ".." ".." "build" "t2j" "t2j.ps1");
    Write-Host -ForegroundColor Blue "##[info]Successfully loaded t2j: $(Invoke-T2J --version)"

    # Now that we have prereqs, set the crate version and make sure we're using
    # rlib dependencies for Rust testing.
    $cargoManifest = ConvertFrom-Toml -Path "./Cargo.toml.template";
    $cargoManifest | Format-List | Out-String | Write-Host;
    $cargoManifest.package.version = $Env:NUGET_VERSION;
    $cargoManifest.lib."crate-type" = @("rlib");
    ConvertTo-Toml -InputObject $cargoManifest -Path "./Cargo.toml";

    # If running in CI, use cargo2junit to expose unit tests to the
    # PublishTestResults task.
    if ($IsCI) {
        cargo install cargo2junit
        $testJson = cargo +nightly test -- -Z unstable-options --format json;
        $script:allOk = $script:allOk -and $LASTEXITCODE -eq 0;

        $testJson `
            | cargo2junit `
            <# We use this name to match the *_results.xml pattern that is used
               to find test results in steps-wrap-up.yml. #> `
            | Out-File -FilePath opensim_results.xml -Encoding utf8NoBOM
    } else {
        # Outside of CI, show human-readable output.
        cargo +nightly test
        $script:allOk = $script:allOk -and $LASTEXITCODE -eq 0;
    }

    # Run performance benchmarks as well.
    cargo bench
    $script:allOk = $script:allOk -and $LASTEXITCODE -eq 0;

    # This step isn't required, but we use it to upload run summaries.
    $reportPath = (Join-Path "target" "criterion");
    $perfDest = (Join-Path $Env:DROPS_DIR "perf" "qdk_sim_rs");
    if (Get-Item -ErrorAction SilentlyContinue $reportPath) {
        New-Item -Type Directory -Force -Path $perfDest;
        Copy-Item -Recurse -Force -Path $reportPath -Destination $perfDest;
    }

    # Free disk space by cleaning up.
    # Note that this takes longer, but saves ~1 GB of space, which is
    # exceptionally helpful in CI builds.
    if ($IsCI) {
        cargo clean;
    }
Pop-Location

if (-not $script:allOk) {
    Write-Host "##vso[task.logissue type=error;]Failed to test qdk_sim_rs — please check logs above."
    exit -1;
}
