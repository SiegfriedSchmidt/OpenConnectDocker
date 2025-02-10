# === Build Stage ===
FROM debian:bullseye-slim AS build
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies per the official README
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    pkg-config \
    autoconf \
    automake \
    libtool \
    git \
    libgnutls28-dev \
    libev-dev \
    ipcalc-ng \
 && rm -rf /var/lib/apt/lists/*

# Copy the entire forked repository into the build container.
WORKDIR /src
COPY . .

# Regenerate build scripts and compile (this follows the instructions in the official README)
RUN autoreconf -fvi && \
    ./configure && \
    make && \
    make install DESTDIR=/install

# === Final Stage ===
FROM debian:bullseye-slim
ENV DEBIAN_FRONTEND=noninteractive

# Install runtime libraries needed by ocserv.
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

# Copy the compiled files from the build stage.
COPY --from=build /install/ /

# Run ocserv in the foreground using the configuration file from /etc/ocserv/ocserv.conf.
CMD ["/usr/local/sbin/ocserv", "--foreground", "--config", "/etc/ocserv/ocserv.conf"]
