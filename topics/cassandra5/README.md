# Cassandra 5.0 Support

This document describes how to build and run integration tests against Apache Cassandra 5.0+.

## Prerequisites

### 1. CCM (Cassandra Cluster Manager)

**IMPORTANT**: The version of CCM published on PyPI is outdated and does not properly support Cassandra 5.0. You must install CCM from the GitHub repository.

See: https://github.com/apache/cassandra-ccm

### 2. Java Requirements

Cassandra 5.0 requires Java 11 or higher (Java 17 recommended). The integration test framework supports running different Cassandra versions with different Java versions:

- **Cassandra 3.x and 4.x**: Java 8
- **Cassandra 5.0+**: Java 11 or Java 17 (recommended)

#### Linux Setup

Install Java 17:
```bash
# Ubuntu/Debian
sudo apt-get install openjdk-17-jdk

# RHEL/CentOS/Fedora
sudo dnf install java-17-openjdk-devel
```

Verify installation:
```bash
ls -la /usr/lib/jvm/java-17-openjdk-amd64
```

#### Windows Setup

1. Download and install Java 17 from [Oracle](https://www.oracle.com/java/technologies/downloads/#java17) or [OpenJDK](https://adoptium.net/temurin/releases/?version=17)
2. Default installation path is typically: `C:\Program Files\Java\jdk-17`

### 3. Environment Variable Configuration

The integration test framework uses the `JAVA17_HOME` environment variable to locate Java 17 when running tests against Cassandra 5.0+. This environment variable:

- Is **ONLY** required when testing Cassandra 5.0+
- Does **NOT** affect your system's global Java configuration
- Is **ONLY** passed to the CCM subprocess, not applied globally

#### Linux
```bash
export JAVA17_HOME=/usr/lib/jvm/java-17-openjdk-amd64
```

#### Windows
```cmd
set JAVA17_HOME=C:\Program Files\Java\jdk-17
```

## How It Works

The integration test framework uses CCM (Cassandra Cluster Manager) to automatically download, configure, and run Cassandra clusters for testing. When testing against Cassandra 5.0+, the framework:

1. Detects the Cassandra version from the `--version` parameter
2. If version >= 5.0.0, reads the `JAVA17_HOME` environment variable
3. Modifies **only the CCM subprocess environment** to use Java 17 by setting:
   - `JAVA_HOME` to point to Java 17
   - `PATH` to include Java 17's bin directory
4. Launches CCM with this modified environment
5. Your system's global Java configuration remains unchanged

This approach allows testing different Cassandra versions with their required Java versions without modifying your system configuration.

## Running Cassandra 5.0 Integration Tests

### Important Notes

1. **Use Full Version Numbers**: You must specify the complete Cassandra version (e.g., `5.0.5`). Major version only (e.g., `5.0`) will not work.

2. **JAVA17_HOME Required**: The `JAVA17_HOME` environment variable must be set before running tests against Cassandra 5.0+.

3. **Automatic Java Selection**: The test framework automatically uses Java 17 for Cassandra 5.0+ and the system's default Java for older versions.

### Linux Example

```bash
# Set Java 17 location
export JAVA17_HOME=/usr/lib/jvm/java-17-openjdk-amd64

# Run all tests against Cassandra 5.0.5
./build/cassandra-integration-tests --version=5.0.5

# Run specific tests
./build/cassandra-integration-tests --version=5.0.5 --gtest_filter="*Cassandra5*"

# Run vector-specific tests (once implemented)
./build/cassandra-integration-tests --version=5.0.5 --gtest_filter="*Vector*"
```

### Windows Example

```cmd
REM Set Java 17 location
set JAVA17_HOME=C:\Program Files\Java\jdk-17

REM Run all tests against Cassandra 5.0.5
build\cassandra-integration-tests.exe --version=5.0.5

REM Run specific tests
build\cassandra-integration-tests.exe --version=5.0.5 --gtest_filter="*Cassandra5*"
```

## Backward Compatibility

The integration test framework maintains full backward compatibility with older Cassandra versions. Tests against Cassandra 3.x and 4.x continue to work as before without any changes:

```bash
# These work without JAVA17_HOME (uses system Java 8)
./build/cassandra-integration-tests --version=3.11.6
./build/cassandra-integration-tests --version=4.1.0
```

## Vector Data Type Support

Cassandra 5.0 introduces vector data type support for vector search and AI/ML workloads. The C++ driver fully supports vector operations with important limitations documented below.

### Supported Vector Element Types

The following types are fully supported:
- Numeric types: `boolean`, `tinyint`, `smallint`, `int`, `bigint`, `float`, `double`
- String types: `text`, `varchar`, `ascii`
- UUID types: `uuid`, `timeuuid`
- Temporal types: `timestamp`, `date`, `time`
- Binary types: `blob`
- Arbitrary precision: `decimal`, `varint`

### Unsupported Vector Element Types

The following types are NOT supported due to Cassandra 5.0 limitations. The driver blocks these to prevent runtime errors:

- `inet` - Causes deserialization failures
- `duration` - Causes deserialization failures
- `list<T>`, `set<T>`, `map<K,V>` - Collection types not properly supported
- `tuple<...>` - Tuples not supported
- `vector<T,N>` - Nested vectors not supported
- User-defined types - Not supported as vector elements

### Validation Methodology

These limitations were confirmed through:
1. Direct testing with Cassandra 5.0.5 using Docker/Podman containers and cqlsh
2. Cross-validation with the Python driver bundled with Cassandra
3. Analysis of the protocol v4 custom type encoding for vectors

The unsupported types either fail to deserialize correctly or cause server errors, even when using cqlsh directly. The driver proactively blocks these types to provide clear error messages rather than runtime failures.

### References

- [Apache Cassandra 5.0 Vector Documentation](https://cassandra.apache.org/doc/5.0/cassandra/vector-search/overview.html)
- [CASSANDRA-18504](https://issues.apache.org/jira/browse/CASSANDRA-18504) - Vector type implementation

## Further Reading

- [Apache Cassandra 5.0 Release Notes](https://cassandra.apache.org/_/blog/Apache-Cassandra-5.0-Release.html)
- [CCM Documentation](https://github.com/apache/cassandra-ccm)
- [Running Integration Tests](../testing/README.md)