# PDX Search Tool

PDX Search Tool is a C++ command-line application for inspecting PDX and ODX diagnostic data. It can read a `.pdx` archive, an extracted PDX folder, an `.odx` file, or an `.xml` file and print useful diagnostic information from it.

The tool can generate package summaries, list diagnostic services, show DTC definitions, find DID candidates, search raw ODX/XML text, and look up value-related information such as request, response, and formula matches.

## Requirements

- Linux or another system with a C++17 compiler
- CMake 3.16 or newer
- Make or another CMake-supported build tool
- ZLIB development package
- Git, if you need to initialize the `pugixml` submodule

On Ubuntu or Debian, install the required system packages with:

```bash
sudo apt update
sudo apt install build-essential cmake zlib1g-dev git
```

This project uses `pugixml` from the `external/pugixml` submodule. If the folder is empty after cloning, initialize it with:

```bash
git submodule update --init --recursive
```

## Build

From the project root, run:

```bash
./build.sh
```

The script configures CMake, builds the project in the `build/` directory, and produces the executable here:

```bash
build/pdx-search-tool
```

## Usage

Show help:

```bash
./build/pdx-search-tool --help
```

Print the full report for a PDX file:

```bash
./build/pdx-search-tool --pdx-file path/to/package.pdx
```

Print only the package summary:

```bash
./build/pdx-search-tool --summary --pdx-file path/to/package.pdx
```

List diagnostic services:

```bash
./build/pdx-search-tool --services --pdx-file path/to/package.pdx
```

List DTC definitions:

```bash
./build/pdx-search-tool --dtcs --pdx-file path/to/package.pdx
```

List DID candidates:

```bash
./build/pdx-search-tool --dids --pdx-file path/to/package.pdx
```

Search raw ODX/XML text:

```bash
./build/pdx-search-tool --search "engine" --context 3 --pdx-file path/to/package.pdx
```

Find value information:

```bash
./build/pdx-search-tool --find-value "VehicleSpeed" --pdx-file path/to/package.pdx
```

Run value search interactively:

```bash
./build/pdx-search-tool --find-value "VehicleSpeed" --interactive --pdx-file path/to/package.pdx
```

Write output to a file:

```bash
./build/pdx-search-tool --summary --output report.txt --pdx-file path/to/package.pdx
```

## Options

- `--help`, `-h`: Show the help message.
- `--pdx-file <path>`: Input `.pdx`, extracted folder, `.odx`, or `.xml` file.
- `--output <file>`, `-o <file>`: Write the report to a file.
- `--search <text>`: Search raw ODX/XML text.
- `--find-value <text>`, `--value <text>`: Find value request, response, or formula information. This option can be repeated.
- `--interactive`: Choose one structured value match interactively. Requires `--find-value`.
- `--context <lines>`: Number of context lines for raw text search. Default is `2`.
- `--summary`: Print package summary.
- `--services`: Print diagnostic services.
- `--dtcs`: Print DTC definitions.
- `--dids`: Print DID candidates.

If no report mode is selected, the tool prints the full report.

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
