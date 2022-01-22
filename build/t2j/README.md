# t2j

Small utility for editing TOML at the command line by converting to and from JSON.

```
# Convert a TOML file to JSON.
cargo run -- toml2json Cargo.toml Cargo.json
# Read JSON from stdin and write to stdout.
cargo run -- json2toml - -
```
