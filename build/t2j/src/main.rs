// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

use std::{
    fs,
    io::{self, Read, Write},
    path::PathBuf,
};

use clap::{Parser, Subcommand};
use thiserror::Error;

#[derive(Error, Debug)]
pub enum T2JError {
    #[error("I/O error")]
    IO(#[from] io::Error),

    #[error(transparent)]
    TomlDeserialization(#[from] toml::de::Error),

    #[error(transparent)]
    TomlSerialization(#[from] toml::ser::Error),

    #[error("JSON error")]
    JsonError(#[from] serde_json::Error),

    #[error("Number `{value}` is not a u64, i64, or f64 value, and cannot be serialized to TOML.")]
    UnexpectedNumber { value: serde_json::Number },
}

struct TryFromWrapper<'a>(&'a serde_json::Value);
impl<'a> Into<TryFromWrapper<'a>> for &'a serde_json::Value {
    fn into(self) -> TryFromWrapper<'a> {
        TryFromWrapper(self)
    }
}
impl<'a> TryFrom<TryFromWrapper<'a>> for toml::Value {
    type Error = T2JError; // FIXME: Refine

    fn try_from(value: TryFromWrapper) -> Result<Self, Self::Error> {
        let TryFromWrapper(value) = value;
        Ok(match value {
            // JSON null serializes to an empty table in TOML.
            serde_json::Value::Null => toml::Value::Table(toml::map::Map::new()),
            serde_json::Value::Bool(bool) => toml::Value::Boolean(*bool),
            // FIXME: Break down JSON numbers into either float or int.
            // FIXME: Make unwrap an Error instead.
            serde_json::Value::Number(number) => {
                if number.is_i64() {
                    toml::Value::Integer(number.as_i64().unwrap())
                } else if number.is_i64() {
                    toml::Value::Integer(number.as_i64().unwrap())
                } else if number.is_f64() {
                    toml::Value::Float(number.as_f64().unwrap())
                } else {
                    return Err(T2JError::UnexpectedNumber {
                        value: number.to_owned(),
                    });
                }
            }
            serde_json::Value::String(str) => toml::Value::String(str.to_string()),
            serde_json::Value::Array(array) => toml::Value::Array(
                array
                    .iter()
                    .map(|element| {
                        let element = TryFromWrapper(element);
                        let toml_value: toml::Value = TryFrom::try_from(element).unwrap();
                        toml_value
                    })
                    .collect(),
            ),
            serde_json::Value::Object(map) => toml::Value::Table({
                let mut toml_map = toml::map::Map::<String, toml::Value>::new();
                for (key, element) in map.iter() {
                    let key = key.to_string();
                    let element = TryFromWrapper(element);
                    let toml_value: toml::Value = TryFrom::try_from(element).unwrap();
                    toml_map.insert(key, toml_value);
                }
                toml_map
            }),
        })
    }
}

#[derive(Parser, Debug)]
#[clap(author, version, about, long_about = None)]
struct Args {
    #[clap(subcommand)]
    action: Action,
}

#[derive(Subcommand, Debug)]
enum Action {
    #[clap(name = "toml2json")]
    Toml2Json { from: PathBuf, to: PathBuf },
    #[clap(name = "json2toml")]
    Json2Toml { from: PathBuf, to: PathBuf },
}

fn read(from: PathBuf) -> Result<String, io::Error> {
    if from.to_str().map_or(false, |p| p == "-") {
        let stdin = io::stdin();
        let mut stdin = stdin.lock();
        let mut buf = String::new();
        stdin.read_to_string(&mut buf)?;
        Ok(buf)
    } else {
        fs::read_to_string(from)
    }
}

fn write<T: AsRef<[u8]>>(to: PathBuf, data: T) -> Result<(), io::Error> {
    if to.to_str().map_or(false, |p| p == "-") {
        let mut stdout = io::stdout();
        stdout.write_all(data.as_ref())
    } else {
        fs::write(to, data)
    }
}

fn toml_to_json(from: PathBuf, to: PathBuf) -> Result<(), T2JError> {
    let source = read(from)?;
    let value = source.parse::<toml::Value>()?;
    let output = serde_json::to_string(&value)?;
    write(to, output)?;
    Ok(())
}

fn json_to_toml(from: PathBuf, to: PathBuf) -> Result<(), T2JError> {
    let source = read(from)?;
    let value = source.parse::<serde_json::Value>()?;
    let value: toml::Value = TryFrom::try_from(TryFromWrapper(&value))?;
    let output = toml::to_string(&value)?;
    write(to, output)?;
    Ok(())
}

fn main() -> Result<(), T2JError> {
    let args = Args::parse();
    match args.action {
        Action::Toml2Json { from, to } => toml_to_json(from, to),
        Action::Json2Toml { from, to } => json_to_toml(from, to),
    }
}
