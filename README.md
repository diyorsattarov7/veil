# Veil

A C++23 application for appearing offline while playing Riot Games titles.

Inspired by [Deceive](https://github.com/molenzwiebel/Deceive) by [molenzwiebel](https://github.com/molenzwiebel).

## Status

Early development. Currently experimental.

- League of Legends — Windows (experimental)
- League of Legends — macOS (in development)
- VALORANT — Windows (planned)

Veil attempts to provide invisible mode for League of Legends by intercepting and rewriting XMPP presence messages.

## Requirements

- Windows 10/11 or macOS
- CMake
- C++23-compatible compiler
- [vcpkg](https://github.com/microsoft/vcpkg) (Windows)
- [Homebrew](https://brew.sh/) (macOS)
- [DuckDNS](https://www.duckdns.org/) hostname
- [spdlog](https://github.com/gabime/spdlog) — C++ logging library
- [GoogleTest](https://github.com/google/googletest) — C++ unit testing framework
- Trusted TLS certificate and private key for your hostname

## Setup

### 1. Install dependencies

**Windows**

Install dependencies using vcpkg:

```powershell
vcpkg install
```

The dependencies are declared in `vcpkg.json`. Configure CMake with your vcpkg toolchain file if it is not already configured through your environment or CMake presets.

**macOS**

Install dependencies using Homebrew:

```bash
brew install cmake ninja boost openssl@3 pugixml spdlog googletest
```

### 2. Configure DuckDNS and TLS certificates

Veil requires a DuckDNS hostname and a trusted TLS certificate to establish local chat connections.

#### 2.1. Register a DuckDNS hostname

1. Visit [DuckDNS](https://www.duckdns.org/) and sign in.
2. Register a subdomain, such as `YOUR_SUBDOMAIN.duckdns.org`.
3. Locate your DuckDNS account token. It will be used for DNS validation.

#### 2.2. Generate a TLS certificate

Choose the instructions for your operating system.

**Windows — Posh-ACME**

Install [Posh-ACME](https://poshac.me/) using PowerShell:

```powershell
Install-Module -Name Posh-ACME -Scope CurrentUser
Import-Module Posh-ACME
```

Select the Let's Encrypt production server:

```powershell
Set-PAServer LE_PROD
```

Request a certificate using the DuckDNS DNS validation plugin:

```powershell
$domain = "YOUR_SUBDOMAIN.duckdns.org"
$token = Read-Host "Enter DuckDNS token" -AsSecureString

$cert = New-PACertificate `
    -Domain $domain `
    -AcceptTOS `
    -Plugin DuckDNS `
    -PluginArgs @{
        DuckToken = $token
    }
```

Replace `YOUR_SUBDOMAIN.duckdns.org` with your registered DuckDNS hostname.

Posh-ACME automatically creates the required DNS TXT record and completes the Let's Encrypt DNS-01 challenge.

Once the certificate has been issued, create the certificate directory and copy the generated files:

```powershell
New-Item -ItemType Directory -Force certs

Copy-Item $cert.FullChainFile "certs\veil-fullchain.pem"
Copy-Item $cert.KeyFile "certs\veil-key.pem"
```

**macOS — Certbot**

Install Certbot using Homebrew:

```bash
brew install certbot
```

Request a certificate using DNS validation:

```bash
sudo certbot certonly --manual \
  --preferred-challenges dns \
  -d YOUR_SUBDOMAIN.duckdns.org
```

Certbot will display a DNS TXT record challenge.

Keep Certbot running and open another terminal window.

Update the DuckDNS TXT record:

```bash
curl -G "https://www.duckdns.org/update" \
  --data-urlencode "domains=YOUR_SUBDOMAIN" \
  --data-urlencode "token=YOUR_DUCKDNS_TOKEN" \
  --data-urlencode "txt=YOUR_DNS_CHALLENGE_VALUE"
```

Replace the placeholders with your DuckDNS subdomain (without `.duckdns.org`), account token, and Certbot's challenge value.

Wait for DNS propagation before continuing in Certbot.

Once validation succeeds, Certbot saves the certificate and private key under its certificate directory, typically:

```text
/etc/letsencrypt/live/YOUR_SUBDOMAIN.duckdns.org/
```

Copy the certificate and private key into Veil's `certs/` directory:

```bash
mkdir -p certs

sudo cp /etc/letsencrypt/live/YOUR_SUBDOMAIN.duckdns.org/fullchain.pem \
  certs/veil-fullchain.pem

sudo cp /etc/letsencrypt/live/YOUR_SUBDOMAIN.duckdns.org/privkey.pem \
  certs/veil-key.pem

sudo chown "$(id -un)" certs/veil-fullchain.pem certs/veil-key.pem
```

#### 2.3. Download Mozilla's CA certificate bundle

Veil also needs a CA certificate bundle to verify upstream TLS connections.

This is separate from the certificate generated for your DuckDNS hostname.

Download the Mozilla CA bundle distributed by curl:

[Mozilla CA certificate bundle](https://curl.se/ca/cacert.pem)

**Windows (PowerShell)**

From the project root:

```powershell
New-Item -ItemType Directory -Force certs

Invoke-WebRequest `
  -Uri "https://curl.se/ca/cacert.pem" `
  -OutFile "certs/cacert.pem"
```

**macOS**

From the project root:

```bash
mkdir -p certs
curl -L https://curl.se/ca/cacert.pem -o certs/cacert.pem
```

#### 2.4. Verify certificate files

Your `certs/` directory should contain:

```text
certs/
├── veil-fullchain.pem
├── veil-key.pem
└── cacert.pem
```

You can inspect the certificate with OpenSSL:

```bash
openssl x509 -in certs/veil-fullchain.pem -noout -subject -dates -ext subjectAltName
```

Verify that the certificate includes your DuckDNS hostname and has not expired.

Certificates must be renewed before expiration. After renewal, update the corresponding files in `certs/`.

### 3. Configure environment variables

Copy `.env.example` to `.env`.

**Windows (PowerShell)**

```powershell
Copy-Item .env.example .env
```

**macOS**

```bash
cp .env.example .env
```

Configure the following values:

```dotenv
VEIL_HOST=YOUR_SUBDOMAIN.duckdns.org

VEIL_CHAT_PORT=5500
VEIL_CONFIG_PORT=5001

VEIL_CERT_FILE=certs/veil-fullchain.pem
VEIL_KEY_FILE=certs/veil-key.pem
VEIL_CA_FILE=certs/cacert.pem
```

Replace `YOUR_SUBDOMAIN.duckdns.org` with your registered DuckDNS hostname.

The hostname must match the TLS certificate.

### 4. Configure local hostname resolution

Veil's chat proxy listens on the local loopback interface. The DuckDNS hostname must therefore resolve to `127.0.0.1` on the computer running Veil.

**Windows**

Open Notepad as Administrator and edit:

```text
C:\Windows\System32\drivers\etc\hosts
```

Add:

```text
127.0.0.1 YOUR_SUBDOMAIN.duckdns.org
```

Save the file.

Optionally, flush the Windows DNS cache:

```powershell
ipconfig /flushdns
```

**macOS**

Open Terminal and edit the hosts file:

```bash
sudo nano /etc/hosts
```

Add:

```text
127.0.0.1 YOUR_SUBDOMAIN.duckdns.org
```

Save the file using `Ctrl+O`, press Enter, then exit with `Ctrl+X`.

Flush the macOS DNS cache:

```bash
sudo dscacheutil -flushcache
sudo killall -HUP mDNSResponder
```

**Verify hostname resolution**

On macOS:

```bash
dscacheutil -q host -a name YOUR_SUBDOMAIN.duckdns.org
```

On Windows:

```powershell
ping -4 YOUR_SUBDOMAIN.duckdns.org
```

The hostname should resolve to `127.0.0.1`.

Note that `nslookup` queries DNS directly and may not reflect hosts-file overrides.

### 5. Build

**Windows**

Configure CMake using your vcpkg toolchain:

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release --parallel
```

**macOS**

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/opt/homebrew;/opt/homebrew/opt/openssl@3"

cmake --build build --parallel
```

The macOS prefix paths above assume a standard Apple Silicon Homebrew installation.

### 6. Run

Run Veil from the project root so relative configuration and certificate paths resolve correctly.

**Windows**

```powershell
.\build\Release\veil.exe
```

The executable location may differ depending on your CMake generator.

**macOS**

```bash
./build/veil
```

Veil loads its configuration, starts a local configuration server, and launches Riot Client with a custom configuration URL.

When Riot Client requests chat configuration, Veil attempts to redirect the chat endpoint through its local TLS proxy.

Example startup output:

```text
[info] [veil] Starting Veil
[info] [riot] Riot Client launch requested
[info] [chat] Proxy listening on port 5500
```

Enable debug logging during development to see additional HTTP, TLS, and XMPP activity.

## Limitations

- League of Legends support is experimental.
- macOS client launching is implemented, but end-to-end invisibility is not yet confirmed.
- VALORANT support is planned.
- DuckDNS, TLS certificates, and hosts-file configuration currently require manual setup.
- Riot Client updates may break compatibility.
- Invisibility is not guaranteed across every Riot service or game activity.

## License

GPL-3.0. See [LICENSE](LICENSE).

Veil is an unofficial project and is not affiliated with Riot Games or the developers of Deceive.
