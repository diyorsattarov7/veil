# Veil

A C++23 application for appearing offline while playing Riot Games titles.

Inspired by [Deceive](https://github.com/molenzwiebel/Deceive) by [molenzwiebel](https://github.com/molenzwiebel).

## Status

Early development. **Currently only functional on Windows.**

- League of Legends — Windows (experimental)
- League of Legends — macOS (planned)
- VALORANT — Windows (planned)

Veil currently supports invisible mode for League of Legends by intercepting and rewriting XMPP presence messages.

## Requirements

- Windows 10/11
- CMake
- C++23-compatible compiler (MSVC recommended)
- [vcpkg](https://github.com/microsoft/vcpkg)
- [DuckDNS](https://www.duckdns.org/) hostname
- TLS certificate and private key for your hostname

## Setup

### 1. Install dependencies

Install the project's dependencies using [vcpkg](https://github.com/microsoft/vcpkg):

```powershell
vcpkg install
```

vcpkg will automatically install the dependencies declared in `vcpkg.json`.

### 2. Configure DuckDNS

1. Create an account at [DuckDNS](https://www.duckdns.org/).
2. Register a unique subdomain, such as `YOUR_SUBDOMAIN.duckdns.org`.
3. Obtain a trusted TLS certificate for your hostname. A Let's Encrypt certificate using a DNS-01 challenge is one option.
4. Place the certificate and private key in the `certs/` directory.

Expected certificate files:

```text
certs/
├── veil-fullchain.pem
├── veil-key.pem
└── cacert.pem
```

`cacert.pem` is a CA trust bundle used for upstream TLS verification. It is not your DuckDNS certificate.

### 3. Configure environment variables

Veil uses a `.env` file for its local configuration.

Copy the provided `.env.example` file:

```powershell
Copy-Item .env.example .env
```

Open `.env` and configure the following values:

```dotenv
VEIL_HOST=YOUR_SUBDOMAIN.duckdns.org

VEIL_CHAT_PORT=5000
VEIL_CONFIG_PORT=5001

VEIL_CERT_FILE=certs/veil-fullchain.pem
VEIL_KEY_FILE=certs/veil-key.pem
VEIL_CA_FILE=certs/cacert.pem
```

Replace `YOUR_SUBDOMAIN.duckdns.org` with the hostname you registered on DuckDNS.

The hostname must match your TLS certificate.

### 4. Configure the Windows hosts file

Open Notepad as Administrator and edit:

```text
C:\Windows\System32\drivers\etc\hosts
```

Add:

```text
127.0.0.1 YOUR_SUBDOMAIN.duckdns.org
```

Replace `YOUR_SUBDOMAIN.duckdns.org` with the same hostname configured in `.env`.

This makes Riot Client connect to Veil locally rather than connecting to the hostname's public IP address.

### 5. Build

```powershell
cmake -S . -B build
cmake --build build
```

### 6. Run

From the project root:

```powershell
.\build\veil.exe
```

Veil loads the `.env` configuration, starts a local configuration proxy and TLS chat proxy, then launches Riot Client using the modified configuration.

When working correctly, you should see messages similar to:

```text
[TLS] Certificate loaded
[CHAT] Proxy listening on 5000
[CHAT] Client connected
[CHAT] Client TLS established
[CHAT] Upstream TLS established
[XMPP] Presence rewritten: offline
```

## Limitations

- Windows only for now.
- League of Legends is the only tested game.
- Invisible mode is experimental.
- DuckDNS, TLS certificates, and hosts-file configuration must currently be set up manually.
- XMPP parsing and error handling are still under development.
- Riot Client updates may break compatibility.
- Invisibility is not guaranteed across every Riot service or game activity.

## License

GPL-3.0. See [LICENSE](LICENSE).

Veil is an unofficial project and is not affiliated with Riot Games or the developers of Deceive.