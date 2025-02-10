# === Build Stage ===
FROM debian:bullseye-slim AS build
# Prevent interactive prompts during apt installs.
ENV DEBIAN_FRONTEND=noninteractive

# Install basic build dependencies per the official README for Debian/Ubuntu.
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    pkg-config \
    autoconf \
    automake \
    libtool \
    libgnutls28-dev \
    libev-dev \
    # (Optionally, add more packages from the official README if you need extra functionality)
    && rm -rf /var/lib/apt/lists/*

# Copy the entire repository (your fork) into the container.
WORKDIR /src
COPY . .

# If building from a Git checkout
RUN autoreconf -fvi

# Configure, compile, and install ocserv into a temporary install directory.
RUN ./configure && make && make install DESTDIR=/install

# === Final Stage ===
FROM debian:bullseye-slim
ENV DEBIAN_FRONTEND=noninteractive

# Install the minimal runtime libraries.
RUN apt-get update && apt-get install -y --no-install-recommends \
    gnutls-bin \
    libev4 \
    libnl-3-200 \
    libpam0g \
    liblz4-1 \
    libseccomp2 \
    libreadline8 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Copy ocserv files from the build stage.
COPY --from=build /install/ /

# Run ocserv in the foreground. It will load its configuration from /etc/ocserv/ocserv.conf.
CMD ["/usr/local/sbin/ocserv", "--foreground", "--config", "/etc/ocserv/ocserv.conf"]
