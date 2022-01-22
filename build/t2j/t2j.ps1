$t2jRoot = Resolve-Path (Join-Path $PSScriptRoot "Cargo.toml");

function Invoke-T2J {
    $input | cargo run --manifest-path $t2jRoot -- @args
}

function ConvertFrom-Toml {
    [CmdletBinding(
        DefaultParameterSetName = "Path"
    )]
    param(
        [Parameter(ParameterSetName = "Path", Position = 0)]
        [string]
        $Path,

        [Parameter(ParameterSetName = "Value", ValueFromPipeline = $true)]
        [string]
        $Value
    )

    begin {
        if ($PSCmdlet.ParameterSetName -eq "Path") {
            Invoke-T2J toml2json $Path - | ConvertFrom-Json -Depth 10
        } elseif ($PSCmdlet.ParameterSetName -eq "Value") {
            $completeDocument = ""
        }
    }

    process {
        if ($PSCmdlet.ParameterSetName -eq "Value") {
            $completeDocument += "$Value`n";
        }
    }

    end {
        if ($PSCmdlet.ParameterSetName -eq "Value") {
            $completeDocument | Invoke-T2J toml2json - - | ConvertFrom-Json -Depth 10 | Write-Output
        }
    }

}

function ConvertTo-Toml {
    param(
        [Parameter(Position = 0)]
        [string]
        $Path = $null,

        [Parameter(ValueFromPipeline = $true)]
        [object]
        $InputObject
    )

    process {
        $json = ConvertTo-Json -Depth 10 -InputObject $InputObject;
        if ("$Path" -eq "") {
            $json | Invoke-T2J json2toml - - | Write-Output
        } else {
            $json | Invoke-T2J json2toml - $Path
        }
    }
}
